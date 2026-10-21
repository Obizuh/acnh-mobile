// 客体内存写保护脏页跟踪：预排布贴图（CPU 写、GPU 读）的池页上传后设只读，游戏一写即缺页 → 标脏、解保护；
// 使用前查覆盖页的写代号，比上传时新就重传。NVN 语义：贴图描述符指向池内存，GPU 每次读的是当前内容
// （实测游戏会把流式素材写进仍被采样的贴图存储，不重新登记）。
// 信号处理约束：缺页路径只用原子操作与 mprotect，不分配、不加锁。
// Write-protect dirty-page tracking for guest memory: pool pages of packaged textures (CPU writes, GPU reads) are set read-only after upload, and as soon as the game writes it faults -> mark dirty, unprotect;
// before use, check the write generation of the covered pages and re-upload if newer than at upload time. NVN semantics: a texture descriptor points to pool memory, and the GPU reads the current content every time
// (measured: the game writes streamed assets into texture storage that is still being sampled, without re-registering).
// Signal handler constraint: the page-fault path uses only atomic operations and mprotect, no allocation, no locks.
#include "nvn_dirty.h"
#include "../../host/dm.h"
#include <stdatomic.h>
#include <pthread.h>
#include <sys/mman.h>
#include <unistd.h>
#include <mach/mach_time.h>
#include <sched.h>

// 页状态与 mprotect 的次序（改保护的一方先把状态改成 BUSY，mprotect 落地后才改成终态；别的线程见 BUSY 就等）：
//   设只读：NONE → BUSY → mprotect(只读) → PROT；解保护：PROT → BUSY → mprotect(可写) → NONE。
// 不变式：状态是 NONE 的页一定可写、是 PROT 的页一定只读。原先设只读是「先记 PROT、攒一段再 mprotect」，这段时间里另一线程的写前解保护
// 会把状态改回 NONE，随后攒批的 mprotect 落地 ⇒ 页只读而状态 NONE：游戏线程写它时缺页处理只回「重执行」永远空转；
// 宿主用系统调用往里读文件时 prep_write 看状态 NONE 跳过，系统调用不走缺页处理，直接 EFAULT、数据读不全。
// BUSY 最长持续 = 一次 mprotect 的时间（设只读时再加上给这一段逐页改状态的几次原子写）。等待有上限 BUSY_WAIT_MS，超限报红：
// 缺页处理里超限按「不是我们的页」返回，交给崩溃处理如实报崩，不静默空转。等待只读原子量、不拿任何锁。
// Order of page state and mprotect (the side changing protection first sets the state to BUSY and sets the final state only after mprotect has landed; other threads wait when they see BUSY):
//   set read-only: NONE -> BUSY -> mprotect(read-only) -> PROT; unprotect: PROT -> BUSY -> mprotect(writable) -> NONE.
// Invariant: a page whose state is NONE is always writable, and one whose state is PROT is always read-only. Setting read-only used to be "record PROT first, batch up a run, then mprotect"; during that time another thread's unprotect-before-write
// could set the state back to NONE, and then the batched mprotect landed => the page read-only with state NONE: when the game thread wrote to it the page-fault handler only returned "re-execute" and spun forever;
// when the host read a file into it with a system call, prep_write saw state NONE and skipped, the system call did not go through the page-fault handler and failed with EFAULT, leaving the data incomplete.
// BUSY lasts at most one mprotect (plus, when setting read-only, the few atomic writes that change the state of the pages in the run). The wait has an upper limit BUSY_WAIT_MS and reports red beyond it:
// in the page-fault handler, exceeding it returns "not our page" and lets the crash handler report the crash truthfully instead of spinning silently. The wait only reads atomics and takes no lock.
enum { PG_NONE = 0, PG_PROT = 1, PG_BUSY = 2 };
enum { BUSY_WAIT_MS = 2000 };
// 跟踪区间 = 各内存池（MemoryPoolInitialize 登记）。池销毁后复用 live=0 的槽，信号处理里无锁扫描。
// Tracked ranges = the memory pools (registered by MemoryPoolInitialize). After a pool is destroyed its live=0 slot is reused; the signal handler scans without locks.
#define MAX_RANGE 4096   // 原为 256：内存池多于它时超出的池不跟踪，里面的贴图每次取用都当成脏的重传（暗黑实测峰值见内存读数行的「内存池」；塞尔达约 300 个，踩过）。扫描只到已用的槽数，容量大不增加开销 | was 256: with more memory pools than that the excess pools are untracked and their textures are treated as dirty and re-uploaded on every use (for Diablo's measured peak see "memory pools" in the memory readout line; Zelda has about 300 and hit this). The scan only covers slots in use, so a larger capacity costs nothing
// Protection belongs to a host page, not to a guest pool. Several 4K-aligned
// guest pools can share one 16K page. Stable page records survive pool slot reuse;
// signal handlers never follow a recycled range's arrays.
typedef struct { _Atomic uintptr_t key; _Atomic uint64_t gen; _Atomic uint32_t refs; _Atomic uint8_t state; } page_t;
#define PAGE_SLOTS (1u << 20)
static page_t *g_pages;
typedef struct { uintptr_t base, end, mem, limit; _Atomic int live; } range_t;
static range_t g_r[MAX_RANGE];
static _Atomic int g_nr;
static uintptr_t g_pg, g_shift;
static _Atomic uint64_t g_now = 1;
static _Atomic uint64_t g_faults, g_preps;
static _Atomic uint64_t g_adds_total;
static _Atomic int g_live, g_live_peak;   // 在册的池数与峰值（内存读数行用） | number of registered pools and peak (for the memory readout line)
void nvn_dirty_stat(int *live, int *peak, int *cap) { *live = atomic_load(&g_live); *peak = atomic_load(&g_live_peak); *cap = MAX_RANGE; }
static _Atomic int g_overflow_logged;
static pthread_mutex_t g_slot_mu = PTHREAD_MUTEX_INITIALIZER;   // 建池 / 销池（客体线程，可能并发）互斥选槽；缺页处理路径不碰它 | Pool creation / destruction (guest threads, possibly concurrent) pick slots under a mutex; the page-fault handling path does not touch it
static _Atomic uint64_t g_mp_calls, g_mp_pages, g_mp_ticks;   // 本模块的 mprotect：调用次数、覆盖页数、耗时（mach 时基，缺页路径里也安全） | This module's mprotect: call count, pages covered, time spent (mach timebase, safe in the page-fault path too)
static void mp(uintptr_t a, size_t len, int prot) {
    uint64_t t = mach_absolute_time(); mprotect((void *)a, len, prot);
    atomic_fetch_add(&g_mp_ticks, mach_absolute_time() - t); atomic_fetch_add(&g_mp_calls, 1); atomic_fetch_add(&g_mp_pages, len >> g_shift);
}

