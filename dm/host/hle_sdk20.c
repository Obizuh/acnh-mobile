// 动森 3.0.3 的 SDK 20 音频初始化会检查进程句柄与 applet 资源号非零；宿主接管 oe 初始化后，SDK 的 IPC 状态没有填入。
// ACNH 3.0.3's SDK 20 audio initialization checks for a nonzero process handle and applet resource ID; the host replaces oe initialization, leaving the SDK IPC state unset.
#include "hle.h"
#include <stdatomic.h>
static int acnh_sdk20(void) { return !strcmp(dm_main_build(), "ff1d1c05670db6021c85b624a710b963"); }

// SDK20 的 FinishStartupLogo 经 applet 虚表通知系统结束启动图；宿主没有该系统图层。
// SDK20 FinishStartupLogo calls the applet vtable to dismiss the system startup logo; the host has no such system layer.
HLE_FORCE("_ZN2nn2oe17FinishStartupLogoEv", "动森结束系统启动图", void, h_acnh_FinishStartupLogo, void) {
    if (!acnh_sdk20()) { ((void (*)(void))guest_sym("_ZN2nn2oe17FinishStartupLogoEv"))(); return; }
    LOG("FinishStartupLogo：宿主启动图结束 | host startup logo finished");
}

// 单进程宿主用 Horizon 的当前进程伪句柄；其他构建继续跑各自 SDK 的原函数。
// The single-process host uses Horizon's current-process pseudo handle; other builds keep their SDK's original function.
HLE_FORCE("_ZN2nn2os23GetCurrentProcessHandleEv", "动森 SDK20 当前进程句柄", uint32_t, h_sdk20_ProcessHandle, void) {
    if (acnh_sdk20()) return UINT32_C(0xffff8001);
    return ((uint32_t (*)(void))guest_sym("_ZN2nn2os23GetCurrentProcessHandleEv"))();
}

// 进程内 CPU 音频不经过 applet 服务，给这个本地宿主实例一个非零资源标识；并非任天堂服务授权。
// In-process CPU audio does not use the applet service; give this local host instance a nonzero resource identity, not a Nintendo service authorization.
HLE_FORCE("_ZN2nn6applet23GetAppletResourceUserIdEv", "动森 SDK20 本地 applet 资源标识", uint64_t, h_sdk20_AppletResourceId, void) {
    if (acnh_sdk20()) return 1;
    return ((uint64_t (*)(void))guest_sym("_ZN2nn6applet23GetAppletResourceUserIdEv"))();
}

// 动森启动显式初始化 BCAT 下载缓存；宿主没有任天堂分发服务，不建立虚假成功的缓存挂载。
// ACNH explicitly initializes the BCAT download cache at startup; the host has no Nintendo delivery service and must not report a cache mount as successful.
HLE_FORCE("_ZN2nn4bcat10InitializeEv", "动森离线下载缓存初始化", void, h_acnh_BcatInitialize, void) {
    if (!acnh_sdk20()) { ((void (*)(void))guest_sym("_ZN2nn4bcat10InitializeEv"))(); return; }
    LOG("BCAT：本地诊断无下载缓存服务 | no delivery cache service in local diagnostics");
}
HLE_FORCE("_ZN2nn4bcat25MountDeliveryCacheStorageEv", "动森离线缓存不可用", Result, h_acnh_BcatMount, void) {
    if (!acnh_sdk20()) return ((Result (*)(void))guest_sym("_ZN2nn4bcat25MountDeliveryCacheStorageEv"))();
    LOG("BCAT：缓存挂载不可用 | cache mount unavailable"); return RES_FAIL;
}

// SDK20 经设置服务读硬件地区；诊断宿主没有该服务，提供可覆盖的本地枚举（默认 0），不访问账号。
// SDK20 reads the hardware region through the settings service; diagnostics has no such service and supplies a local overrideable enum (default 0), without account access.
HLE_FORCE("_ZN2nn2oe13GetRegionCodeEv", "动森本地硬件地区枚举", int, h_acnh_RegionCode, void) {
    if (!acnh_sdk20()) return ((int (*)(void))guest_sym("_ZN2nn2oe13GetRegionCodeEv"))();
    const char *e = getenv("DM_REGION_CODE"); return e ? atoi(e) : 0;
}

