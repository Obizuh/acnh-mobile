#pragma once
#import <Foundation/Foundation.h>
// Inspect the real host's on-disk plist, never the substituted guest mainBundle.
static inline BOOL photo_host_purpose(NSString *bundlePath,NSString *signingID) {
 if(!bundlePath.length || !signingID.length)return NO;
 NSDictionary *info=[NSDictionary dictionaryWithContentsOfFile:[bundlePath stringByAppendingPathComponent:@"Info.plist"]];
 if(![info[@"CFBundleIdentifier"] isEqual:signingID])return NO;
 id purpose=info[@"NSPhotoLibraryAddUsageDescription"];
 return [purpose isKindOfClass:NSString.class] && [purpose length]>0;
}
