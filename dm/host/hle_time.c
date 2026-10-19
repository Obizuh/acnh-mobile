// nn::time：真机经 time 服务取网络校准的用户时钟；宿主直接取系统时钟（单机、离线）。
// 布局按公开 Horizon ABI：PosixTime = int64 秒；CalendarTime = {s16 年, s8 月/日/时/分/秒}；
// CalendarAdditionalInfo = {u32 星期, u32 年内日, char 时区名[8], u32 夏令时, s32 UTC 偏移秒}。
// nn::time: a real device gets the network-calibrated user clock through the time service; the host reads the system clock directly (single machine, offline).
// Layout follows the public Horizon ABI: PosixTime = int64 seconds; CalendarTime = {s16 year, s8 month/day/hour/minute/second};
// CalendarAdditionalInfo = {u32 day of week, u32 day of year, char time zone name[8], u32 DST, s32 UTC offset in seconds}.
#include "hle.h"
#include <stdatomic.h>
#include <time.h>
#include <sys/sysctl.h>
#include <sys/time.h>
#include <CoreFoundation/CoreFoundation.h>

typedef struct { int16_t year; int8_t month, day, hour, minute, second; int8_t pad; } CalendarTime;
typedef struct { uint32_t wday, yday; char tzname[8]; uint32_t dst; int32_t offset; } CalendarAdditionalInfo;

// DM_FIXED_TIME=秒（确定性对拍用；真值侧对应 dm/verify/ryu/drive.py --fix-time）：
// ① 用户时钟（存档 d2s +0x20 最后游玩时间、.meta last_modified_time 的来源）恒返回该值；
// ② 建局随机数种子那一处 time()（main 0x3509a0 bl time，返回点 0x3509a4，与 drive.py 同点）返回该值。
// 本 SDK 的 libc time() 是开机时长（clock_gettime(0) → __nnmusl_ClockGetTime → GetSystemTick），不是墙钟；
// main 另有 17 处 time() 调用不动，原样转给 SDK 自己的 time()。
// DM_FIXED_TIME=seconds (for deterministic comparison; the ground-truth side counterpart is dm/verify/ryu/drive.py --fix-time):
// ① the user clock (source of the save's d2s +0x20 last-played time and .meta last_modified_time) always returns this value;
// ② the one time() at the game-creation random seed (main 0x3509a0 bl time, return point 0x3509a4, same point as drive.py) returns this value.
// This SDK's libc time() is uptime (clock_gettime(0) → __nnmusl_ClockGetTime → GetSystemTick), not wall clock;
// the other 17 time() call sites in main are left alone and forwarded unchanged to the SDK's own time().
static int64_t fixed_time(void) {
    static int64_t v = -2;
    if (v == -2) { const char *e = getenv("DM_FIXED_TIME"); v = e ? strtoll(e, 0, 0) : -1;
                   if (v >= 0) LOG("DM_FIXED_TIME=%lld：用户时钟与建局 time()（main 0x3509a4）返回定值", (long long)v); }
    return v;
}
HLE_FORCE("time", "按返回点认出建局种子那一处（DM_FIXED_TIME）；其余原样转调 SDK 的 time()", int64_t, h_time_time, int64_t *t) {
    uint64_t lr = (uint64_t)__builtin_return_address(0);
    static int64_t (*sdk_time)(int64_t *); static uint64_t seed_ret;
    if (!sdk_time) {
        module_t *ms = NULL, *mm = NULL;
        for (int i = 0; i < g_nmod; i++) { if (!strcmp(g_mod[i].name, "sdk")) ms = &g_mod[i]; if (!strcmp(g_mod[i].name, "main")) mm = &g_mod[i]; }
        void *f = ms ? module_sym(ms, "time") : NULL; if (!f) die("SDK 里找不到 time");
        uint32_t w = mm && !strcmp(mm->build_id, DM_D2R_BUILD) ? *(uint32_t *)(mm->base + 0x3509a0) : 0;   // 只在 D2R 构建上读（别的游戏这里是不相干的代码，main 小于 3.3 MB 还会越界） | read only on the D2R build (elsewhere this is unrelated code, and a main smaller than 3.3 MB would be read past its end)
        if ((w & 0xfc000000) == 0x94000000) seed_ret = (uint64_t)mm->base + 0x3509a4;               // bl time（构建 db0507f3…；改写器会改跳转目标，只认 bl 本身） | bl time (build db0507f3…; the rewriter changes the branch target, so only the bl itself is matched)
        else if (fixed_time() >= 0) LOG("【红】DM_FIXED_TIME：main 0x3509a0 不是预期的 bl time（%#x），建局 time() 未固定", w);
        sdk_time = (int64_t (*)(int64_t *))f;
    }
    int64_t ft = fixed_time();
    if (ft >= 0 && lr == seed_ret) {
        static int once; if (!once++) LOG("DM_FIXED_TIME：建局 time() 返回 %lld", (long long)ft);
        if (t) *t = ft; return ft;
    }
    return sdk_time(t);
}

