#pragma once
#include <stdint.h>
#include <stddef.h>
extern int dm_caps_save_rgba(const void *,size_t,uint8_t[32]);
// Both SDK overloads must reach the same local writer and frontend export hook.
static inline int dm_album_save_local(const void *pixels,size_t bytes) {
 uint8_t entry[32]={0};return dm_caps_save_rgba(pixels,bytes,entry);
}
