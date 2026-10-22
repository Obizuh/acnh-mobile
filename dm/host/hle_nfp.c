#include "hle.h"
#include "front.h"
#include <stdatomic.h>

#include "amiibo_file.h"
#include <pthread.h>
// Read-only raw NTAG215 import. ACNH imports TagInfo / Mount / ModelInfo only;
// encrypted owner/application data is neither decrypted nor exposed.
#define NFP_RES_TAG_NOT_FOUND (115u | 97u << 9)   // 模块 115（nfp）描述 97 | module 115 (nfp), description 97
enum { NFP_STATE_INIT = 0, NFP_STATE_SEARCH = 1, NFP_STATE_FOUND = 2, NFP_STATE_MOUNTED = 4 };
typedef struct { uint32_t id; uint8_t reserved[4]; } NfpDeviceHandle;
#define NFP_DEVICE_ID 0x4e465030u   // 'NFP0'
static _Atomic int g_nfp_state;
static _Atomic int g_nfp_initialized;
uint32_t hle_hid_connected_npad(void);
static pthread_mutex_t g_nfp_lock=PTHREAD_MUTEX_INITIALIZER;
static uint64_t g_nfp_session;
static uint8_t g_nfp_uid[7],g_nfp_model[7];
static int nfp_present(void) { int s=atomic_load(&g_nfp_state);return s==NFP_STATE_FOUND || s==NFP_STATE_MOUNTED; }
static void nfp_close(void) {
    pthread_mutex_lock(&g_nfp_lock);
    ++g_nfp_session;memset(g_nfp_uid,0,sizeof g_nfp_uid);memset(g_nfp_model,0,sizeof g_nfp_model);
    atomic_store(&g_nfp_state,NFP_STATE_INIT);pthread_mutex_unlock(&g_nfp_lock);
    front_amiibo_file_stop();front_nfc_probe_stop();
}
int hle_nfp_import_file(uint64_t session,const void *data,size_t size) {
    const uint8_t *d=data;
    // Standard raw NTAG215 dumps: tag memory, config, optional signature.
    if(!d || (size!=532 && size!=540 && size!=572)){LOG("[amiibo-file] rejected: unsupported size=%zu",size);return -2;}
    // amiibo's capability container is F1 10 FF EE, not the generic NDEF
    // E1 10 3E 00 used by a blank NTAG215. Keep both for read-only backups.
    const uint8_t amiibo_cc[4]={0xf1,0x10,0xff,0xee},ndef_cc[4]={0xe1,0x10,0x3e,0x00};
    if(d[0]!=4 || d[3]!=(0x88^d[0]^d[1]^d[2]) || d[8]!=(d[4]^d[5]^d[6]^d[7]) ||
       d[9]!=0x48){LOG("[amiibo-file] rejected: UID/manufacturer/header check");return -2;}
    if(memcmp(d+12,amiibo_cc,4) && memcmp(d+12,ndef_cc,4)){LOG("[amiibo-file] rejected: unsupported capability container");return -2;}
    if(d[0x5b]!=2 || d[0x57]>3){LOG("[amiibo-file] rejected: model layout");return -2;}
    unsigned nonzero=0;for(int i=0;i<7;i++)nonzero|=d[0x54+i];if(!nonzero)return -2;
    pthread_mutex_lock(&g_nfp_lock);
    if(session!=g_nfp_session || !atomic_load(&g_nfp_initialized) || atomic_load(&g_nfp_state)!=NFP_STATE_SEARCH){pthread_mutex_unlock(&g_nfp_lock);return -1;}
    memcpy(g_nfp_uid,d,3);memcpy(g_nfp_uid+3,d+4,4);
    // Character ID is an opaque three-byte sequence, NOT a native-endian u16.
    // Only the numbering/model ID is byte-swapped by the NFP service.
    g_nfp_model[0]=d[0x54];g_nfp_model[1]=d[0x55];g_nfp_model[2]=d[0x56];g_nfp_model[3]=d[0x5a];
    g_nfp_model[4]=d[0x59];g_nfp_model[5]=d[0x58];g_nfp_model[6]=d[0x57];
    atomic_store(&g_nfp_state,NFP_STATE_FOUND);pthread_mutex_unlock(&g_nfp_lock);
    LOG("[amiibo-file] accepted raw dump bytes=%zu; tag found (read-only)",size);return 0;
}
static Result nfp_no_tag(const char *what) {
    static _Atomic int logged; if (!atomic_exchange(&logged, 1)) LOG("nfp：%s 等要 amiibo 标签的操作 → 没找到标签（未导入标签）", what);
    return NFP_RES_TAG_NOT_FOUND;
}
// Mac-only diagnostic exercises the real SDK lifecycle on the same address.
static void nfp_event_selftest(void) {
#if !TARGET_OS_IPHONE
    static int ran;
    if (!getenv("ACNH_NFP_EVENT_SELFTEST") || ran++) return;
    _Alignas(16) unsigned char ev[128]={0};
    _Bool (*poll)(void *)=guest_sym("_ZN2nn2os18TryWaitSystemEventEPNS0_15SystemEventTypeE");
    void hle_destroy_system_event(void *);
    for(int i=0;i<64;i++){
        hle_bind_system_event(ev,0);
        if(poll(ev))die("NFP event selftest: quiet event signaled");
        hle_destroy_system_event(ev);
    }
    LOG("NFP event selftest: 64 same-address bind/poll/destroy cycles passed");
    if(!strcmp(getenv("ACNH_NFP_EVENT_SELFTEST"),"exit"))exit(0);
#endif
}
HLE_FORCE("_ZN2nn3nfp10InitializeEv", "ACNH service compatibility", void, h_nfp_Init, void) { nfp_event_selftest(); if (!atomic_exchange(&g_nfp_initialized, 1)) LOG("nfp::Initialize：宿主一台 NFC 设备，找不到 amiibo 标签"); }
// Library state differs from tag-search device state; ACNH polls library state each frame, so it must not read the untouched SDK globals.
HLE_FORCE("_ZN2nn3nfp8GetStateEv", "ACNH service compatibility", int, h_nfp_GetLibraryState, void) { return atomic_load(&g_nfp_initialized) ? 1 : 0; }
HLE_FORCE("_ZN2nn3nfp8FinalizeEv", "ACNH service compatibility", void, h_nfp_Finalize, void) { nfp_close(); atomic_store(&g_nfp_initialized, 0); }
HLE_FORCE("_ZN2nn3nfp29AttachAvailabilityChangeEventEPNS_2os15SystemEventTypeE", "ACNH service compatibility", void, h_nfp_AttachAvail, void *ev) { hle_bind_system_event(ev, 0); }
HLE_FORCE("_ZN2nn3nfp11ListDevicesEPNS0_12DeviceHandleEPii", "ACNH service compatibility", Result, h_nfp_List, NfpDeviceHandle *out, int *n, int max) {
    *n = 0; if (max >= 1) { memset(&out[0], 0, sizeof out[0]); out[0].id = NFP_DEVICE_ID; *n = 1; }
    return RES_OK;
}
HLE_FORCE("_ZN2nn3nfp9GetNpadIdEPjRKNS0_12DeviceHandleE", "ACNH service compatibility", Result, h_nfp_NpadId, uint32_t *out, const NfpDeviceHandle *h) { (void)h; *out = hle_hid_connected_npad(); return RES_OK; }
HLE_FORCE("_ZN2nn3nfp14StartDetectionERKNS0_12DeviceHandleE", "ACNH service compatibility", Result, h_nfp_Start, const NfpDeviceHandle *h) { (void)h;
    pthread_mutex_lock(&g_nfp_lock);
    if(!atomic_load(&g_nfp_initialized)){pthread_mutex_unlock(&g_nfp_lock);return NFP_RES_TAG_NOT_FOUND;}
    uint64_t session=++g_nfp_session;memset(g_nfp_uid,0,sizeof g_nfp_uid);memset(g_nfp_model,0,sizeof g_nfp_model);
    atomic_store(&g_nfp_state,NFP_STATE_SEARCH);pthread_mutex_unlock(&g_nfp_lock);
    front_amiibo_file_start(session);return RES_OK; }
