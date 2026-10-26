// Coordinate the existing atomic save-directory exchange with iOS document providers.
// Never fall back to uncoordinated or per-file replacement on coordination failure.
#import <Foundation/Foundation.h>
#include <errno.h>
#include <stdio.h>
#include <sys/attr.h>

int front_save_swap(const char *replacement,const char *committed) {
    @autoreleasepool {
        if(!replacement || !committed){errno=EINVAL;return -1;}
        NSURL *url=[NSURL fileURLWithFileSystemRepresentation:committed isDirectory:YES relativeToURL:nil];
        NSFileCoordinator *coordinator=[[NSFileCoordinator alloc] initWithFilePresenter:nil];
        NSError *coordinationError=nil;
        __block int result=-1,swapError=EIO;
        [coordinator coordinateWritingItemAtURL:url options:NSFileCoordinatorWritingForReplacing error:&coordinationError byAccessor:^(NSURL *coordinatedURL){
            result=renamex_np(replacement,coordinatedURL.fileSystemRepresentation,RENAME_SWAP);
            swapError=result ? errno:0;
        }];
        if(coordinationError){
            fprintf(stderr,"save file coordination failed (domain=%s code=%ld)\n",coordinationError.domain.UTF8String,(long)coordinationError.code);
            // Accessor may already have completed. Never report a successful
            // exchange as failed: the caller owns removal of the old directory.
            if(result!=0)swapError=EACCES;
        }
        if(result)errno=swapError;
        return result;
    }
}
