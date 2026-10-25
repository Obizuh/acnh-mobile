#import "../Sources/ACNHLogFiles.h"
#include <assert.h>
static void put(NSString *p,NSString *s){assert([s writeToFile:p atomically:YES encoding:NSUTF8StringEncoding error:nil]);}
static NSString *read_log_text(NSString *p){return [NSString stringWithContentsOfFile:p encoding:NSUTF8StringEncoding error:nil];}
int main(){@autoreleasepool{
 NSFileManager *fm=NSFileManager.defaultManager;NSString *doc=[NSTemporaryDirectory() stringByAppendingPathComponent:NSUUID.UUID.UUIDString];
 assert([fm createDirectoryAtPath:doc withIntermediateDirectories:YES attributes:nil error:nil]);
 for(NSString *n in @[@"log.txt",@"log.prev.txt",@"pso_user.tsv",@"startup-files.tsv"])put([doc stringByAppendingPathComponent:n],n);
 NSError *e=nil;NSString *logs=acnh_prepare_logs(doc,&e);assert(logs && !e);
 for(NSString *n in @[@"log.txt",@"log.prev.txt",@"pso_user.tsv",@"startup-files.tsv"]){assert(![fm fileExistsAtPath:[doc stringByAppendingPathComponent:n]]);assert([read_log_text([logs stringByAppendingPathComponent:n]) isEqual:n]);}
 put([doc stringByAppendingPathComponent:@"log.txt"],@"legacy-log");
 put([logs stringByAppendingPathComponent:@"pso_user.tsv"],@"current");put([doc stringByAppendingPathComponent:@"pso_user.tsv"],@"old\n");
 assert(acnh_prepare_logs(doc,&e));assert([read_log_text([logs stringByAppendingPathComponent:@"log.txt"]) isEqual:@"log.txt"]);
 assert([read_log_text([logs stringByAppendingPathComponent:@"pso_user.tsv"]) isEqual:@"current\nold\n"]);
 BOOL found=NO;for(NSString *n in [fm contentsOfDirectoryAtPath:logs error:nil])if([n hasPrefix:@"log-legacy-"] && [read_log_text([logs stringByAppendingPathComponent:n]) isEqual:@"legacy-log"])found=YES;assert(found);
 assert(acnh_prepare_logs(doc,&e));assert([read_log_text([logs stringByAppendingPathComponent:@"pso_user.tsv"]) isEqual:@"current\nold\n"]);
 assert([fm removeItemAtPath:logs error:nil]);put(logs,@"blocked");put([doc stringByAppendingPathComponent:@"log.txt"],@"keep-me");e=nil;
 assert(!acnh_prepare_logs(doc,&e));assert([read_log_text([doc stringByAppendingPathComponent:@"log.txt"]) isEqual:@"keep-me"]);
 [fm removeItemAtPath:doc error:nil];puts("Log migration: old files, conflicts, PSO merge, repeated startup and failure preservation passed");
}}
