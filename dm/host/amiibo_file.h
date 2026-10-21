#pragma once
#include <stddef.h>
#include <stdint.h>
// 0 accepted; -1 stale/closed session; -2 invalid raw NTAG215 dump.
int hle_nfp_import_file(uint64_t session, const void *data, size_t size);
