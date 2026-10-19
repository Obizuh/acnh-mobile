// iOS App 入口：设环境（游戏文件、存档、着色器库）、把宿主日志写进 Documents/log.txt、建窗口（front_ios.m），再在大栈线程上跑宿主 dm_main。
// 游戏文件（RomFS）随 App 打包在 <App>/romfs；也认「文件」App 里放的 Documents/romfs（不必重装 App 就能换）。
// iOS App entry: set the environment (game files, saves, shader libraries), write the host log to Documents/log.txt, create the window (front_ios.m), then run the host's dm_main on a big-stack thread.
// The game files (RomFS) ship inside the App at <App>/romfs; Documents/romfs placed through the Files app is also accepted (lets you swap them without reinstalling).
#import <UIKit/UIKit.h>
#include <pthread.h>
#include <signal.h>
#include <stdio.h>
#include <unistd.h>
#include <mach/mach.h>
#include <os/proc.h>
#import "AcnhResourcePack.h"
#include "../../host/acnh_data.h"
#include "game_profile.h"   // 游戏档案常数（RomFS 认定路径） | game profile constants (the RomFS marker path)

int dm_main(int argc, char **argv);   // dm/host/main.c 以 -Dmain=dm_main 编入 | dm/host/main.c compiled in with -Dmain=dm_main
void dm_host_preload_start(int argc, char **argv);
UIViewController *front_ios_root(void);
void front_ios_focus_event(int ev);

static char *g_argv[4]; static int g_argc;

static NSString *g_resource_failure;
static NSString *data_locate(void) {
    NSString *doc=NSSearchPathForDirectoriesInDomains(NSDocumentDirectory,NSUserDomainMask,YES)[0];
    for(NSString *folder in @[doc,[doc stringByAppendingPathComponent:@"ACNHMobile"],[doc stringByAppendingPathComponent:@"data"],[doc stringByAppendingPathComponent:@"romfs"],[doc stringByAppendingPathComponent:@"动森分卷资源"]])
        if([NSFileManager.defaultManager fileExistsAtPath:[folder stringByAppendingPathComponent:@"ACNH-index.data"]])return folder;
    return nil;
}
static NSString *romfs_locate(void) {
    NSFileManager *fm = NSFileManager.defaultManager;
    NSString *doc = NSSearchPathForDirectoriesInDomains(NSDocumentDirectory, NSUserDomainMask, YES)[0];
    for (NSString *d in @[[doc stringByAppendingPathComponent:@"romfs"], [NSBundle.mainBundle.resourcePath stringByAppendingPathComponent:@"romfs"]])
        if ([fm fileExistsAtPath:[d stringByAppendingPathComponent:@DM_GAME_ROMFS_MARKER]]) return d;   // 有游戏档案 romfs_marker 那个路径的目录才算（马力欧：System/Resource，资源尺寸表） | only a directory containing the game profile's romfs_marker path counts (Mario: System/Resource, the resource size table)
    return nil;
}

