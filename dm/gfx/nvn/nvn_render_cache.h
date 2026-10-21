#pragma once
#include <string.h>
#include <stdint.h>
// Exact bytes only, scoped to one render encoder. Never caches resources or draws.
typedef struct { uint32_t valid; unsigned char value[9][64]; } nvn_render_cache;
static inline void nvn_render_cache_reset(nvn_render_cache *c) { c->valid=0; }
static inline int nvn_render_cache_changed(nvn_render_cache *c, unsigned slot, const void *v, size_t n) {
    if (slot>=9 || n>64) return 1;
    uint32_t bit=1u<<slot;
    if ((c->valid&bit) && !memcmp(c->value[slot],v,n)) return 0;
    memcpy(c->value[slot],v,n);c->valid|=bit;return 1;
}
