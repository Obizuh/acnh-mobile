#import "../Sources/PhotoHostPurpose.h"
#include <assert.h>
int main(void){@autoreleasepool{
 NSString *dir=[NSTemporaryDirectory() stringByAppendingPathComponent:NSUUID.UUID.UUIDString];
 [NSFileManager.defaultManager createDirectoryAtPath:dir withIntermediateDirectories:YES attributes:nil error:nil];
 NSString *p=[dir stringByAppendingPathComponent:@"Info.plist"];
 [@{@"CFBundleIdentifier":@"host",@"NSPhotoLibraryAddUsageDescription":@"Save photos"} writeToFile:p atomically:YES];
 assert(photo_host_purpose(dir,@"host"));assert(!photo_host_purpose(dir,@"guest"));
 [@{@"CFBundleIdentifier":@"host",@"NSPhotoLibraryAddUsageDescription":@""} writeToFile:p atomically:YES];assert(!photo_host_purpose(dir,@"host"));
 [@{@"CFBundleIdentifier":@"host",@"NSPhotoLibraryAddUsageDescription":@1} writeToFile:p atomically:YES];assert(!photo_host_purpose(dir,@"host"));
 assert(!photo_host_purpose(nil,@"host"));assert(!photo_host_purpose(dir,nil));
 [NSFileManager.defaultManager removeItemAtPath:dir error:nil];puts("host plist identity and purpose checks passed");
}}
