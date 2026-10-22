#import "ACNHResourceGate.h"
#import <UniformTypeIdentifiers/UniformTypeIdentifiers.h>
#import <CommonCrypto/CommonDigest.h>
#include "../../host/acnh_data.h"
static void (^verifyProgress)(double);
static void reportVerify(double p){if(verifyProgress)verifyProgress(p);}
@interface ACNHResourceGate : NSObject<UIDocumentPickerDelegate>
@property(nonatomic,weak) UIViewController *vc;
@property(nonatomic,copy) void (^progress)(double);
@property(nonatomic,copy) void (^completion)(NSString *,NSString *);
- (void)selectFolder;
- (void)check:(NSURL *)url importing:(BOOL)importing;
@end
static ACNHResourceGate *gate;
@implementation ACNHResourceGate
- (void)selectFolder {
    UIDocumentPickerViewController *picker=[[UIDocumentPickerViewController alloc] initForOpeningContentTypes:@[UTTypeFolder] asCopy:NO];
    picker.delegate=self;picker.allowsMultipleSelection=NO;[self.vc presentViewController:picker animated:YES completion:nil];
}
- (void)failed:(NSString *)why {
    UIAlertController *a=[UIAlertController alertControllerWithTitle:@"游戏数据未就绪" message:why preferredStyle:UIAlertControllerStyleAlert];
    [a addAction:[UIAlertAction actionWithTitle:@"选择 .data 文件夹" style:UIAlertActionStyleDefault handler:^(UIAlertAction *x){[self selectFolder];}]];
    [a addAction:[UIAlertAction actionWithTitle:@"取消" style:UIAlertActionStyleCancel handler:^(UIAlertAction *x){self.completion(nil,why);}]];
    [self.vc presentViewController:a animated:YES completion:nil];
}
- (void)documentPickerWasCancelled:(UIDocumentPickerViewController *)controller {[self failed:@"请选择包含 ACNH-index.data 和所有分卷的文件夹，或把它们放到本应用 Documents/ACNHMobile。"] ;}
- (void)documentPicker:(UIDocumentPickerViewController *)controller didPickDocumentsAtURLs:(NSArray<NSURL *> *)urls {if(urls.count)[self check:urls.firstObject importing:YES];}
- (void)check:(NSURL *)url importing:(BOOL)importing {
    BOOL scoped=[url startAccessingSecurityScopedResource];
    dispatch_async(dispatch_get_global_queue(QOS_CLASS_USER_INITIATED,0),^{@autoreleasepool{
        NSFileManager *fm=NSFileManager.defaultManager;NSString *folder=url.path;NSString *failure=nil;NSError *error=nil;
        NSString *doc=NSSearchPathForDirectoriesInDomains(NSDocumentDirectory,NSUserDomainMask,YES).firstObject;
        if(importing){
            // Copy only index and raw parts to an isolated staging directory; never extract.
            NSString *stage=[doc stringByAppendingPathComponent:[@".ACNHMobile.import-" stringByAppendingString:NSUUID.UUID.UUIDString]];
            if(![fm createDirectoryAtPath:stage withIntermediateDirectories:NO attributes:nil error:&error])failure=error.localizedDescription;
            NSArray<NSURL *> *files=[fm contentsOfDirectoryAtURL:url includingPropertiesForKeys:@[NSURLIsRegularFileKey,NSURLIsSymbolicLinkKey] options:0 error:&error];
            if(!files)failure=error.localizedDescription;
            NSMutableArray<NSURL *> *parts=[NSMutableArray new];
            for(NSURL *f in files)if([f.pathExtension.lowercaseString isEqualToString:@"data"]){
                NSNumber *regular=nil,*link=nil;[f getResourceValue:&regular forKey:NSURLIsRegularFileKey error:nil];[f getResourceValue:&link forKey:NSURLIsSymbolicLinkKey error:nil];
                if(!regular.boolValue || link.boolValue){failure=@"数据文件不能是链接或文件夹。";break;}[parts addObject:f];
            }
            if(!parts.count || parts.count>101)failure=@"数据分卷数量无效，请选择正确的文件夹。";
            for(NSUInteger i=0;!failure && i<parts.count;i++){
                NSURL *f=parts[i];NSString *target=[stage stringByAppendingPathComponent:f.lastPathComponent];
                if(![fm copyItemAtPath:f.path toPath:target error:&error])failure=error.localizedDescription;
                dispatch_async(dispatch_get_main_queue(),^{self.progress(0.5*(double)(i+1)/parts.count);});
            }
            folder=stage;
        }
        NSData *index=[NSData dataWithContentsOfFile:[folder stringByAppendingPathComponent:@"ACNH-index.data"] options:NSDataReadingMappedIfSafe error:nil];
        unsigned char hash[CC_SHA256_DIGEST_LENGTH]={0};if(index)CC_SHA256(index.bytes,(CC_LONG)index.length,hash);
        NSMutableString *fingerprint=[NSMutableString new];for(int i=0;i<32;i++)[fingerprint appendFormat:@"%02x",hash[i]];
        NSString *verifiedKey=[@"ACNHVerifiedIndex:" stringByAppendingString:folder];
        BOOL deep=importing || ![NSUserDefaults.standardUserDefaults stringForKey:verifiedKey] || ![[NSUserDefaults.standardUserDefaults stringForKey:verifiedKey] isEqualToString:fingerprint];
        if(!failure){
            verifyProgress=^(double value){dispatch_async(dispatch_get_main_queue(),^{self.progress(importing?0.5+0.5*value:value);});};
            char why[256]={0};if(!dm_acnh_data_verify(folder.fileSystemRepresentation,deep,reportVerify,why,sizeof why))failure=@(why);verifyProgress=nil;
        }
        if(!failure && importing){
            NSString *final=[doc stringByAppendingPathComponent:@"ACNHMobile"];
            if([fm fileExistsAtPath:final]){
                NSString *backup=[doc stringByAppendingPathComponent:[@".ACNHMobile.previous-" stringByAppendingString:NSUUID.UUID.UUIDString]];
                if(![fm moveItemAtPath:final toPath:backup error:&error])failure=error.localizedDescription;
            }
            if(!failure && ![fm moveItemAtPath:folder toPath:final error:&error])failure=error.localizedDescription;
            if(!failure){folder=final;verifiedKey=[@"ACNHVerifiedIndex:" stringByAppendingString:folder];}
        }
        if(!failure){
            [NSUserDefaults.standardUserDefaults setObject:fingerprint forKey:verifiedKey];
            NSURL *dataURL=[NSURL fileURLWithPath:folder isDirectory:YES];[dataURL setResourceValue:@YES forKey:NSURLIsExcludedFromBackupKey error:nil];
        }
        if(scoped)[url stopAccessingSecurityScopedResource];
        dispatch_async(dispatch_get_main_queue(),^{if(failure)[self failed:failure];else{self.progress(1);self.completion(folder,nil);}});
    }});
}
@end
void acnh_prepare_resources(UIViewController *vc,void (^progress)(double),void (^completion)(NSString *,NSString *)) {
    gate=[ACNHResourceGate new];gate.vc=vc;gate.progress=progress;gate.completion=completion;
    NSString *doc=NSSearchPathForDirectoriesInDomains(NSDocumentDirectory,NSUserDomainMask,YES).firstObject;
    for(NSString *folder in @[[doc stringByAppendingPathComponent:@"ACNHMobile"],doc]){
        if([NSFileManager.defaultManager fileExistsAtPath:[folder stringByAppendingPathComponent:@"ACNH-index.data"]]){[gate check:[NSURL fileURLWithPath:folder] importing:NO];return;}
    }
    [gate failed:@"请导入自己的动森 .data 资源包。数据包将直接读取，不会解压。"];
}