HLE_FORCE("_ZN2nn3nfp13StopDetectionERKNS0_12DeviceHandleE", "ACNH service compatibility", Result, h_nfp_Stop, const NfpDeviceHandle *h) { (void)h; nfp_close(); return RES_OK; }
HLE_FORCE("_ZN2nn3nfp14GetDeviceStateERKNS0_12DeviceHandleE", "ACNH service compatibility", int, h_nfp_State, const NfpDeviceHandle *h) { (void)h; return atomic_load(&g_nfp_state); }
HLE_FORCE("_ZN2nn3nfp10GetTagInfoEPNS0_7TagInfoERKNS0_12DeviceHandleE", "ACNH service compatibility", Result, h_nfp_TagInfo, void *out, const NfpDeviceHandle *h) { (void)h;pthread_mutex_lock(&g_nfp_lock);
    if(!out || !nfp_present()){pthread_mutex_unlock(&g_nfp_lock);return nfp_no_tag("GetTagInfo");}
    uint8_t *b=out;memset(b,0,0x58);memcpy(b,g_nfp_uid,7);b[10]=7;
    uint32_t protocol=1,type=2;memcpy(b+0x20,&protocol,4);memcpy(b+0x24,&type,4);
    pthread_mutex_unlock(&g_nfp_lock);LOG("[amiibo-file] GetTagInfo");return RES_OK; }