HLE_FORCE("_ZN2nn4time10InitializeEv", "连 time 服务；时钟改取宿主系统时间", Result, h_time_Initialize, void) { return RES_OK; }
HLE_FORCE("_ZN2nn4time13IsInitializedEv", "time 服务已被接管，恒为已初始化", int, h_time_IsInitialized, void) { return 1; }
HLE_FORCE("_ZN2nn4time23StandardUserSystemClock14GetCurrentTimeEPNS0_9PosixTimeE", "用户时钟 → 宿主 time()", Result, h_time_GetCurrentTime, int64_t *out) {
    int64_t ft = fixed_time(); *out = ft >= 0 ? ft : time(NULL); return RES_OK;
}
// 动森 3.0.3 另用可调用户时钟：宿主时钟直接随系统，无需启动原版 time 服务做 Adjust。
// ACNH 3.0.3 also uses the adjustable user clock: the host clock already follows the system, without the original time service's Adjust step.
HLE_FORCE("_ZN2nn4time25AdjustableUserSystemClock6AdjustEv", "可调时钟直接跟宿主系统", Result, h_time_Adjust, void) { return RES_OK; }
HLE_FORCE("_ZN2nn4time25AdjustableUserSystemClock14GetCurrentTimeEPNS0_9PosixTimeE", "可调用户时钟取宿主时间", Result, h_time_AdjustableGet, int64_t *out) { return h_time_GetCurrentTime(out); }
// 首领连战第 6–10 关的强制恐怖地带（dm_bossrush.c 设）：g_dm_tz_time 非 0 且未过期时，只有「恐怖地带」那几处取时钟拿到它，存档时间等其余调用照旧。
// 哪几处：返回点落在地带函数 0x58f1e0 起的那一簇（0x58f1e0–0x591440），或返回后 8 条指令内调地带日程 0x58f310（服务端 0x636b38 套地带、0x694b48 等 9 处）。
// 过期：dm_bossrush 每拍续 1 秒，本局停拍（退出 / 崩溃）后不再生效，下一局建局时取到的是真实时钟。
// Forced Terror Zone for stages 6–10 of the boss rush (set by dm_bossrush.c): when g_dm_tz_time is non-zero and not expired, only the clock reads at the "Terror Zone" sites get it; save timestamps and all other calls are unchanged.
// Which sites: the return point falls in the cluster starting at the zone function 0x58f1e0 (0x58f1e0–0x591440), or the zone schedule 0x58f310 is called within 8 instructions after the return (server side 0x636b38 applying the zone, 0x694b48 and so on, 9 sites).
// Expiry: dm_bossrush renews it by 1 second every tick; once this run stops ticking (exit / crash) it no longer applies, and the next run gets the real clock when the game is created.
_Atomic int64_t g_dm_tz_time;
_Atomic uint64_t g_dm_tz_live_ns;
static int tz_site(uint64_t lr) {   // 只在强制期间才判，每次读返回点后 8 条指令，不缓存（多线程都会取时钟） | Checked only while forcing is active, reading the 8 instructions after the return point each time, no caching (several threads read the clock)
    static _Atomic uint64_t base; uint64_t b = atomic_load(&base);
    if (!b) { for (int i = 0; i < g_nmod; i++) if (!strcmp(g_mod[i].name, "main")) b = (uint64_t)g_mod[i].base; atomic_store(&base, b); }
    if (!b || lr < b) return 0;
    uint64_t off = lr - b; if (off > 0x58f1e0 && off < 0x591440) return 1;
    for (int k = 0; k < 8; k++) { uint32_t w = *(const uint32_t *)(uintptr_t)(lr + 4 * k);
        if ((w & 0xfc000000) == 0x94000000) { int64_t imm = w & 0x3ffffff; if (imm & 0x2000000) imm -= 1 << 26; if (off + 4 * k + imm * 4 == 0x58f310) return 1; } }
    return 0;
}
HLE_FORCE("_ZN2nn4time23StandardUserSystemClock3nowEv", "用户时钟 → 宿主 time()；首领连战强制地带时恐怖地带那几处另返回", int64_t, h_time_now, void) {
    int64_t tz = atomic_load(&g_dm_tz_time);
    if (tz && (uint64_t)clock_gettime_nsec_np(CLOCK_MONOTONIC) < atomic_load(&g_dm_tz_live_ns) && tz_site((uint64_t)__builtin_return_address(0))) return tz;
    int64_t ft = fixed_time(); return ft >= 0 ? ft : time(NULL); }
