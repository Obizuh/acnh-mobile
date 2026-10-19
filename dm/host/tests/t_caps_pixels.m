#import <Foundation/Foundation.h>
#import <ImageIO/ImageIO.h>
#import <CoreGraphics/CoreGraphics.h>
int dm_caps_save_rgba(const void*,size_t,uint8_t[32]);
int main(){@autoreleasepool{
setenv("DM_PHOTO_DIR","/private/tmp/acnh-caps-pixel-test",1);size_t n=1280*720*4;uint8_t*p=malloc(n);for(size_t i=0;i<n;i+=4){p[i]=240;p[i+1]=32;p[i+2]=16;p[i+3]=255;}uint8_t entry[32];if(!dm_caps_save_rgba(p,n,entry))return 1;
NSArray*files=[NSFileManager.defaultManager contentsOfDirectoryAtPath:@"/private/tmp/acnh-caps-pixel-test" error:nil];NSString*file=[@"/private/tmp/acnh-caps-pixel-test" stringByAppendingPathComponent:files.lastObject];CGImageSourceRef src=CGImageSourceCreateWithURL((__bridge CFURLRef)[NSURL fileURLWithPath:file],NULL);CGImageRef img=src?CGImageSourceCreateImageAtIndex(src,0,NULL):NULL;int ok=img&&CGImageGetWidth(img)==1280&&CGImageGetHeight(img)==720;if(img){uint8_t rgb[4]={0};CGColorSpaceRef cs=CGColorSpaceCreateDeviceRGB();CGContextRef ctx=CGBitmapContextCreate(rgb,1,1,8,4,cs,kCGImageAlphaPremultipliedLast);CGContextDrawImage(ctx,CGRectMake(0,0,1,1),img);ok&=rgb[0]==240&&rgb[1]==32&&rgb[2]==16;CGContextRelease(ctx);CGColorSpaceRelease(cs);CGImageRelease(img);}if(src)CFRelease(src);
ok&=dm_caps_save_rgba(p,1280u*768u*4u,entry);ok&=!dm_caps_save_rgba((void*)1,n,entry);ok&=!dm_caps_save_rgba(p,123,entry);setenv("DM_PHOTO_DIR","/dev/null/photos",1);ok&=!dm_caps_save_rgba(p,n,entry);free(p);printf("PNG size/color, invalid address/size, unwritable directory: %s\n",ok?"PASS":"FAIL");return !ok;}}
