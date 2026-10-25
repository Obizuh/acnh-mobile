#import <Foundation/Foundation.h>

// Migrate only known diagnostic files. Existing histories are never overwritten.
static inline NSString *acnh_prepare_logs(NSString *doc,NSError **error) {
    NSFileManager *fm=NSFileManager.defaultManager;
    NSString *logs=[doc stringByAppendingPathComponent:@"logs"];
    if(![fm createDirectoryAtPath:logs withIntermediateDirectories:YES attributes:nil error:error])return nil;
    NSDictionary *attributes=[fm attributesOfItemAtPath:logs error:error];
    if(![attributes[NSFileType] isEqual:NSFileTypeDirectory])return nil;
    for(NSString *name in @[@"log.txt",@"log.prev.txt",@"pso_user.tsv",@"startup-files.tsv"]){
        NSString *src=[doc stringByAppendingPathComponent:name],*dst=[logs stringByAppendingPathComponent:name];
        if(![fm fileExistsAtPath:src])continue;
        if([fm fileExistsAtPath:dst]){
            if([name isEqualToString:@"pso_user.tsv"]){
                NSMutableData *merged=[[NSData dataWithContentsOfFile:dst options:0 error:error] mutableCopy];
                NSData *old=[NSData dataWithContentsOfFile:src options:0 error:error];
                if(!merged || !old)return nil;
                if(merged.length && ((const unsigned char *)merged.bytes)[merged.length-1]!='\n')[merged appendData:[@"\n" dataUsingEncoding:NSUTF8StringEncoding]];
                [merged appendData:old];
                if(![merged writeToFile:dst options:NSDataWritingAtomic error:error])return nil;
                if(![fm removeItemAtPath:src error:error])return nil;
                continue;
            }
            NSString *legacy=[NSString stringWithFormat:@"%@-legacy-%@.%@",name.stringByDeletingPathExtension,NSUUID.UUID.UUIDString,name.pathExtension];
            dst=[logs stringByAppendingPathComponent:legacy];
        }
        if(![fm moveItemAtPath:src toPath:dst error:error])return nil;
    }
    return logs;
}