HLE_FORCE("_ZN2nn3nfp12GetModelInfoEPNS0_9ModelInfoERKNS0_12DeviceHandleE", "ACNH service compatibility", Result, h_nfp_ModelInfo, void *out, const NfpDeviceHandle *h) { (void)h;pthread_mutex_lock(&g_nfp_lock);
    if(!out || atomic_load(&g_nfp_state)!=NFP_STATE_MOUNTED){pthread_mutex_unlock(&g_nfp_lock);return nfp_no_tag("GetModelInfo");}
    memset(out,0,0x40);memcpy(out,g_nfp_model,7);pthread_mutex_unlock(&g_nfp_lock);
    LOG("[amiibo-file] GetModelInfo delivered");return RES_OK; }
HLE_FORCE("_ZN2nn3nfp5MountERKNS0_12DeviceHandleENS0_9ModelTypeENS0_11MountTargetE", "ACNH service compatibility", Result, h_nfp_Mount, const NfpDeviceHandle *h, int model, int target) { (void)h;pthread_mutex_lock(&g_nfp_lock);
    // This backend exposes ROM only; never claim to mount encrypted RAM.
    int ok=atomic_load(&g_nfp_state)==NFP_STATE_FOUND && model==0 && target==1;
    if(ok)atomic_store(&g_nfp_state,NFP_STATE_MOUNTED);pthread_mutex_unlock(&g_nfp_lock);
    LOG("[amiibo-file] Mount model=%d target=%d accepted=%d",model,target,ok);return ok?RES_OK:NFP_RES_TAG_NOT_FOUND; }
