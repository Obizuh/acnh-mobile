#import <UIKit/UIKit.h>
#import <CoreNFC/CoreNFC.h>
#import <Security/Security.h>
#include <dlfcn.h>
#include "dm.h"
#include "front.h"

// SecTask inspects the running process's signature, including under LiveContainer.
// Resolve optionally because its declarations are not in the iPhone SDK headers.
static NSInteger probeEntitlement(NSString **identifier) {
    static void *security;
    static dispatch_once_t once;
    dispatch_once(&once,^{security=dlopen("/System/Library/Frameworks/Security.framework/Security",RTLD_NOW|RTLD_LOCAL);});
    if(!security)return -1;
    CFTypeRef (*create)(CFAllocatorRef)=dlsym(security,"SecTaskCreateFromSelf");
    CFTypeRef (*value)(CFTypeRef,CFStringRef,CFErrorRef *)=dlsym(security,"SecTaskCopyValueForEntitlement");
    CFStringRef (*signingID)(CFTypeRef,CFErrorRef *)=dlsym(security,"SecTaskCopySigningIdentifier");
    if(!create || !value)return -1;
    CFTypeRef task=create(kCFAllocatorDefault);if(!task)return -1;
    if(signingID && identifier)*identifier=CFBridgingRelease(signingID(task,NULL));
    CFErrorRef error=NULL;
    id formats=CFBridgingRelease(value(task,CFSTR("com.apple.developer.nfc.readersession.formats"),&error));
    BOOL ok=[formats isKindOfClass:NSArray.class] && [formats containsObject:@"TAG"];
    // Never log card IDs, payloads, credentials, or complete entitlements.
    BOOL unknown=error && CFErrorGetCode(error)!=-34018;
    if(error){LOG("[nfc-probe] entitlement query error=%ld",(long)CFErrorGetCode(error));CFRelease(error);}
    CFRelease(task);
    return ok?1:unknown?-1:0;
}
static UIViewController *probeVC(void) {
    for(UIScene *scene in UIApplication.sharedApplication.connectedScenes){
        if(scene.activationState!=UISceneActivationStateForegroundActive || ![scene isKindOfClass:UIWindowScene.class])continue;
        for(UIWindow *w in ((UIWindowScene *)scene).windows)if(w.isKeyWindow){
            UIViewController *v=w.rootViewController;
            while(v.presentedViewController)v=v.presentedViewController;
            return v;
        }
    }
    return nil;
}
static void probeMessage(NSString *message) {
    UIViewController *vc=probeVC();if(!vc || [vc isKindOfClass:UIAlertController.class])return;
    UIAlertController *a=[UIAlertController alertControllerWithTitle:@"NFC 读卡测试" message:message preferredStyle:UIAlertControllerStyleAlert];
    [a addAction:[UIAlertAction actionWithTitle:@"好" style:UIAlertActionStyleDefault handler:nil]];
    [vc presentViewController:a animated:YES completion:nil];
}
@interface ACNHNFCProbe : NSObject<NFCTagReaderSessionDelegate>
@property(nonatomic,strong) NFCTagReaderSession *session;
@property(nonatomic) BOOL requested,reading;
@property(nonatomic,copy) NSString *result;
@end
@implementation ACNHNFCProbe
+ (instancetype)shared {static ACNHNFCProbe *p;static dispatch_once_t once;dispatch_once(&once,^{p=[ACNHNFCProbe new];});return p;}
- (void)start {
    if(self.requested)return;self.requested=YES;
    const char *e=getenv("ACNH_NFC_PROBE");if(!e || strcmp(e,"1"))return;
    if(UIApplication.sharedApplication.applicationState!=UIApplicationStateActive){LOG("[nfc-probe] skipped: app not active");return;}
    NSString *identifier=nil;NSInteger permission=probeEntitlement(&identifier);
    LOG("[nfc-probe] process=%s TAG-entitlement=%ld readingAvailable=%d",identifier.UTF8String?:"unknown",(long)permission,NFCTagReaderSession.readingAvailable);
    if(permission!=1){
        probeMessage(permission==0?@"当前运行进程没有 NFC（TAG）签名权限，不能启动实体卡扫描。\n\n直接安装时需要游戏的签名包含权限；通过 LiveContainer 运行时需要容器本体具备权限。\n\n可以退出扫描后重进，选择“导入 amiibo 文件”，不需要 NFC 权限。结果已记入 logs/log.txt。":@"未能确认当前运行宿主的 NFC 签名权限，本次没有启动扫描。结果已记入 logs/log.txt。");return;
    }
    if(!NFCTagReaderSession.readingAvailable){probeMessage(@"当前设备或系统不提供 NFC 标签读取。结果已记入 logs/log.txt。");return;}
    // LiveContainer replaces mainBundle with the guest. Check the actual signing
    // identity's bundle too: a guest purpose string cannot configure the host.
    NSBundle *host=identifier.length?[NSBundle bundleWithIdentifier:identifier]:nil;
    if(!host && [NSBundle.mainBundle.bundleIdentifier isEqualToString:identifier])host=NSBundle.mainBundle;
    NSString *purpose=[host objectForInfoDictionaryKey:@"NFCReaderUsageDescription"];
    if(![purpose isKindOfClass:NSString.class] || !purpose.length){LOG("[nfc-probe] host purpose missing or host bundle unknown");probeMessage(@"未能确认运行宿主包含 NFC 用途说明，未启动扫描。需要宿主同时具备 NFC 签名权限和用途说明。结果已记入 logs/log.txt。");return;}
    self.result=nil;self.reading=NO;
    @try{
        self.session=[[NFCTagReaderSession alloc] initWithPollingOption:NFCPollingISO14443 delegate:self queue:dispatch_get_main_queue()];
        self.session.alertMessage=@"请将一张卡片靠近 iPhone 顶部。仅做只读测试，暂不用于游戏召唤。";
        [self.session beginSession];LOG("[nfc-probe] beginSession requested");
    }@catch(NSException *exception){self.session=nil;LOG("[nfc-probe] session exception=%s",exception.name.UTF8String);probeMessage(@"系统未能开启 NFC 扫描，结果已记入 logs/log.txt。");}
}
- (void)stop {
    self.requested=NO;self.reading=NO;self.result=nil;
    NFCTagReaderSession *s=self.session;self.session=nil;
    if(s){[s invalidateSession];LOG("[nfc-probe] stopped by game");}
}
- (void)tagReaderSessionDidBecomeActive:(NFCTagReaderSession *)session {
    if(session==self.session)LOG("[nfc-probe] reader active");
}
- (void)tagReaderSession:(NFCTagReaderSession *)session didInvalidateWithError:(NSError *)error {
    if(session!=self.session)return;
    NSString *result=self.result;self.session=nil;self.reading=NO;self.result=nil;
    LOG("[nfc-probe] session ended domain=%s code=%ld completed=%d",error.domain.UTF8String,(long)error.code,result!=nil);
    NSString *message=result;
    if(!message && error.code!=NFCReaderSessionInvalidationErrorUserCanceled){
        message=[NSString stringWithFormat:@"系统未完成 NFC 读卡（错误 %ld）。\n\n结果已记入 logs/log.txt；这不是游戏已识别 amiibo。",(long)error.code];
    }
    if(message)dispatch_after(dispatch_time(DISPATCH_TIME_NOW,400*NSEC_PER_MSEC),dispatch_get_main_queue(),^{if(self.requested)probeMessage(message);});
}
- (void)tagReaderSession:(NFCTagReaderSession *)session didDetectTags:(NSArray<__kindof id<NFCTag>> *)tags {
    if(session!=self.session || self.reading)return;
    LOG("[nfc-probe] detected count=%lu",(unsigned long)tags.count);
    if(tags.count!=1){session.alertMessage=@"请只放置一张卡片，再重试。";[session restartPolling];return;}
    id<NFCTag> tag=tags.firstObject;
    if(tag.type!=NFCTagTypeMiFare || tag.asNFCMiFareTag.mifareFamily!=NFCMiFareUltralight){
        self.result=@"已检测到 NFC 标签，但类型不符合本次 amiibo 测试的读取方式。未写入卡片。";
        LOG("[nfc-probe] unsupported tag type=%ld",(long)tag.type);[session invalidateSession];return;
    }
    self.reading=YES;
    [session connectToTag:tag completionHandler:^(NSError *error){dispatch_async(dispatch_get_main_queue(),^{
        if(self.session!=session)return;
        if(error){LOG("[nfc-probe] connect error domain=%s code=%ld",error.domain.UTF8String,(long)error.code);self.result=@"检测到标签，但连接失败。请保持卡片贴近手机顶部后重试。";[session invalidateSession];return;}
        const uint8_t command[2]={0x30,0x00}; // READ pages 0-3; never a write command.
        [tag.asNFCMiFareTag sendMiFareCommand:[NSData dataWithBytes:command length:2] completionHandler:^(NSData *data,NSError *readError){dispatch_async(dispatch_get_main_queue(),^{
            if(self.session!=session)return;
            LOG("[nfc-probe] read test bytes=%lu error-domain=%s code=%ld",(unsigned long)data.length,readError.domain.UTF8String?:"none",(long)readError.code);
            self.result=(!readError && data.length==16)?@"实体 NFC 读卡通道测试成功：已读取16字节测试页。\n\n本测试未写卡，也尚未把数据传给游戏；还不能用它召唤角色。":@"检测到标签，但未完成测试页读取。请把 logs/log.txt 发来排查。";
            [session invalidateSession];
        });}];
    });}];
}
@end
void front_nfc_probe_start(void){dispatch_async(dispatch_get_main_queue(),^{[[ACNHNFCProbe shared] start];});}
void front_nfc_probe_stop(void){dispatch_async(dispatch_get_main_queue(),^{[[ACNHNFCProbe shared] stop];});}
