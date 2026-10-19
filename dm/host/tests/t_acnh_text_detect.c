#include "../../ios-acnh/Sources/AcnhTextDetect.h"
#include <string.h>
#include <CoreGraphics/CoreGraphics.h>
#include <ImageIO/ImageIO.h>
#include <stdio.h>
int main(int argc,char **argv){int failures=0;for(int i=1;i<argc;i++){
 CFURLRef url=CFURLCreateFromFileSystemRepresentation(NULL,(const UInt8*)argv[i],strlen(argv[i]),0);CGImageSourceRef src=CGImageSourceCreateWithURL(url,NULL);CGImageRef img=src?CGImageSourceCreateImageAtIndex(src,0,NULL):NULL;if(!img)return 2;
 uint8_t pixels[192*108*4];CGColorSpaceRef cs=CGColorSpaceCreateWithName(kCGColorSpaceSRGB);CGContextRef ctx=CGBitmapContextCreate(pixels,192,108,8,192*4,cs,kCGImageAlphaPremultipliedLast|kCGBitmapByteOrder32Big);CGContextDrawImage(ctx,CGRectMake(0,0,192,108),img);
 int matched=acnh_text_keyboard(pixels),want=strstr(argv[i],"scene")==NULL;printf("%s expected=%d actual=%d\n",argv[i],want,matched);failures+=matched!=want;
 CGContextRelease(ctx);CGColorSpaceRelease(cs);CGImageRelease(img);CFRelease(src);CFRelease(url);
 }printf("failures=%d\n",failures);return !!failures;}
