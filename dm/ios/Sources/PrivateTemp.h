#pragma once
#import <Foundation/Foundation.h>
#include <stdio.h>
// Keep scratch data out of Files without losing an older installation's contents.
static inline NSString *acnh_private_temp(NSString *documents) {
    NSFileManager *fm=NSFileManager.defaultManager;
    NSString *legacy=[documents stringByAppendingPathComponent:@"temp"];
    NSString *hidden=[documents stringByAppendingPathComponent:@".temp"];
    NSError *error=nil;
    if([fm fileExistsAtPath:legacy]) {
        NSString *target=hidden;
        if([fm fileExistsAtPath:hidden])
            target=[hidden stringByAppendingPathComponent:[@"previous-" stringByAppendingString:NSUUID.UUID.UUIDString]];
        if(![fm moveItemAtPath:legacy toPath:target error:&error]) {
            // Preserve the original on a migration failure; still try its hidden attribute.
            [[NSURL fileURLWithPath:legacy isDirectory:YES] setResourceValue:@YES forKey:NSURLIsHiddenKey error:nil];
            fprintf(stderr,"boot: temp migration failed: %s\n",error.localizedDescription.UTF8String);
            return legacy;
        }
    }
    if(![fm createDirectoryAtPath:hidden withIntermediateDirectories:YES attributes:nil error:&error])
        fprintf(stderr,"boot: private temp create failed: %s\n",error.localizedDescription.UTF8String);
    return hidden;
}
