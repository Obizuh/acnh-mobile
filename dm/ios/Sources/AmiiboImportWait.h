#pragma once
// A dedicated freeze reason composes with background (1) and keyboard (2).
// Held across menus, file-provider I/O, and retry prompts; never a real-time
// clock adjustment. Idempotent cleanup cannot unfreeze another UI's reason.
void guest_clock_freeze_for(int reason,int on);
void dm_gfx_suspend_for(int reason,int on);
typedef struct { int held; } AmiiboImportWait;
static inline void amiibo_import_wait_begin(AmiiboImportWait *wait) {
    if(!wait->held){wait->held=1;guest_clock_freeze_for(3,1);dm_gfx_suspend_for(3,1);}
}
static inline void amiibo_import_wait_end(AmiiboImportWait *wait) {
    if(wait->held){wait->held=0;guest_clock_freeze_for(3,0);dm_gfx_suspend_for(3,0);}
}
