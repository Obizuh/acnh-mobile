#pragma once
#include <stddef.h>
// Raw GPU addresses/resource IDs are a valid fast path only on the modern
// tier-2 layout we understand. Older GPU layouts remain opaque even if their
// total encoded size happens to match. This decision is made once per shader.
static inline int nvn_argument_uses_encoder(int modern_family,int tier2,size_t bytes,int max_aid,int force) {
    return force || !modern_family || !tier2 || max_aid<0 || bytes!=(size_t)8*((size_t)max_aid+1);
}