static mach_timebase_info_data_t g_tb;   // 首次登记池时取好（缺页路径里只做乘除） | Obtained when a pool is first registered (the fault path only multiplies and divides)
static _Atomic int g_busy_red;
// 等这一页离开 BUSY。返回 1 = 已离开；0 = 等过了上限（报红一次）。信号处理里可用：只有原子读、mach_absolute_time、sched_yield
// Wait for this page to leave BUSY. Returns 1 = it has left; 0 = waited past the limit (reported red once). Usable in a signal handler: only atomic reads, mach_absolute_time and sched_yield
static page_t *page_find(uintptr_t addr, int create) {
    uintptr_t base = addr & ~(g_pg - 1), key = base + 1;
    size_t h = ((base >> g_shift) * 11400714819323198485ull) >> 44;
    for (size_t n = 0; n < PAGE_SLOTS; n++) {
        page_t *p = &g_pages[(h + n) & (PAGE_SLOTS - 1)];
        uintptr_t old = atomic_load_explicit(&p->key, memory_order_acquire);
        if (old == key) return p;
        if (!old) {
            if (!create) return NULL;
            // Only range registration creates records, under g_slot_mu.
            atomic_store_explicit(&p->key, key, memory_order_release);
            return p;
        }
    }
    if (create) die("dirty host-page table exhausted");
    return NULL;
}
static int busy_wait(page_t *p) {
    uint64_t t0 = 0;
    for (unsigned n = 1; atomic_load(&p->state) == PG_BUSY; n++) {
        if (n & 0x3ff) continue;
        uint64_t now = mach_absolute_time(); if (!t0) t0 = now;
        if ((now - t0) * g_tb.numer / g_tb.denom > (uint64_t)BUSY_WAIT_MS * 1000000ull) {
            if (!atomic_exchange(&g_busy_red, 1)) LOG("dirty page %p busy for %d ms", (void *)(atomic_load(&p->key) - 1), BUSY_WAIT_MS);
            return 0;
        }
        sched_yield();
    }
    return 1;
}
// Regular-thread callers hold g_slot_mu; signal handlers use page_find only.
static range_t *find(uintptr_t a) {
    int n = atomic_load(&g_nr);
    for (int i = 0; i < n; i++) if (atomic_load(&g_r[i].live) && a >= g_r[i].mem && a < g_r[i].limit) return &g_r[i];
    return NULL;
}
static void unprotect_page(page_t *p) {
    mp(atomic_load(&p->key) - 1, g_pg, PROT_READ | PROT_WRITE);
    atomic_store(&p->gen, atomic_fetch_add(&g_now, 1) + 1);
    atomic_store(&p->state, PG_NONE);
}
static int on_fault(void *addr, int is_write, void *uc) {
    (void)uc;
    page_t *p = is_write ? page_find((uintptr_t)addr, 0) : NULL;
    if (!p || !atomic_load(&p->refs)) return 0;
    uint8_t st = PG_PROT;
    if (atomic_compare_exchange_strong(&p->state, &st, PG_BUSY)) { unprotect_page(p); atomic_fetch_add(&g_faults, 1); return 1; }
    if (st == PG_BUSY) return busy_wait(p);
    return atomic_load(&p->gen) != 0;
}
static void prep_write(void *addr, size_t len) {
    if (!len || !g_pages) return;
    uintptr_t a = (uintptr_t)addr & ~(g_pg - 1), end = (uintptr_t)addr + len;
    for (; a < end; a += g_pg) {
        page_t *p = page_find(a, 0);
        if (!p || !atomic_load(&p->refs)) continue;
        for (;;) {
            uint8_t st = PG_PROT;
            if (atomic_compare_exchange_strong(&p->state, &st, PG_BUSY)) { unprotect_page(p); atomic_fetch_add(&g_preps, 1); break; }
            if (st != PG_BUSY || !busy_wait(p)) break;
        }
    }
}
static pthread_once_t g_init_once = PTHREAD_ONCE_INIT;
static void dirty_init(void) {
    g_pg = (uintptr_t)getpagesize(); g_shift = __builtin_ctzl(g_pg);
    mach_timebase_info(&g_tb);
    g_pages = calloc(PAGE_SLOTS, sizeof *g_pages);
    if (!g_pages) die("dirty host-page table allocation failed");
    dm_fault_hook_register(on_fault); dm_guest_write_prep_register(prep_write);
}
void nvn_dirty_add_range(const void *mem, size_t size) {
    if (!mem || !size) return;
    pthread_once(&g_init_once, dirty_init);
    atomic_fetch_add(&g_adds_total, 1);
    pthread_mutex_lock(&g_slot_mu);
    int n = atomic_load(&g_nr), slot = -1;
    for (int i = 0; i < n; i++) if (!atomic_load(&g_r[i].live)) { slot = i; break; }
    if (slot < 0) {
        if (n >= MAX_RANGE) {
            if (!atomic_exchange(&g_overflow_logged, 1)) LOG("dirty range table full (%d), pool %p untracked", MAX_RANGE, mem);
            pthread_mutex_unlock(&g_slot_mu); return;
        }
        slot = n;
    }
    range_t *r = &g_r[slot];
    r->mem = (uintptr_t)mem; r->limit = (uintptr_t)mem + size;
    r->base = r->mem & ~(g_pg - 1); r->end = (r->limit + g_pg - 1) & ~(g_pg - 1);
    for (uintptr_t a = r->base; a < r->end; a += g_pg) atomic_fetch_add(&page_find(a, 1)->refs, 1);
    int lv = atomic_fetch_add(&g_live, 1) + 1, pk = atomic_load(&g_live_peak);
    while (lv > pk && !atomic_compare_exchange_weak(&g_live_peak, &pk, lv)) {}
    if (slot == n) atomic_store(&g_nr, n + 1);
    atomic_store_explicit(&r->live, 1, memory_order_release);
    pthread_mutex_unlock(&g_slot_mu);
}
void nvn_dirty_remove_range(const void *mem) {
    pthread_mutex_lock(&g_slot_mu);
    // Match the original pool address, not the rounded host-page address.
    // An adjacent pool may start on the same host page.
    int n = atomic_load(&g_nr);
    for (int i = 0; i < n; i++) {
        range_t *r = &g_r[i];
        if (!atomic_load(&r->live) || r->mem != (uintptr_t)mem) continue;
        prep_write((void *)r->base, r->end - r->base);
        for (uintptr_t a = r->base; a < r->end; a += g_pg) atomic_fetch_sub(&page_find(a, 0)->refs, 1);
        atomic_store_explicit(&r->live, 0, memory_order_release);
        atomic_fetch_sub(&g_live, 1);
        break;
    }
    pthread_mutex_unlock(&g_slot_mu);
}
int nvn_dirty_tracked(const void *addr, size_t len) {
    pthread_mutex_lock(&g_slot_mu);
    range_t *r = find((uintptr_t)addr);
    int tracked = r && len <= r->limit - (uintptr_t)addr;
    pthread_mutex_unlock(&g_slot_mu);
    return tracked;
}
void nvn_dirty_protect(const void *addr, size_t len) {
    if (!len) return;
    pthread_mutex_lock(&g_slot_mu);
    range_t *r = find((uintptr_t)addr);
    if (!r || len > r->limit - (uintptr_t)addr) { pthread_mutex_unlock(&g_slot_mu); return; }
    uintptr_t a = (uintptr_t)addr & ~(g_pg - 1), end = ((uintptr_t)addr + len + g_pg - 1) & ~(g_pg - 1);
    uintptr_t run = 0; int open = 0;
    for (uintptr_t i = a; i <= end; i += g_pg) {
        uint8_t st = PG_NONE;
        page_t *p = i < end ? page_find(i, 0) : NULL;
        int took = p && atomic_compare_exchange_strong(&p->state, &st, PG_BUSY);
        if (took && !open) { run = i; open = 1; }
        if (!took && open) {
            mp(run, i - run, PROT_READ);
            for (uintptr_t k = run; k < i; k += g_pg) atomic_store(&page_find(k, 0)->state, PG_PROT);
            open = 0;
        }
    }
    pthread_mutex_unlock(&g_slot_mu);
}
uint64_t nvn_dirty_now(void) { return atomic_load(&g_now); }
int nvn_dirty_since(const void *addr, size_t len, uint64_t stamp) {
    if (!len) return 0;
    pthread_mutex_lock(&g_slot_mu);
    range_t *r = find((uintptr_t)addr);
    int dirty = !r || len > r->limit - (uintptr_t)addr;
    if (!dirty) {
        uintptr_t a = (uintptr_t)addr & ~(g_pg - 1), end = (uintptr_t)addr + len;
        for (; a < end; a += g_pg) {
            page_t *p = page_find(a, 0);
            if (!p || atomic_load(&p->gen) > stamp) { dirty = 1; break; }
        }
    }
    pthread_mutex_unlock(&g_slot_mu);
    return dirty;
}
int nvn_dirty_clean(const void *addr, size_t len, uint64_t stamp, uint64_t seen, uint64_t now) {
    pthread_mutex_lock(&g_slot_mu);
    range_t *r = find((uintptr_t)addr);
    int clean = r && len <= r->limit - (uintptr_t)addr;
    if (clean && now != seen) {
        uintptr_t a = (uintptr_t)addr & ~(g_pg - 1), end = (uintptr_t)addr + len;
        for (; a < end; a += g_pg) {
            page_t *p = page_find(a, 0);
            if (!p || atomic_load(&p->gen) > stamp) { clean = 0; break; }
        }
    }
    pthread_mutex_unlock(&g_slot_mu);
    return clean;
}
void nvn_dirty_slot_stats(int *live, int *used_slots, uint64_t *adds_total) {
    int n = atomic_load(&g_nr), active = 0;
    for (int i = 0; i < n; i++) active += atomic_load_explicit(&g_r[i].live, memory_order_acquire) != 0;
    *live = active; *used_slots = n; *adds_total = atomic_load(&g_adds_total);
}
void nvn_dirty_stats(uint64_t *faults, uint64_t *preps) { *faults = atomic_load(&g_faults); *preps = atomic_load(&g_preps); }
void nvn_dirty_mp_stats(uint64_t *calls, uint64_t *pages, uint64_t *ns) {
    static mach_timebase_info_data_t tb; if (!tb.denom) mach_timebase_info(&tb);
    *calls = atomic_load(&g_mp_calls); *pages = atomic_load(&g_mp_pages); *ns = atomic_load(&g_mp_ticks) * tb.numer / tb.denom;
}
