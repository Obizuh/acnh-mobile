// Counter pending ledger adapted from MacNativeGames (MIT), copyright retained in ACNH-THIRD-PARTY-NOTICES.md.
typedef struct { uint64_t addr, fence, val; int known; } cpend_ent;
#define CPEND_N 4096
static cpend_ent g_cpend[CPEND_N]; static unsigned g_cpend_head, g_cpend_tail; static pthread_mutex_t g_cpend_mu = PTHREAD_MUTEX_INITIALIZER;   // TOTK: [tail, head) may still be pending (fences rise with the index, so completions clear a prefix)
static void cpend_push(uint64_t addr, uint64_t fence, uint64_t val, int known) {
    pthread_mutex_lock(&g_cpend_mu); g_cpend[g_cpend_head % CPEND_N] = (cpend_ent){addr, fence, val, known}; g_cpend_head++; pthread_mutex_unlock(&g_cpend_mu);
}
static void cpend_done(uint64_t fence) {   // the records of this submit are now in memory
    pthread_mutex_lock(&g_cpend_mu);
    if (g_cpend_head - g_cpend_tail > CPEND_N) g_cpend_tail = g_cpend_head - CPEND_N;
    for (unsigned i = g_cpend_tail; i != g_cpend_head; i++) if (g_cpend[i % CPEND_N].addr && g_cpend[i % CPEND_N].fence <= fence) g_cpend[i % CPEND_N].addr = 0;
    while (g_cpend_tail != g_cpend_head && !g_cpend[g_cpend_tail % CPEND_N].addr) g_cpend_tail++;
    pthread_mutex_unlock(&g_cpend_mu);
}
// 1 = value known (written to *val), 0 = pending but unknown, -1 = no pending write (read memory)
static int cpend_lookup(uint64_t addr, uint64_t *val) {
    int r = -1; pthread_mutex_lock(&g_cpend_mu);
    unsigned n = g_cpend_head - g_cpend_tail; if (n > CPEND_N) n = CPEND_N;   // only the part that may still be pending (it was all 4,096 slots twice per conditional)
    for (unsigned k = 0; k < n; k++) { const cpend_ent *e = &g_cpend[(g_cpend_head - 1 - k) % CPEND_N]; if (e->addr == addr) { r = e->known; *val = e->val; break; } }
    pthread_mutex_unlock(&g_cpend_mu); return r;
}
