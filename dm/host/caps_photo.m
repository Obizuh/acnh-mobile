// Save locally first; the optional iOS frontend exports the completed PNG asynchronously.
#import <Foundation/Foundation.h>
#import <CoreGraphics/CoreGraphics.h>
#import <ImageIO/ImageIO.h>
#import <UniformTypeIdentifiers/UniformTypeIdentifiers.h>
#include <mach/mach.h>
#include <stdatomic.h>
#include <time.h>
#include <stdio.h>
extern void front_photo_saved(const char *path) __attribute__((weak_import));
int dm_caps_save_image(CGImageRef image,uint8_t entry[32]);
int dm_caps_save_rgba(const void *pixels,size_t bytes,uint8_t entry[32]) {
 @autoreleasepool {
  size_t width=(bytes==1280u*720u*4u||bytes==1280u*768u*4u)?1280:(bytes==1920u*1080u*4u||bytes==1920u*1088u*4u)?1920:0;
  if(!pixels||!width)return 0;size_t height=width==1280?720:1080;
  // NVN passes the padded allocation size (e.g. 1280x768); trailing rows are not pixels.
  bytes=width*height*4;
  // Reject an invalid guest address without faulting on a guest stack.
  void *copy=malloc(bytes);if(!copy)return 0;vm_size_t got=0;
  if(vm_read_overwrite(mach_task_self(),(vm_address_t)pixels,bytes,(vm_address_t)copy,&got)!=KERN_SUCCESS||got!=bytes){free(copy);return 0;}
  CGColorSpaceRef cs=CGColorSpaceCreateWithName(kCGColorSpaceSRGB);
  CGContextRef ctx=cs?CGBitmapContextCreate(copy,width,height,8,width*4,cs,kCGImageAlphaNoneSkipLast|kCGBitmapByteOrder32Big):NULL;
  CGImageRef image=ctx?CGBitmapContextCreateImage(ctx):NULL;
  int ok=image?dm_caps_save_image(image,entry):0;
  if(image)CGImageRelease(image);if(ctx)CGContextRelease(ctx);if(cs)CGColorSpaceRelease(cs);free(copy);return ok;
 }
}

// Shared by the in-game camera and dedicated capture key; preserves render dimensions.
int dm_caps_save_image(CGImageRef image,uint8_t entry[32]) {
 @autoreleasepool {
  if(!image||!entry)return 0;
  size_t width=CGImageGetWidth(image),height=CGImageGetHeight(image);
  NSString *folder=getenv("DM_PHOTO_DIR")?@(getenv("DM_PHOTO_DIR")):[NSFileManager.defaultManager.currentDirectoryPath stringByAppendingPathComponent:@"photos"];
  NSError *error=nil;BOOL ok=[NSFileManager.defaultManager createDirectoryAtPath:folder withIntermediateDirectories:YES attributes:nil error:&error];
  struct timespec now;clock_gettime(CLOCK_REALTIME,&now);struct tm local;localtime_r(&now.tv_sec,&local);
  static _Atomic unsigned sequence;unsigned serial=atomic_fetch_add(&sequence,1);
  NSString *path=[folder stringByAppendingPathComponent:[NSString stringWithFormat:@"ACNH-%04d%02d%02d-%02d%02d%02d-%09ld-%u.png",local.tm_year+1900,local.tm_mon+1,local.tm_mday,local.tm_hour,local.tm_min,local.tm_sec,now.tv_nsec,serial]];
  NSString *temporary=[path stringByAppendingString:@".tmp"];
  CGImageDestinationRef dest=ok&&image?CGImageDestinationCreateWithURL((__bridge CFURLRef)[NSURL fileURLWithPath:temporary],(__bridge CFStringRef)UTTypePNG.identifier,1,NULL):NULL;
  ok=dest!=NULL;if(dest){CGImageDestinationAddImage(dest,image,NULL);ok=CGImageDestinationFinalize(dest);CFRelease(dest);}
  if(ok)ok=[NSFileManager.defaultManager moveItemAtPath:temporary toPath:path error:&error];
  if(!ok)[NSFileManager.defaultManager removeItemAtPath:temporary error:nil];
  if(ok){memset(entry,0,32);uint64_t size=[NSFileManager.defaultManager attributesOfItemAtPath:path error:nil].fileSize;memcpy(entry,&size,8);
   uint64_t identifier=((uint64_t)now.tv_sec<<32)^now.tv_nsec^serial;memcpy(entry+8,&identifier,8);uint16_t year=local.tm_year+1900;memcpy(entry+16,&year,2);
   entry[18]=local.tm_mon+1;entry[19]=local.tm_mday;entry[20]=local.tm_hour;entry[21]=local.tm_min;entry[22]=local.tm_sec;entry[23]=serial;entry[24]=1;entry[31]=1;}
  if(ok && front_photo_saved)front_photo_saved(path.UTF8String);
  fprintf(stderr,"[dm] CAPS local photo %zux%zu %s: %s\n",width,height,ok?"saved":"failed",ok?path.UTF8String:(error.localizedDescription.UTF8String?:"image encoding failed"));return ok;
 }
}

int dm_caps_photo_space(void) { @autoreleasepool {
 NSString *folder=getenv("DM_PHOTO_DIR")?@(getenv("DM_PHOTO_DIR")):[NSFileManager.defaultManager.currentDirectoryPath stringByAppendingPathComponent:@"photos"];
 if(![NSFileManager.defaultManager createDirectoryAtPath:folder withIntermediateDirectories:YES attributes:nil error:nil])return 0;
 NSDictionary *attrs=[NSFileManager.defaultManager attributesOfFileSystemForPath:folder error:nil];return attrs && [attrs[NSFileSystemFreeSize] unsignedLongLongValue]>16u*1024u*1024u;
}}
