#import <UIKit/UIKit.h>
#import <Photos/Photos.h>
#import <Security/Security.h>
#include <dlfcn.h>
#include "dm.h"
#include "PhotoExportCleanup.h"
#include "PhotoHostPurpose.h"
#import <objc/message.h>

// All state stays on the main queue. Keep file URLs, never full-resolution images.
static NSMutableArray<NSURL *> *pendingPhotos;
static BOOL exportingPhoto, requestingPhotoPermission;
static void photoNotice(NSString *text) {
    for(UIScene *scene in UIApplication.sharedApplication.connectedScenes) {
        if(scene.activationState!=UISceneActivationStateForegroundActive || ![scene isKindOfClass:UIWindowScene.class])continue;
        for(UIWindow *window in ((UIWindowScene *)scene).windows)if(window.isKeyWindow) {
            UILabel *label=[[UILabel alloc] initWithFrame:CGRectMake(20,window.safeAreaInsets.top+16,window.bounds.size.width-40,48)];
            label.text=text;label.textAlignment=NSTextAlignmentCenter;label.textColor=UIColor.whiteColor;
            label.backgroundColor=[UIColor.blackColor colorWithAlphaComponent:0.75];label.font=[UIFont systemFontOfSize:14];
            label.numberOfLines=2;label.layer.cornerRadius=12;label.clipsToBounds=YES;label.userInteractionEnabled=NO;
            [window addSubview:label];dispatch_after(dispatch_time(DISPATCH_TIME_NOW,3*NSEC_PER_SEC),dispatch_get_main_queue(),^{[label removeFromSuperview];});return;
        }
    }
}
static BOOL photoHostHasPurpose(void) {
    // LiveContainer can replace mainBundle. TCC checks the actual signed host.
    static void *security;static dispatch_once_t once;
    dispatch_once(&once,^{security=dlopen("/System/Library/Frameworks/Security.framework/Security",RTLD_NOW|RTLD_LOCAL);});
    if(!security){LOG("[photo-library] Security framework unavailable");return NO;}
    CFTypeRef (*create)(CFAllocatorRef)=dlsym(security,"SecTaskCreateFromSelf");
    CFStringRef (*signingID)(CFTypeRef,CFErrorRef *)=dlsym(security,"SecTaskCopySigningIdentifier");
    if(!create || !signingID){LOG("[photo-library] host signature API unavailable");return NO;}
    CFTypeRef task=create(kCFAllocatorDefault);if(!task){LOG("[photo-library] host signature task unavailable");return NO;}
    NSString *identifier=CFBridgingRelease(signingID(task,NULL));CFRelease(task);
    NSMutableArray<NSBundle *> *candidates=[NSMutableArray array];
    // LiveContainer retains its original bundle here before substituting mainBundle.
    SEL selector=NSSelectorFromString(@"lcMainBundle");
    if([NSUserDefaults respondsToSelector:selector]){
        id retained=((id(*)(id,SEL))objc_msgSend)(NSUserDefaults.class,selector);
        if([retained isKindOfClass:NSBundle.class])[candidates addObject:retained];
    }
    NSBundle *registered=identifier.length?[NSBundle bundleWithIdentifier:identifier]:nil;
    if(registered)[candidates addObject:registered];
    [candidates addObject:NSBundle.mainBundle];
    for(NSBundle *candidate in candidates){
        if(photo_host_purpose(candidate.bundlePath,identifier)){
            LOG("[photo-library] verified host purpose from retained/registered bundle");return YES;
        }
    }
    LOG("[photo-library] host signature identified=%d verified purpose missing",identifier.length>0);
    return NO;
}
static void drainPhotos(void) {
    if(exportingPhoto || requestingPhotoPermission || !pendingPhotos.count)return;
    PHAuthorizationStatus status=[PHPhotoLibrary authorizationStatusForAccessLevel:PHAccessLevelAddOnly];
    LOG("[photo-library] add authorization status=%ld",(long)status);
    if(status==PHAuthorizationStatusNotDetermined) {
        requestingPhotoPermission=YES;
        [PHPhotoLibrary requestAuthorizationForAccessLevel:PHAccessLevelAddOnly handler:^(PHAuthorizationStatus result){
            dispatch_async(dispatch_get_main_queue(),^{requestingPhotoPermission=NO;drainPhotos();});
        }];return;
    }
    if(status!=PHAuthorizationStatusAuthorized && status!=PHAuthorizationStatusLimited) {
        [pendingPhotos removeAllObjects];LOG("[photo-library] add permission denied/restricted; local PNG retained");
        photoNotice(@"照片已存到应用内；允许添加照片后可保存到相册。");return;
    }
    NSURL *url=pendingPhotos.firstObject;[pendingPhotos removeObjectAtIndex:0];exportingPhoto=YES;
    __block BOOL created=NO;
    [PHPhotoLibrary.sharedPhotoLibrary performChanges:^{
        created=[PHAssetChangeRequest creationRequestForAssetFromImageAtFileURL:url]!=nil;
    } completionHandler:^(BOOL success,NSError *error){
        dispatch_async(dispatch_get_main_queue(),^{
            exportingPhoto=NO;
            BOOL saved=success && created;
            NSError *cleanupError=nil;BOOL removed=photo_export_cleanup(url,saved,&cleanupError);
            LOG("[photo-library] export %s domain=%s code=%ld; local PNG %s",saved?"saved":"failed",error.domain.UTF8String?:"none",(long)error.code,removed?"removed":"retained");
            if(saved && !removed)LOG("[photo-library] local cleanup failed code=%ld",(long)cleanupError.code);
            photoNotice(saved?(removed?@"照片已保存到手机相册":@"照片已存入相册，应用内副本未能清理。"):@"相册保存失败，照片已保留在应用内。");drainPhotos();
        });
    }];
}
void front_photo_saved(const char *path) {
    if(!path || !*path)return;
    NSString *file=[[NSString alloc] initWithUTF8String:path];if(!file)return;
    dispatch_async(dispatch_get_main_queue(),^{
        if(!photoHostHasPurpose()) {
            LOG("[photo-library] actual host purpose unavailable; skipped authorization; local PNG retained");
            photoNotice(@"照片已存到应用内；当前运行宿主未提供相册权限说明。");return;
        }
        if(!pendingPhotos)pendingPhotos=[NSMutableArray new];
        if(pendingPhotos.count>=8){LOG("[photo-library] export queue full; local PNG retained");return;}
        [pendingPhotos addObject:[NSURL fileURLWithPath:file]];drainPhotos();
    });
}
