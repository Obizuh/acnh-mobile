#pragma once
#import <Foundation/Foundation.h>
// Only a confirmed Photos commit permits removing this shot's local copy.
static inline BOOL photo_export_cleanup(NSURL *url,BOOL saved,NSError **error) {
    if(!saved)return NO;
    return [NSFileManager.defaultManager removeItemAtURL:url error:error];
}
