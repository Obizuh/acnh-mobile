#import <Foundation/Foundation.h>
#include <assert.h>
#include "../../ios/Sources/PhotoExportCleanup.h"
int main(void){@autoreleasepool{
 NSFileManager *fm=NSFileManager.defaultManager;
 NSURL *root=[NSURL fileURLWithPath:[NSTemporaryDirectory() stringByAppendingPathComponent:NSUUID.UUID.UUIDString]];
 assert([fm createDirectoryAtURL:root withIntermediateDirectories:YES attributes:nil error:nil]);
 NSURL *shot=[root URLByAppendingPathComponent:@"shot.png"],*old=[root URLByAppendingPathComponent:@"old.png"];
 NSData *data=[@"photo fixture" dataUsingEncoding:NSUTF8StringEncoding];
 assert([data writeToURL:shot atomically:YES] && [data writeToURL:old atomically:YES]);
 NSError *error=nil;
 assert(!photo_export_cleanup(shot,NO,&error)); // denied or failed Photos commit
 assert([[NSData dataWithContentsOfURL:shot] isEqualToData:data] && !error);
 assert(photo_export_cleanup(shot,YES,&error));
 assert(![fm fileExistsAtPath:shot.path]);
 assert([[NSData dataWithContentsOfURL:old] isEqualToData:data]);
 assert(!photo_export_cleanup(shot,YES,&error) && error); // cleanup failure is reported
 assert([fm removeItemAtURL:root error:nil]);
 puts("PASS: failed exports retain photo; confirmed exports delete only their copy; cleanup errors reported");
 return 0;
}}