HLE_FORCE("_ZN2nn3nfp5FlushERKNS0_12DeviceHandleE", "ACNH service compatibility", Result, h_nfp_Flush, const NfpDeviceHandle *h) { (void)h; return nfp_no_tag("Flush"); }
HLE_FORCE("_ZN2nn3nfp19OpenApplicationAreaERKNS0_12DeviceHandleEj", "ACNH service compatibility", Result, h_nfp_OpenArea, const NfpDeviceHandle *h, uint32_t id) { (void)h; (void)id; return nfp_no_tag("OpenApplicationArea"); }
HLE_FORCE("_ZN2nn3nfp18GetApplicationAreaEPvPmRKNS0_12DeviceHandleEm", "ACNH service compatibility", Result, h_nfp_GetArea, void *buf, uint64_t *n, const NfpDeviceHandle *h, uint64_t sz) { (void)buf; (void)h; (void)sz; if (n) *n = 0; return nfp_no_tag("GetApplicationArea"); }
HLE_FORCE("_ZN2nn3nfp18SetApplicationAreaERKNS0_12DeviceHandleEPKvm", "ACNH service compatibility", Result, h_nfp_SetArea, const NfpDeviceHandle *h, const void *buf, uint64_t sz) { (void)h; (void)buf; (void)sz; return nfp_no_tag("SetApplicationArea"); }
HLE_FORCE("_ZN2nn3nfp21CreateApplicationAreaERKNS0_12DeviceHandleERKNS0_25ApplicationAreaCreateInfoE", "ACNH service compatibility", Result, h_nfp_CreateArea, const NfpDeviceHandle *h, const void *info) { (void)h; (void)info; return nfp_no_tag("CreateApplicationArea"); }
HLE_FORCE("_ZN2nn3nfp23RecreateApplicationAreaERKNS0_12DeviceHandleERKNS0_25ApplicationAreaCreateInfoE", "ACNH service compatibility", Result, h_nfp_RecreateArea, const NfpDeviceHandle *h, const void *info) { (void)h; (void)info; return nfp_no_tag("RecreateApplicationArea"); }
HLE_FORCE("_ZN2nn3nfp13StartRestorerEPNS0_12DeviceHandleERKNS0_24AmiiboSettingsStartParamERKNS0_7TagInfoE", "ACNH service compatibility", Result, h_nfp_Restorer, NfpDeviceHandle *h, const void *p, const void *tag) { (void)h; (void)p; (void)tag; return nfp_no_tag("StartRestorer"); }
HLE_FORCE("_ZN2nn3nfp19StartGameDataEraserEPNS0_12DeviceHandleERKNS0_24AmiiboSettingsStartParamERKNS0_7TagInfoE", "ACNH service compatibility", Result, h_nfp_Eraser, NfpDeviceHandle *h, const void *p, const void *tag) { (void)h; (void)p; (void)tag; return nfp_no_tag("StartGameDataEraser"); }
HLE_FORCE("_ZN2nn3nfp29StartNicknameAndOwnerSettingsEPNS0_12DeviceHandleEPbPNS0_12RegisterInfoERKNS0_24AmiiboSettingsStartParamERKNS0_7TagInfoE", "ACNH service compatibility", Result, h_nfp_Nick, NfpDeviceHandle *h, uint8_t *changed, void *reg, const void *p, const void *tag) { (void)h; (void)reg; (void)p; (void)tag; if (changed) *changed = 0; return nfp_no_tag("StartNicknameAndOwnerSettings"); }
HLE_FORCE("_ZN2nn3nfp29StartNicknameAndOwnerSettingsEPNS0_12DeviceHandleEPbPNS0_12RegisterInfoERKNS0_24AmiiboSettingsStartParamERKNS0_7TagInfoERKS4_", "ACNH service compatibility", Result, h_nfp_Nick2, NfpDeviceHandle *h, uint8_t *changed, void *reg, const void *p, const void *tag, const void *old) { (void)h; (void)reg; (void)p; (void)tag; (void)old; if (changed) *changed = 0; return nfp_no_tag("StartNicknameAndOwnerSettings"); }


HLE_FORCE("_ZN2nn3nfp7UnmountERKNS0_12DeviceHandleE", "ACNH service compatibility", Result, h_nfp_Unmount, const NfpDeviceHandle *h) {
 (void)h;pthread_mutex_lock(&g_nfp_lock);int ok=atomic_load(&g_nfp_state)==NFP_STATE_MOUNTED;
 if(ok)atomic_store(&g_nfp_state,NFP_STATE_FOUND);pthread_mutex_unlock(&g_nfp_lock);return ok?RES_OK:NFP_RES_TAG_NOT_FOUND;
}
