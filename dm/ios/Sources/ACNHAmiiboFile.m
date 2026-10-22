#import <UIKit/UIKit.h>
#import <UniformTypeIdentifiers/UniformTypeIdentifiers.h>
#include "dm.h"
#include "front.h"
#include "amiibo_file.h"
#include "hle.h"
#include "AmiiboImportWait.h"

static UIViewController *amiiboVC(void) {
    for(UIScene *s in UIApplication.sharedApplication.connectedScenes){
        if(s.activationState!=UISceneActivationStateForegroundActive || ![s isKindOfClass:UIWindowScene.class])continue;
        for(UIWindow *w in ((UIWindowScene *)s).windows)if(w.isKeyWindow){UIViewController *v=w.rootViewController;while(v.presentedViewController)v=v.presentedViewController;return v;}
    }return nil;
}
@interface ACNHAmiiboFile : NSObject<UIDocumentPickerDelegate>
@property(nonatomic) uint64_t token;
@property(nonatomic) BOOL closed;
@property(nonatomic) AmiiboImportWait wait;
@property(nonatomic,strong) UIViewController *panel;
- (void)choose;
- (void)pick;
- (void)close;
@end
static ACNHAmiiboFile *currentImport;
@implementation ACNHAmiiboFile
- (BOOL)active {return !self.closed && currentImport==self;}
- (void)show:(UIAlertController *)a {
    if(!self.active)return;UIViewController *vc=amiiboVC();if(!vc){[self resume];return;}
    // Alert style works on both iPhone and iPad without a popover anchor.
    self.panel=a;[vc presentViewController:a animated:YES completion:nil];
}
- (void)finishPanel:(dispatch_block_t)next {
    UIViewController *p=self.panel;self.panel=nil;
    if(p.presentingViewController)[p dismissViewControllerAnimated:YES completion:^{if(self.active)next();}];
    else if(self.active)next();
}
- (void)resume { AmiiboImportWait wait=self.wait;if(wait.held)LOG("[amiibo-file] selection pause ended");amiibo_import_wait_end(&wait);self.wait=wait; }
- (void)choose {
    AmiiboImportWait wait=self.wait;amiibo_import_wait_begin(&wait);self.wait=wait;LOG("[amiibo-file] selection pause active: clock + frame submission");
    UIAlertController *a=[UIAlertController alertControllerWithTitle:@"读取 amiibo" message:@"选择你自己的 amiibo .bin 文件，无需 NFC 权限。选择期间游戏计时暂停，不会因等待过久退出。只读导入，不修改原文件。" preferredStyle:UIAlertControllerStyleAlert];
    [a addAction:[UIAlertAction actionWithTitle:@"导入 amiibo 文件" style:UIAlertActionStyleDefault handler:^(UIAlertAction *x){[self finishPanel:^{[self pick];}];}]];
    [a addAction:[UIAlertAction actionWithTitle:@"实体 NFC 通道测试" style:UIAlertActionStyleDefault handler:^(UIAlertAction *x){[self finishPanel:^{[self resume];front_nfc_probe_start();}];}]];
    [a addAction:[UIAlertAction actionWithTitle:@"取消" style:UIAlertActionStyleCancel handler:^(UIAlertAction *x){self.panel=nil;[self resume];LOG("[amiibo-file] choice cancelled");}]];
    [self show:a];
}
- (void)pick {
    if(!self.active)return;UIViewController *vc=amiiboVC();if(!vc){[self resume];return;}
    UIDocumentPickerViewController *p=[[UIDocumentPickerViewController alloc] initForOpeningContentTypes:@[UTTypeData] asCopy:NO];
    p.delegate=self;p.allowsMultipleSelection=NO;self.panel=p;[vc presentViewController:p animated:YES completion:nil];
}
- (void)failed:(NSString *)message {
    if(!self.active)return;
    UIAlertController *a=[UIAlertController alertControllerWithTitle:@"未导入 amiibo" message:message preferredStyle:UIAlertControllerStyleAlert];
    [a addAction:[UIAlertAction actionWithTitle:@"重新选择" style:UIAlertActionStyleDefault handler:^(UIAlertAction *x){[self finishPanel:^{[self pick];}];}]];
    [a addAction:[UIAlertAction actionWithTitle:@"取消" style:UIAlertActionStyleCancel handler:^(UIAlertAction *x){self.panel=nil;[self resume];}]];[self show:a];
}
- (void)documentPickerWasCancelled:(UIDocumentPickerViewController *)controller {
    if(self.panel==controller)self.panel=nil;[self resume];LOG("[amiibo-file] picker cancelled");
}
- (void)documentPicker:(UIDocumentPickerViewController *)controller didPickDocumentsAtURLs:(NSArray<NSURL *> *)urls {
    if(!self.active || urls.count!=1)return;NSURL *url=urls.firstObject;
    [self finishPanel:^{[self readURL:url];}];
}
- (void)readURL:(NSURL *)url {
    if(!self.active)return;
    // Read on a worker: cloud providers may block. Bound the read even if a
    // provider reports a false length. No copy is stored, no writes are made.
    dispatch_async(dispatch_get_global_queue(QOS_CLASS_USER_INITIATED,0),^{
        BOOL scope=[url startAccessingSecurityScopedResource];
        __block NSData *bytes=nil;__block BOOL ioError=NO;
        NSFileCoordinator *coordinator=[[NSFileCoordinator alloc] initWithFilePresenter:nil];NSError *coordinateError=nil;
        [coordinator coordinateReadingItemAtURL:url options:NSFileCoordinatorReadingWithoutChanges error:&coordinateError byAccessor:^(NSURL *readURL){
            NSError *error=nil;NSFileHandle *h=[NSFileHandle fileHandleForReadingFromURL:readURL error:&error];
            @try {if(h)bytes=[h readDataUpToLength:573 error:&error];} @catch(NSException *e){ioError=YES;}
            [h closeAndReturnError:nil];if(error || !h)ioError=YES;
        }];
        if(scope)[url stopAccessingSecurityScopedResource];
        dispatch_async(dispatch_get_main_queue(),^{
            if(!self.active)return;
            if(ioError || coordinateError || !bytes){LOG("[amiibo-file] provider read failed");[self failed:@"文件读取失败。请先在“文件”App 中下载到本机，再重新选择。"];return;}
            int result=hle_nfp_import_file(self.token,bytes.bytes,bytes.length);
            LOG("[amiibo-file] import bytes=%lu result=%d",(unsigned long)bytes.length,result);
            if(result==-2)[self failed:@"文件格式不符合原始 NTAG215 amiibo 数据。支持 532、540 或 572 字节的 .bin；不支持压缩包、密钥文件或文本格式。\n\n原文件未修改。"]; 
            if(result!=-2)[self resume];
            // -1 means the game stopped/restarted scanning; silently discard.
        });
    });
}
- (void)close {
    self.closed=YES;[self resume];UIViewController *p=self.panel;self.panel=nil;
    if(p.presentingViewController)[p dismissViewControllerAnimated:NO completion:nil];
}
@end
void front_amiibo_file_start(uint64_t token) {
    dispatch_async(dispatch_get_main_queue(),^{
        [currentImport close];ACNHAmiiboFile *p=[ACNHAmiiboFile new];p.token=token;currentImport=p;
        [p choose];
    });
}
void front_amiibo_file_stop(void) {
    dispatch_async(dispatch_get_main_queue(),^{[currentImport close];currentImport=nil;});
}
