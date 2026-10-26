#import <Foundation/Foundation.h>
#include <assert.h>
#include <errno.h>
#include <sys/stat.h>
int front_save_swap(const char*,const char*);
int main(void){@autoreleasepool{
 NSFileManager *fm=NSFileManager.defaultManager;
 NSURL *root=[NSURL fileURLWithPath:[NSTemporaryDirectory() stringByAppendingPathComponent:NSUUID.UUID.UUIDString] isDirectory:YES];
 NSURL *old=[root URLByAppendingPathComponent:@"save"],*next=[root URLByAppendingPathComponent:@"save.new"];
 assert([fm createDirectoryAtURL:old withIntermediateDirectories:YES attributes:nil error:nil]);
 assert([fm createDirectoryAtURL:next withIntermediateDirectories:YES attributes:nil error:nil]);
 assert([[@"old-main" dataUsingEncoding:NSUTF8StringEncoding] writeToURL:[old URLByAppendingPathComponent:@"main.dat"] atomically:YES]);
 assert([[@"new-main" dataUsingEncoding:NSUTF8StringEncoding] writeToURL:[next URLByAppendingPathComponent:@"main.dat"] atomically:YES]);
 assert([[@"new-header" dataUsingEncoding:NSUTF8StringEncoding] writeToURL:[next URLByAppendingPathComponent:@"mainHeader.dat"] atomically:YES]);
 assert(front_save_swap(next.fileSystemRepresentation,old.fileSystemRepresentation)==0);
 assert([[NSString stringWithContentsOfURL:[old URLByAppendingPathComponent:@"main.dat"] encoding:NSUTF8StringEncoding error:nil] isEqual:@"new-main"]);
 assert([[NSString stringWithContentsOfURL:[old URLByAppendingPathComponent:@"mainHeader.dat"] encoding:NSUTF8StringEncoding error:nil] isEqual:@"new-header"]);
 assert([[NSString stringWithContentsOfURL:[next URLByAppendingPathComponent:@"main.dat"] encoding:NSUTF8StringEncoding error:nil] isEqual:@"old-main"]);
 assert(front_save_swap("/nonexistent/acnh-stage",old.fileSystemRepresentation)==-1);
 assert([[NSString stringWithContentsOfURL:[old URLByAppendingPathComponent:@"main.dat"] encoding:NSUTF8StringEncoding error:nil] isEqual:@"new-main"]);
 assert(front_save_swap(NULL,old.fileSystemRepresentation)==-1 && errno==EINVAL);
 [fm removeItemAtURL:root error:nil];puts("PASS coordinated atomic directory swap, paired files, old backup and failed replacement preservation");
}}