HLE_FORCE("_ZN2nn4time23StandardUserSystemClock9to_time_tERKNSt3__16chrono10time_pointINS0_17SystemClockTraitsENS3_8durationIlNS2_5ratioILl1ELl1EEEEEEE",
          "SDK 实现与 now() 配套，一并接管", int64_t, h_time_to_time_t, const int64_t *tp) { return *tp; }
HLE_FORCE("_ZN2nn4time14ToCalendarTimeEPNS0_12CalendarTimeEPNS0_22CalendarAdditionalInfoERKNS0_9PosixTimeE",
          "连 time 服务的时区规则；改用宿主 localtime_r", Result, h_time_ToCalendarTime, CalendarTime *ct, CalendarAdditionalInfo *ai, const int64_t *pt) {
    time_t t = *pt; struct tm tm; localtime_r(&t, &tm);
    static int once; if (!once++) LOG("ToCalendarTime：首次换算 %lld → %04d-%02d-%02d %02d:%02d（时区 %s，偏移 %ld 秒）", (long long)t, tm.tm_year + 1900, tm.tm_mon + 1, tm.tm_mday, tm.tm_hour, tm.tm_min, tm.tm_zone ? tm.tm_zone : "?", (long)tm.tm_gmtoff);
    if (ct) *ct = (CalendarTime){(int16_t)(tm.tm_year + 1900), (int8_t)(tm.tm_mon + 1), (int8_t)tm.tm_mday, (int8_t)tm.tm_hour, (int8_t)tm.tm_min, (int8_t)tm.tm_sec, 0};
    if (ai) { memset(ai, 0, sizeof *ai); ai->wday = tm.tm_wday; ai->yday = tm.tm_yday; strlcpy(ai->tzname, tm.tm_zone ? tm.tm_zone : "UTC", sizeof ai->tzname);
              ai->dst = tm.tm_isdst > 0; ai->offset = (int32_t)tm.tm_gmtoff; }
    return RES_OK;
}
// ACNH's displayed island time uses the explicit-rule overload. The SDK's
// time service is absent, so loading its rule failed and the game fell back to UTC.
// Keep an opaque host rule containing an IANA identifier; all imported rule
// consumers below use CoreFoundation's system timezone database, including DST.
#define HOST_ZONE_MAGIC UINT64_C(0x444d545a52554c31)
typedef struct { uint64_t magic; char location[0x24]; uint32_t reserved; } HostZoneRule;
_Static_assert(sizeof(HostZoneRule) == 48, "host timezone rule header");
static void host_zone_name(char out[0x24]) {
    memset(out, 0, 0x24);
    const char *override = getenv("TZ");
    if (override && *override && strlen(override) < 0x24) { strlcpy(out, override, 0x24); return; }
    CFTimeZoneRef zone = CFTimeZoneCopySystem();
    if (!zone || !CFStringGetCString(CFTimeZoneGetName(zone), out, 0x24, kCFStringEncodingUTF8)) strlcpy(out, "UTC", 0x24);
    if (zone) CFRelease(zone);
}
static CFTimeZoneRef host_zone_open(const char *name) {
    if (!name || !memchr(name, 0, 0x24)) return NULL;
    CFStringRef n = CFStringCreateWithCString(NULL, name, kCFStringEncodingUTF8);
    if (!n) return NULL;
    CFTimeZoneRef zone = CFTimeZoneCreateWithName(NULL, n, false); CFRelease(n); return zone;
}
HLE_FORCE("_ZN2nn4time21GetDeviceLocationNameEPNS0_12LocationNameE", "设备时区使用完整系统标识，不返回歧义缩写", Result, h_time_LocName, char *out) {
    if (!out) return RES_FAIL; host_zone_name(out); return RES_OK;
}
HLE_FORCE("_ZN2nn4time16LoadTimeZoneRuleEPNS0_12TimeZoneRuleERKNS0_12LocationNameE", "从宿主系统加载命名时区规则，不连接 time 服务", Result, h_time_LoadRule, HostZoneRule *out, const char *name) {
    if (!out) return RES_FAIL;
    CFTimeZoneRef zone = host_zone_open(name); if (!zone) return RES_FAIL;
    HostZoneRule rule = {.magic = HOST_ZONE_MAGIC}; strlcpy(rule.location, name, sizeof rule.location); *out = rule; CFRelease(zone); return RES_OK;
}
static CFTimeZoneRef host_rule_open(const HostZoneRule *rule) {
    return rule && rule->magic == HOST_ZONE_MAGIC ? host_zone_open(rule->location) : NULL;
}
static CalendarTime host_calendar(CFAbsoluteTime at, CFTimeZoneRef zone) {
    CFGregorianDate d = CFAbsoluteTimeGetGregorianDate(at, zone);
    return (CalendarTime){d.year, d.month, d.day, d.hour, d.minute, (int8_t)d.second, 0};
}
HLE_FORCE("_ZN2nn4time14ToCalendarTimeEPNS0_12CalendarTimeEPNS0_22CalendarAdditionalInfoERKNS0_9PosixTimeERKNS0_12TimeZoneRuleE", "显式时区规则日历转换，包含系统夏令时规则", Result, h_time_ToCalendarRule, CalendarTime *ct, CalendarAdditionalInfo *ai, const int64_t *pt, const HostZoneRule *rule) {
    if (!pt) return RES_FAIL; CFTimeZoneRef zone = host_rule_open(rule); if (!zone) return RES_FAIL;
    CFAbsoluteTime at = (double)*pt - kCFAbsoluteTimeIntervalSince1970;
    CalendarTime cal = host_calendar(at, zone);
    if (ct) *ct = cal;
    if (ai) {
        memset(ai, 0, sizeof *ai); ai->wday = (uint32_t)CFAbsoluteTimeGetDayOfWeek(at, zone) % 7;
        CFGregorianDate d = {cal.year, cal.month, cal.day, 0, 0, 0}, jan = {cal.year, 1, 1, 0, 0, 0};
        ai->yday = (uint32_t)((CFGregorianDateGetAbsoluteTime(d, NULL) - CFGregorianDateGetAbsoluteTime(jan, NULL)) / 86400);
        ai->offset = (int32_t)CFTimeZoneGetSecondsFromGMT(zone, at); ai->dst = CFTimeZoneIsDaylightSavingTime(zone, at);
        CFStringRef abbr = CFTimeZoneCopyAbbreviation(zone, at);
        if (abbr) { CFStringGetCString(abbr, ai->tzname, sizeof ai->tzname, kCFStringEncodingUTF8); CFRelease(abbr); }
    }
    static int once; if (!once++) LOG("Explicit timezone rule: %s, %04d-%02d-%02d %02d:%02d", rule->location, cal.year, cal.month, cal.day, cal.hour, cal.minute);
    CFRelease(zone); return RES_OK;
}
static int calendar_equal(CalendarTime a, CalendarTime b) {
    return a.year==b.year && a.month==b.month && a.day==b.day && a.hour==b.hour && a.minute==b.minute && a.second==b.second;
}
HLE_FORCE("_ZN2nn4time11ToPosixTimeEPiPNS0_9PosixTimeEiRKNS0_12CalendarTimeERKNS0_12TimeZoneRuleE", "命名时区日历转绝对时间，处理夏令时重复与缺失时刻", Result, h_time_ToPosixRule, int *count, int64_t *out, int capacity, const CalendarTime *ct, const HostZoneRule *rule) {
    if (!count || !ct || capacity < 0 || (capacity && !out)) return RES_FAIL;
    CFTimeZoneRef zone = host_rule_open(rule); if (!zone) return RES_FAIL;
    *count = 0;
    CFGregorianDate d = {ct->year, ct->month, ct->day, ct->hour, ct->minute, ct->second};
    CFAbsoluteTime naive = CFGregorianDateGetAbsoluteTime(d, NULL);
    if (ct->year < 1 || ct->year > 9999 || !calendar_equal(*ct, host_calendar(naive, NULL))) { CFRelease(zone); return RES_FAIL; }
    int64_t candidates[2]; int n = 0;
    // Sampling both sides of a transition finds the two offsets of an overlap;
    // roundtripping rejects nonexistent local times in spring's forward jump.
    const int days[] = {-2, -1, 0, 1, 2};
    for (unsigned i=0; i<sizeof days/sizeof *days; i++) {
        CFAbsoluteTime at = naive - CFTimeZoneGetSecondsFromGMT(zone, naive + days[i]*86400.0);
        if (!calendar_equal(*ct, host_calendar(at, zone))) continue;
        int64_t t = (int64_t)(at + kCFAbsoluteTimeIntervalSince1970);
        if ((n>0 && candidates[0]==t) || (n>1 && candidates[1]==t)) continue;
        if (n<2) candidates[n++] = t;
    }
    if (n==2 && candidates[0]>candidates[1]) { int64_t t=candidates[0];candidates[0]=candidates[1];candidates[1]=t; }
    int written = n < capacity ? n : capacity;
    for (int i=0;i<written;i++) out[i]=candidates[i]; *count=written;
    CFRelease(zone); return RES_OK;
}
// 时钟快照按 Horizon 公开 ABI（0xd0）：墙钟、单调时间及其偏移在同一次采样中一致；无网络校准，自动校准标志为 0。
// Clock snapshots use Horizon's public 0xd0 ABI: wall time, monotonic time and offsets agree in one sample; automatic network correction is disabled.
typedef struct { int64_t seconds; uint8_t source[16]; } HostSteadyPoint;
typedef struct { int64_t offset; HostSteadyPoint point; } HostClockContext;
typedef struct {
    HostClockContext user_context, network_context;
    int64_t user_time, network_time;
    CalendarTime user_calendar, network_calendar;
    CalendarAdditionalInfo user_info, network_info;
    HostSteadyPoint steady;
    char location[0x24];
    uint8_t automatic, type, pad[2];
} HostClockSnapshot;
_Static_assert(sizeof(HostClockSnapshot) == 0xd0, "ClockSnapshot ABI");
static uint8_t g_clock_source[16];
static pthread_once_t g_clock_source_once = PTHREAD_ONCE_INIT;
static void clock_source_init(void) {
    // 启动时间形成每次宿主开机唯一、跨 App 重启稳定的时钟源；系统不允许读取时才退到本进程随机源。
    // Boot time identifies a host boot across app restarts; use a process-local random source only if the system denies that query.
    struct timeval boot; size_t n = sizeof boot;
    if (!sysctlbyname("kern.boottime", &boot, &n, NULL, 0) && n == sizeof boot) {
        uint64_t id[2] = {UINT64_C(0x444d484f5354434c) ^ (uint64_t)boot.tv_sec, (uint64_t)boot.tv_usec};
        memcpy(g_clock_source, id, sizeof id);
    } else arc4random_buf(g_clock_source, sizeof g_clock_source);
}
HLE_FORCE("_ZN2nn4time13ClockSnapshot29CreateWithStandardSystemClockEPS1_", "宿主墙钟与单调时钟快照", Result, h_time_ClockSnapshot, HostClockSnapshot *out) {
    pthread_once(&g_clock_source_once, clock_source_init);
    memset(out, 0, sizeof *out);
    struct timespec mono; if (clock_gettime(CLOCK_MONOTONIC, &mono)) return RES_FAIL;
    h_time_GetCurrentTime(&out->user_time); out->network_time = out->user_time;
    out->steady.seconds = mono.tv_sec; memcpy(out->steady.source, g_clock_source, 16);
    out->user_context.point = out->network_context.point = out->steady;
    out->user_context.offset = out->network_context.offset = out->user_time - mono.tv_sec;
    h_time_ToCalendarTime(&out->user_calendar, &out->user_info, &out->user_time);
    out->network_calendar = out->user_calendar; out->network_info = out->user_info;
    host_zone_name(out->location);
    return RES_OK;
}
// Snapshot comparisons must use the same host clock model as snapshot creation.
// A user adjustment is the change of the wall/steady offset, not elapsed play time.
static int clock_seconds_ns(__int128 seconds, int64_t *out) {
    __int128 ns = seconds * 1000000000;
    if (ns < INT64_MIN || ns > INT64_MAX) return 0;
    *out = (int64_t)ns; return 1;
}
HLE_FORCE("_ZN2nn4time48CalculateStandardUserSystemClockDifferenceByUserERKNS0_13ClockSnapshotES3_",
          "比较快照中的用户调钟偏移，不进入 time 服务", int64_t, h_time_UserClockDifference,
          const HostClockSnapshot *a, const HostClockSnapshot *b) {
    if (memcmp(a->user_context.point.source, b->user_context.point.source, 16) ||
        (a->automatic && b->automatic)) return 0;
    __int128 seconds = (__int128)b->user_context.offset - a->user_context.offset;
    int64_t ns;
    if (!clock_seconds_ns(seconds, &ns)) return seconds < 0 ? INT64_MIN : INT64_MAX;
    return ns;
}
HLE_FORCE("_ZN2nn4time20CalculateSpanBetweenEPNS_8TimeSpanERKNS0_13ClockSnapshotES5_",
          "同源快照用单调时钟计算间隔，跨源回退有效网络时间", Result, h_time_SnapshotSpan,
          int64_t *out, const HostClockSnapshot *a, const HostClockSnapshot *b) {
    __int128 seconds;
    if (!memcmp(a->steady.source, b->steady.source, 16))
        seconds = (__int128)b->steady.seconds - a->steady.seconds;
    else if (a->network_time && b->network_time)
        seconds = (__int128)b->network_time - a->network_time;
    else return RES_FAIL;
    int64_t ns;
    if (!clock_seconds_ns(seconds, &ns)) return RES_FAIL;
    *out = ns; return RES_OK;
}
// 本地时刻换算强制接宿主 libc：SDK 自带的 localtime / localtime_r / mktime 没有时区数据、恒按 UTC（宿主实测：编年史「首次发现」由
// main+0xb04d34 调 localtime 再 strftime 出，差 8 小时）。真机 Switch 的 SDK 按系统时区换算，这里跟宿主系统时区走才一致；
// 宿主 TZ 平时 = 系统时区，只在 DM_FIXED_TIME 确定性对拍时钉 UTC（main.c）。struct tm 布局两边相同（9 个 int、long 偏移、时区名指针）。
// gmtime / gmtime_r / timegm 本来就是 UTC，不接。hle_table.c 的同名直通项只在符号没有定义时才生效（SDK 有定义），所以要 HLE_FORCE
// Local time conversion is forced onto the host libc: the SDK's own localtime / localtime_r / mktime have no time zone data and always use UTC (measured on the host: the chronicle's 「首次发现」(First Discovered) is produced by
// main+0xb04d34 calling localtime and then strftime, and was off by 8 hours). The SDK on a real Switch converts by the system time zone, so only following the host system time zone here is consistent;
// the host TZ normally = the system time zone, and is pinned to UTC only for deterministic comparison with DM_FIXED_TIME (main.c). The struct tm layout is the same on both sides (9 ints, long offset, time zone name pointer).
// gmtime / gmtime_r / timegm are UTC anyway and are not taken over. The same-named pass-through entries in hle_table.c only take effect when the symbol has no definition (the SDK defines them), hence HLE_FORCE
HLE_FORCE("localtime", "本地时刻 → 宿主 localtime（跟系统时区）", struct tm *, h_localtime, const int64_t *t) { time_t v = (time_t)*t; return localtime(&v); }
HLE_FORCE("localtime_r", "本地时刻 → 宿主 localtime_r（跟系统时区）", struct tm *, h_localtime_r, const int64_t *t, struct tm *out) { time_t v = (time_t)*t; return localtime_r(&v, out); }
HLE_FORCE("mktime", "本地日历 → 绝对秒，宿主 mktime（跟系统时区）", int64_t, h_mktime, struct tm *tm) { return (int64_t)mktime(tm); }
