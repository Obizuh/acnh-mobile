// Regression: the watchdog must sample time while holding the audio lifecycle lock.
// No AudioUnit is created. The real watchdog runs one iteration with a controlled sleep.
#include <unistd.h>
#include <stdatomic.h>
#include <errno.h>
static int test_sleep(useconds_t value);
#define usleep test_sleep
#include "../hle_audio.c"
#undef usleep
static _Atomic int phase, samples, protected_samples;
uint64_t now_ns(void) {
    if (atomic_load(&phase) == 1) {
        int rc = pthread_mutex_trylock(&g_aos_lock);
        atomic_fetch_add(&samples, 1);
        if (rc == EBUSY) atomic_fetch_add(&protected_samples, 1);
        else if (!rc) pthread_mutex_unlock(&g_aos_lock);
    }
    return 1000000000ull + atomic_load(&phase) * 50000000ull;
}
static int test_sleep(useconds_t value) {
    (void)value;
    if (atomic_fetch_add(&phase, 1)) pthread_exit(NULL);
    return 0;
}
static struct { const void *k; void *v; } om[8];
void *objmap_get(const void *k) { for (int i = 0; i < 8; i++) if (om[i].k == k) return om[i].v; return NULL; }
void objmap_put(const void *k, void *v) { for (int i = 0; i < 8; i++) if (!om[i].k) { om[i].k = k; om[i].v = v; return; } }
void *objmap_del(const void *k) { for (int i = 0; i < 8; i++) if (om[i].k == k) { om[i].k = 0; return om[i].v; } return NULL; }
static _Atomic int g_sig;
void hle_signal_system_event(void *g) { (void)g; atomic_fetch_add_explicit(&g_sig, 1, memory_order_seq_cst); }
void hle_bind_system_event(void *g, int autoclear) { (void)g; (void)autoclear; }   // the real implementation needs a guest thread + SDK kernel event; the unit test only counts triggers
module_t *module_of(const void *p) { (void)p; return NULL; }   // call sites in the log: no guest modules in the unit test
void dm_host_helper_qos(qos_class_t q) { (void)q; }   // hle_thread.c: the helper threads keep the default class in the unit test
int main(void) {
    pthread_t thread;
    if (pthread_create(&thread, NULL, au_watchdog, NULL)) return 2;
    pthread_join(thread, NULL);
    printf("watchdog samples: %d; serialized with resume: %d\n", atomic_load(&samples), atomic_load(&protected_samples));
    return !(atomic_load(&samples) == 1 && atomic_load(&protected_samples) == 1);
}