// 启动前提（UIApplicationMain 前做）：目录 / 环境变量 / 日志——宿主提前装载要读这些
// Boot prerequisites (done before UIApplicationMain): directories / environment variables / log -- the host's early load reads these
static int boot_env(void) {
    NSFileManager *fm = NSFileManager.defaultManager;
    NSString *doc = NSSearchPathForDirectoriesInDomains(NSDocumentDirectory, NSUserDomainMask, YES)[0];
    NSString *res = NSBundle.mainBundle.resourcePath;
    for (NSString *d in @[@"save", @"temp"]) [fm createDirectoryAtPath:[doc stringByAppendingPathComponent:d] withIntermediateDirectories:YES attributes:nil error:nil];
    // 日志：宿主只写 stderr；重定向到 Documents/log.txt（上一次的留作 log.prev.txt），出问题时从「文件」App 取
    // Log: the host only writes to stderr; redirect it to Documents/log.txt (the previous one is kept as log.prev.txt), retrieve it from the Files app when something goes wrong
    NSString *log = [doc stringByAppendingPathComponent:@"log.txt"], *prev = [doc stringByAppendingPathComponent:@"log.prev.txt"];
    [fm removeItemAtPath:prev error:nil]; [fm moveItemAtPath:log toPath:prev error:nil];
    if (freopen(log.fileSystemRepresentation, "w", stderr)) setvbuf(stderr, NULL, _IOLBF, 0);
    fprintf(stderr, "boot: ACNH build=%s version=%s\n", [[NSBundle.mainBundle objectForInfoDictionaryKey:@"CFBundleVersion"] UTF8String], [[NSBundle.mainBundle objectForInfoDictionaryKey:@"CFBundleShortVersionString"] UTF8String]);
    // 环境开关文件（真机诊断用，侧载装的包没法从外面设环境变量）：每行 KEY=VALUE，# 开头为注释；Documents/env.txt 优先于包内 dm/env.txt（build_ipa.sh 的 DM_ENV_FILE 放进去）
    // Environment switch files (for on-device diagnostics: a sideloaded app cannot be given environment variables from outside): one KEY=VALUE per line, # starts a comment; Documents/env.txt takes precedence over dm/env.txt in the bundle (put there by build_ipa.sh's DM_ENV_FILE)
    for (NSString *ef in @[[doc stringByAppendingPathComponent:@"env.txt"], [[res stringByAppendingPathComponent:@"dm"] stringByAppendingPathComponent:@"env.txt"]]) {
        NSString *txt = [NSString stringWithContentsOfFile:ef encoding:NSUTF8StringEncoding error:nil];
        for (NSString *raw in [txt componentsSeparatedByCharactersInSet:NSCharacterSet.newlineCharacterSet]) {
            NSString *ln = [raw stringByTrimmingCharactersInSet:NSCharacterSet.whitespaceCharacterSet]; NSRange eq = [ln rangeOfString:@"="];
            if (!ln.length || [ln hasPrefix:@"#"] || eq.location == NSNotFound || !eq.location) continue;
            NSString *k = [ln substringToIndex:eq.location], *v = [ln substringFromIndex:eq.location + 1];
            if (!getenv(k.UTF8String)) { setenv(k.UTF8String, v.UTF8String, 0); fprintf(stderr, "boot: 环境 %s=%s（%s）\n", k.UTF8String, v.UTF8String, ef.lastPathComponent.UTF8String); }
        }
    }
    // Old diagnostic env.txt files can survive an IPA replacement in Documents.
    // Normal play should not log every touch poll or maintain a resource census.
    if (!getenv("ACNH_DIAGNOSTICS")) {
        if (getenv("DM_TOUCH_LOG") || getenv("DM_RES_CENSUS")) fprintf(stderr, "boot: high-frequency diagnostics disabled for normal play\n");
        unsetenv("DM_TOUCH_LOG"); unsetenv("DM_RES_CENSUS");
        unsetenv("ACNH_AUDIO_WAV"); unsetenv("DM_FSLOG");
    }
    setenv("DM_OPMODE", "0", 0); // Native 720p handheld rendering reduces pixel work.
    fprintf(stderr, "boot: power profile opmode=%s diagnostics=%d\n", getenv("DM_OPMODE"), getenv("ACNH_DIAGNOSTICS") != NULL);
    NSString *romfs=data_locate();
    if(romfs){g_resource_failure=acnh_mount_resources(romfs);if(g_resource_failure){fprintf(stderr,"boot: .data mount failed: %s\n",g_resource_failure.UTF8String);return 0;}fprintf(stderr,"boot: .data direct read, no extraction\n");}
    else romfs=romfs_locate();
    if (!romfs) { fprintf(stderr, "boot: 没找到游戏文件（<App>/romfs 或 Documents/romfs）\n"); return 0; }
    fprintf(stderr, "boot: 游戏文件 %s\n", romfs.fileSystemRepresentation);
    setenv("DM_ROMFS", romfs.fileSystemRepresentation, dm_acnh_data_root()!=NULL);
    setenv("DM_SAVE", [doc stringByAppendingPathComponent:@"save"].fileSystemRepresentation, 0);
    setenv("DM_TEMP", [doc stringByAppendingPathComponent:@"temp"].fileSystemRepresentation, 0);
    NSString *dm = [res stringByAppendingPathComponent:@"dm"];
    setenv("DM_NVN_SHMAP", [dm stringByAppendingPathComponent:@"shader_map.tsv"].fileSystemRepresentation, 0);
    setenv("DM_MSL_DIR", [res stringByAppendingPathComponent:@"msl"].fileSystemRepresentation, 0);
    setenv("DM_FONT_DIR", [dm stringByAppendingPathComponent:@"font"].fileSystemRepresentation, 0);   // 共享字体替身（standard.ttf / nintendo_ext.ttf）：iOS 上没有 macOS 的字体路径 | Shared-font stand-ins (standard.ttf / nintendo_ext.ttf): iOS lacks the macOS font path
    chdir(doc.fileSystemRepresentation);
    g_argv[0] = strdup("dmhost"); g_argv[1] = strdup(dm.fileSystemRepresentation); g_argc = 2;
    if (getenv("ACNH_DIAGNOSTICS")) {
        setenv("ACNH_AUDIO_WAV", [doc stringByAppendingPathComponent:@"dialogue-audio.wav"].fileSystemRepresentation, 0);
        setenv("DM_FSLOG", [doc stringByAppendingPathComponent:@"startup-files.tsv"].fileSystemRepresentation, 0);
    }
    dm_host_preload_start(g_argc, g_argv);   // 读模块表 / 重定位 / 接管提前在后台跑，与 UIKit 启动并行 | Module tables / relocation / takeover run early in the background, in parallel with UIKit startup
    return 1;
}
static int g_boot_ok;
// Direct .data mount: preparation only validates/maps the index and opens parts.
void front_ios_prepare_resources(void (^progress)(double), void (^completion)(NSString *)) {
    dispatch_async(dispatch_get_global_queue(QOS_CLASS_USER_INITIATED,0), ^{
        NSString *folder=data_locate();NSString *failure=g_resource_failure;
        if(!folder)failure=@"请把 ACNH-index.data 和全部 ACNH-001.data～ACNH-010.data 放到动森数据容器的 Documents（也支持其中的 ACNHMobile 文件夹）。\n本版直接读取数据包，不解压，不需要 acnh-pack.json。";
        else {failure=acnh_mount_resources(folder);if(!failure){g_boot_ok=boot_env();if(!g_boot_ok)failure=@"数据包已挂载，但启动环境未就绪";}}
        if(progress)progress(failure?0:1);
        dispatch_async(dispatch_get_main_queue(),^{completion(failure);});
    });
}

