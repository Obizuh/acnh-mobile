// ELF64 RELR：偶数项给出地址，奇数项用 63 位位图描述后续指针。所有访问先核对映像边界。
// ELF64 RELR: even entries give an address; odd entries bitmap the next 63 pointers. Check image bounds before every access.
#pragma once
#include <stdint.h>
#include <stddef.h>
#include <string.h>

static inline int dm_relr_apply(uint8_t *base, size_t size, uint64_t off, uint64_t bytes, uint64_t *count) {
    if ((off & 7) || (bytes & 7) || off > size || bytes > size - off) return -1;
    uint64_t next = 0, n = 0; int have_address = 0;
    for (uint64_t i = 0; i < bytes / 8; i++) {
        uint64_t entry; memcpy(&entry, base + off + i * 8, 8);
        if (!(entry & 1)) {
            if ((entry & 7) || entry > size || size - entry < 8) return -1;
            uint64_t value; memcpy(&value, base + entry, 8); value += (uintptr_t)base;
            memcpy(base + entry, &value, 8); n++; next = entry + 8; have_address = 1;
        } else {
            if (!have_address || next > UINT64_MAX - 63 * 8) return -1;
            for (unsigned bit = 1; bit < 64; bit++) if (entry & (UINT64_C(1) << bit)) {
                uint64_t target = next + (bit - 1) * 8;
                if (target > size || size - target < 8) return -1;
                uint64_t value; memcpy(&value, base + target, 8); value += (uintptr_t)base;
                memcpy(base + target, &value, 8); n++;
            }
            next += 63 * 8;
        }
    }
    if (count) *count = n;
    return 0;
}
