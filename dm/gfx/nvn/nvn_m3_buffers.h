#ifndef NVN_M3_BUFFERS_H
#define NVN_M3_BUFFERS_H
// Grow with the island's live buffers. A fixed 512-entry table silently
// dropped resources in developed islands, leaving stale bindings or skipped draws.
// Registration is idempotent because residency can be requested more than once.
typedef struct { uint64_t gpu, len; __unsafe_unretained id<MTLBuffer> buf; } m3_buf_entry;
static m3_buf_entry *g_m3_bufs; static int g_m3_nbufs, g_m3_bufcap;
static os_unfair_lock g_m3_lk = OS_UNFAIR_LOCK_INIT;
static void m3_buf_add(id<MTLBuffer> b) {
    os_unfair_lock_lock(&g_m3_lk);
    for (int i = 0; i < g_m3_nbufs; i++) if (g_m3_bufs[i].buf == b) {
        g_m3_bufs[i].gpu = b.gpuAddress; g_m3_bufs[i].len = b.length;
        os_unfair_lock_unlock(&g_m3_lk); return;
    }
    if (g_m3_nbufs == g_m3_bufcap) {
        int cap = g_m3_bufcap ? g_m3_bufcap * 2 : 512;
        m3_buf_entry *next = realloc(g_m3_bufs, (size_t)cap * sizeof *next);
        if (!next) MDIE("Metal 3 buffer registry allocation failed");
        g_m3_bufs = next; g_m3_bufcap = cap;
        if (cap > 512) MLOG("Metal 3 buffer registry capacity %d (live %d)", cap, g_m3_nbufs);
    }
    g_m3_bufs[g_m3_nbufs++] = (m3_buf_entry){b.gpuAddress, b.length, b};
    os_unfair_lock_unlock(&g_m3_lk);
}
static void m3_buf_del(id b) { os_unfair_lock_lock(&g_m3_lk); for (int i = 0; i < g_m3_nbufs; i++) if (g_m3_bufs[i].buf == b) { g_m3_bufs[i] = g_m3_bufs[--g_m3_nbufs]; break; } os_unfair_lock_unlock(&g_m3_lk); }
static id<MTLBuffer> m3_buf(uint64_t gpu, NSUInteger *off) {
    static _Thread_local int last; id<MTLBuffer> r = nil; *off = 0; if (!gpu) return nil;
    os_unfair_lock_lock(&g_m3_lk);
    int i = last < g_m3_nbufs && gpu >= g_m3_bufs[last].gpu && gpu - g_m3_bufs[last].gpu < g_m3_bufs[last].len ? last : -1;
    for (int k = 0; i < 0 && k < g_m3_nbufs; k++) if (gpu >= g_m3_bufs[k].gpu && gpu - g_m3_bufs[k].gpu < g_m3_bufs[k].len) i = k;
    if (i >= 0) { last = i; r = g_m3_bufs[i].buf; *off = (NSUInteger)(gpu - g_m3_bufs[i].gpu); }
    os_unfair_lock_unlock(&g_m3_lk);
    return r;
}
#endif