static void *boot_thread(void *p) {
    (void)p;
    static char alt[65536]; stack_t ss = {.ss_sp = alt, .ss_size = sizeof alt}; sigaltstack(&ss, 0);   // 崩溃打印要在宿主线程上也生效 | crash printing must work on the host thread too
    int rc = dm_main(g_argc, g_argv);
    fprintf(stderr, "boot: dm_main 返回 %d\n", rc);
    return NULL;
}
void front_ios_boot(void) {   // 窗口与 Metal 视图建好后由 front_ios.m 调 | Called by front_ios.m once the window and Metal view exist
    static int once; if (!g_boot_ok || once) return; once=1;
    void front_ios_audio_session(void); front_ios_audio_session();   // 游戏开声卡之前把 I/O 缓冲设短（front_ios.m） | set a short I/O buffer before the game opens audio output (front_ios.m)
    pthread_attr_t a; pthread_attr_init(&a); pthread_attr_setstacksize(&a, 64u << 20);   // Switch 主线程栈由 npdm 定，宿主给足 | the Switch main thread stack is set by npdm; the host gives plenty
    pthread_t t; pthread_create(&t, &a, boot_thread, NULL); pthread_detach(t);
    // 独立于帧数的内存诊断，系统突然终止时仍留下最近读数。 / Memory diagnostics independent of frame progress.
    dispatch_async(dispatch_get_global_queue(QOS_CLASS_UTILITY,0), ^{
        for(int i=0;i<120;i++) {
            usleep(250000);
            task_vm_info_data_t info={0};mach_msg_type_number_t count=TASK_VM_INFO_COUNT;
            if(task_info(mach_task_self(),TASK_VM_INFO,(task_info_t)&info,&count)==KERN_SUCCESS) {
                extern unsigned long long mtl_gpu_allocated_mb(void);
                fprintf(stderr,"[acnh-memory] ms=%d footprint=%lluMB available=%lluMB gpu=%lluMB\n",(i+1)*250,(unsigned long long)(info.phys_footprint>>20),(unsigned long long)(os_proc_available_memory()>>20),mtl_gpu_allocated_mb());
            }
        }
    });

    // 真机启动诊断：只记录现场，不结束进程。 / Device startup diagnostics: capture state without terminating.
    for(NSNumber *seconds in @[@30,@60,@120]) {
        dispatch_after(dispatch_time(DISPATCH_TIME_NOW,seconds.longLongValue*NSEC_PER_SEC), dispatch_get_global_queue(QOS_CLASS_UTILITY,0), ^{
            extern void crash_dump_now(const char *);
            char reason[80];snprintf(reason,sizeof reason,"iOS startup +%ds",seconds.intValue);crash_dump_now(reason);
        });
    }

}
int front_ios_boot_ok(void) { return g_boot_ok; }

@interface App : UIResponder <UIApplicationDelegate>
@end
@implementation App
- (UISceneConfiguration *)application:(UIApplication *)a configurationForConnectingSceneSession:(UISceneSession *)s options:(UISceneConnectionOptions *)o {
    return [[UISceneConfiguration alloc] initWithName:@"Default" sessionRole:s.role];
}
@end

// 场景生命周期：建窗口 + 焦点事件交 front_ios.m | Scene lifecycle: create the window + hand focus events to front_ios.m
@interface SceneDelegate : UIResponder <UIWindowSceneDelegate>
@property (strong, nonatomic) UIWindow *window;
@end
@implementation SceneDelegate
- (void)scene:(UIScene *)scene willConnectToSession:(UISceneSession *)session options:(UISceneConnectionOptions *)o {
    self.window = [[UIWindow alloc] initWithWindowScene:(UIWindowScene *)scene];
    self.window.rootViewController = front_ios_root();
    [self.window makeKeyAndVisible];
}
- (void)sceneWillResignActive:(UIScene *)s { front_ios_focus_event(0); }
- (void)sceneDidEnterBackground:(UIScene *)s { front_ios_focus_event(1); }
- (void)sceneWillEnterForeground:(UIScene *)s { front_ios_focus_event(2); }
- (void)sceneDidBecomeActive:(UIScene *)s { front_ios_focus_event(3); }
@end

int main(int argc, char **argv) {
    @autoreleasepool { g_boot_ok = boot_env(); }
    @autoreleasepool { return UIApplicationMain(argc, argv, nil, NSStringFromClass(App.class)); }
}
