#import <Foundation/Foundation.h>
#import <Metal/Metal.h>
#include <os/lock.h>
#include <stdlib.h>
#define MLOG(...) ((void)0)
#define MDIE(...) abort()
#include "../nvn/nvn_m3_buffers.h"
int main(void) { @autoreleasepool {
    id<MTLDevice> dev=MTLCreateSystemDefaultDevice(); if(!dev)return 2;
    NSMutableArray<id<MTLBuffer>> *buffers=[NSMutableArray array];
    for(int i=0;i<1537;i++) { id<MTLBuffer> b=[dev newBufferWithLength:4096 options:MTLResourceStorageModeShared];if(!b)return 3;[buffers addObject:b];m3_buf_add(b); }
    if(g_m3_nbufs!=1537)return 4;
    for(id<MTLBuffer> b in buffers) {NSUInteger off=0;if(m3_buf(b.gpuAddress+127,&off)!=b||off!=127)return 5;m3_buf_add(b);m3_buf_add(b);}
    if(g_m3_nbufs!=1537)return 6;
    for(int i=0;i<1537;i+=2)m3_buf_del(buffers[i]);
    if(g_m3_nbufs!=768)return 7;
    for(int i=0;i<1537;i++){NSUInteger off=9;id<MTLBuffer>b=buffers[i],got=m3_buf(b.gpuAddress,&off);if((i%2==0&&got)||(i%2&&got!=b)||off)return 8;}
    for(int i=0;i<1537;i+=2)m3_buf_add(buffers[i]);
    for(id<MTLBuffer> b in buffers) {NSUInteger off=0;if(m3_buf(b.gpuAddress+4095,&off)!=b||off!=4095)return 9;}
    NSUInteger off=9;if(m3_buf(0,&off)||off)return 10;
    for(id<MTLBuffer>b in buffers)m3_buf_del(b);
    if(g_m3_nbufs)return 11;
    puts("PASS: 1537 live buffers, duplicate registration, removal and reinsertion; no dropped addresses");
    return 0;
} }
