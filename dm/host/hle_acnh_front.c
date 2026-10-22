#include "front.h"

__attribute__((weak)) void front_nfc_probe_start(void) {}
__attribute__((weak)) void front_nfc_probe_stop(void) {}
__attribute__((weak)) void front_amiibo_file_start(uint64_t session) {(void)session;}
__attribute__((weak)) void front_amiibo_file_stop(void) {}

__attribute__((weak)) void front_photo_saved(const char *path) {(void)path;}
