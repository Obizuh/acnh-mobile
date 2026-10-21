// Exercise production encode_args, including the A12-sized one-binding layout.
#include "../nvn/nvn_mtl.m"
#include <assert.h>
void front_photo_saved(const char *path){}
int prog_key(const prog_t*p,unsigned s,uint8_t*k){return 0;}
void *front_metal_layer(void){return NULL;}
module_t *module_of(const void*p){return NULL;}
module_t g_mod[MAX_MOD];int g_nmod;void svc_thread_dump(void){}
uint64_t now_ns(void){return bp_now();}
static int retiredSamplerDestroyed;
@interface SamplerLifetimeProbe : NSObject @end
@implementation SamplerLifetimeProbe
- (void)dealloc { retiredSamplerDestroyed++; }
@end
static void sampler_retirement_test(void) {
 samp_par sp={0};sp.lod_max=16;mtl_reg_samp(901,&sp);
 __weak id old;
 @autoreleasepool {
  id probe=[SamplerLifetimeProbe new];old=probe;
  g_sview[901]=(void*)CFBridgingRetain(probe);probe=nil;
  // The old argument block may still be queued when this slot is replaced.
  sp.magf=1;mtl_reg_samp(901,&sp);
 }
 assert(old && retiredSamplerDestroyed==0);
 uint64_t previous=g_done;g_done=g_fence;
 @autoreleasepool { defer_drain(); }
 assert(old && retiredSamplerDestroyed==0); // incomplete fence must retain it
 g_done=g_fence+1;
 @autoreleasepool { defer_drain(); }
 assert(!old && retiredSamplerDestroyed==1);g_done=previous;
}
static NvShader *shader(id<MTLLibrary> lib,NSString *name,int api){
 NvShader *sh=[NvShader new];sh->fn=[lib newFunctionWithName:name];assert(sh->fn);
 sh->ae=[sh->fn newArgumentEncoderWithBufferIndex:0];assert(sh->ae);
 sh->arg_len=sh->ae.encodedLength;sh->arg_api=api;return sh;
}
int main(void){@autoreleasepool{
 // Capability/layout routing independent of which GPU runs this test.
 assert(!nvn_argument_uses_encoder(1,1,8,0,0));
 assert(!nvn_argument_uses_encoder(1,1,24,2,0));
 assert(nvn_argument_uses_encoder(0,1,8,0,0)); // A12/A13, even equal-size
 assert(nvn_argument_uses_encoder(1,0,8,0,0)); // non-tier-2
 assert(nvn_argument_uses_encoder(1,1,16,0,0)); // observed padding failure
 assert(nvn_argument_uses_encoder(1,1,8,0,1)); // diagnostic force
 mtl_init();arena_begin();NSError *err=nil;
 NSString *src=@"#include <metal_stdlib>\nusing namespace metal;\n"
 "struct A{constant float4 *c[[id(0)]];};\n"
 "vertex float4 v(uint n[[vertex_id]]){float2 p[3]={float2(-1,-1),float2(3,-1),float2(-1,3)};return float4(p[n],0,1);}\n"
 "fragment float4 f(constant A&a[[buffer(0)]]){return a.c[0];}\n"
 "struct B{constant float4*c[[id(0)]];texture2d<float>t[[id(2)]];sampler s[[id(4)]];device float4*o[[id(6)]];texture2d<float,access::write>im[[id(8)]];texture_buffer<float>tb[[id(10)]];constant float4*c1[[id(12)]];constant uint*d[[id(14)]];constant float*sp[[id(16)]];};\n"
 "kernel void k(constant B&a[[buffer(0)]]){float4 x=a.c[0]+a.t.sample(a.s,float2(.5))+a.tb.read(0u)+a.c1[0]+float4(a.d[0])+float4(a.sp[0]);a.o[0]=x;a.im.write(x,uint2(0));}\n";
 id<MTLLibrary>lib=[g_dev newLibraryWithSource:src options:nil error:&err];if(!lib){NSLog(@"%@",err);return 1;}
 void *mem=NULL;assert(!posix_memalign(&mem,getpagesize(),getpagesize()));memset(mem,0,getpagesize());mtl_pool_add(mem,getpagesize());
 float *c=(float*)((uint8_t*)mem+256);c[0]=.25;c[1]=.5;c[2]=.75;c[3]=1;
 S.ubo[1][0]=(p_bind_buf){.addr=(uint64_t)mem+256,.size=16};S.ubo[5][0]=S.ubo[1][0];
 MTLTextureDescriptor *td=[MTLTextureDescriptor texture2DDescriptorWithPixelFormat:MTLPixelFormatRGBA8Unorm width:3 height:1 mipmapped:NO];td.storageMode=MTLStorageModeShared;td.usage=MTLTextureUsageRenderTarget;
 id<MTLTexture>out=[g_dev newTextureWithDescriptor:td];
 NvShader *sh=shader(lib,@"f",1);sh->rf.nb=1;sh->rf.b[0]=(nvn_rbind){.kind=NVN_RB_CBUF,.reg=0,.aid=0};sh->arg_len=MAX((NSUInteger)16,sh->arg_len);
 MTLRenderPipelineDescriptor *pd=[MTLRenderPipelineDescriptor new];pd.vertexFunction=[lib newFunctionWithName:@"v"];pd.fragmentFunction=sh->fn;pd.colorAttachments[0].pixelFormat=out.pixelFormat;
 id<MTLRenderPipelineState>ps=[g_dev newRenderPipelineStateWithDescriptor:pd error:&err];assert(ps);
 MTLRenderPassDescriptor *rp=[MTLRenderPassDescriptor new];rp.colorAttachments[0].texture=out;rp.colorAttachments[0].loadAction=MTLLoadActionClear;rp.colorAttachments[0].storeAction=MTLStoreActionStore;
 id<MTLCommandBuffer>cmd=[g_q commandBuffer];id<MTLRenderCommandEncoder>en=[cmd renderCommandEncoderWithDescriptor:rp];[en setRenderPipelineState:ps];g_pass.serial++;
 args_dirty_all();assert(encode_args(en,sh,1,0));[en setScissorRect:(MTLScissorRect){0,0,1,1}];[en drawPrimitives:MTLPrimitiveTypeTriangle vertexStart:0 vertexCount:3];
 uint64_t reused=g_stat_args_reuse;assert(encode_args(en,sh,1,0));assert(g_stat_args_reuse==reused+1);[en setScissorRect:(MTLScissorRect){1,0,1,1}];[en drawPrimitives:MTLPrimitiveTypeTriangle vertexStart:0 vertexCount:3];
 // A new UBO offset must invalidate the cache without overwriting queued draws.
 float *changed=(float*)((uint8_t*)mem+320);for(int i=0;i<4;i++)changed[i]=c[i]+.125f;
 uint64_t encodings=g_stat_args_enc;size_t oldoff=sh->ab_off;id<MTLBuffer>oldbuf=sh->ab_buf;
 NSData *oldblock=[NSData dataWithBytes:(uint8_t*)oldbuf.contents+oldoff length:sh->arg_len];
 S.ubo[1][0].addr=(uint64_t)mem+320;g_args_dirty[1]=1;
 assert(encode_args(en,sh,1,0));assert(g_stat_args_enc==encodings+1);
 assert(sh->ab_buf!=oldbuf || sh->ab_off!=oldoff);
 assert(!memcmp(oldblock.bytes,(uint8_t*)oldbuf.contents+oldoff,oldblock.length));
 [en setScissorRect:(MTLScissorRect){2,0,1,1}];[en drawPrimitives:MTLPrimitiveTypeTriangle vertexStart:0 vertexCount:3];
 encodings=g_stat_args_enc;g_pass.serial++;assert(encode_args(en,sh,1,0));assert(g_stat_args_enc==encodings+1);
 [en endEncoding];[cmd commit];[cmd waitUntilCompleted];if(cmd.error){NSLog(@"%@",cmd.error);return 1;}
 uint8_t pixels[12];[out getBytes:pixels bytesPerRow:12 fromRegion:MTLRegionMake2D(0,0,3,1) mipmapLevel:0];
 for(int i=0;i<2;i++){assert(abs(pixels[4*i]-64)<=1);assert(abs(pixels[4*i+1]-128)<=1);assert(abs(pixels[4*i+2]-191)<=1);assert(pixels[4*i+3]==255);}
 assert(abs(pixels[8]-96)<=1 && abs(pixels[9]-159)<=1 && abs(pixels[10]-223)<=1 && pixels[11]==255);
 // Compare sparse and dense API layouts with the valid dense direct layout.
 NSMutableString *denseSource=[src mutableCopy];
 NSRegularExpression *ids=[NSRegularExpression regularExpressionWithPattern:@"id\\(([0-9]+)\\)" options:0 error:nil];
 NSArray *matches=[ids matchesInString:src options:0 range:NSMakeRange(0,src.length)];
 for(NSTextCheckingResult *m in matches.reverseObjectEnumerator){NSRange range=[m rangeAtIndex:1];int aid=[[src substringWithRange:range] intValue];[denseSource replaceCharactersInRange:range withString:[NSString stringWithFormat:@"%d",aid/2]];}
 id<MTLLibrary>denseLib=[g_dev newLibraryWithSource:denseSource options:nil error:&err];if(!denseLib){NSLog(@"%@",err);return 1;}
 for(int scenario=0;scenario<4;scenario++){
  int api=scenario!=2,dense=scenario>0;
  if(scenario==3){S.ubo[5][0].addr=(uint64_t)mem+320;float *fresh=(float*)((uint8_t*)mem+320);for(int i=0;i<4;i++)fresh[i]=c[i]+.25f;args_dirty_all();}
  NvShader *k=shader(dense?denseLib:lib,@"k",api);k->rf.nb=9;k->rf.drv_read[2]=1;
  int kinds[]={NVN_RB_CBUF,NVN_RB_TEX,NVN_RB_SAMPT,NVN_RB_SBUFP,NVN_RB_IMG,NVN_RB_TBUF,NVN_RB_C1,NVN_RB_DRV2,NVN_RB_SUPPORT};
  for(int i=0;i<9;i++)k->rf.b[i]=(nvn_rbind){.kind=kinds[i],.aid=dense?i:2*i,.reg=0,.dtt=MTLTextureType2D,.rw=i==3};
  k->c1buf=[g_dev newBufferWithLength:512 options:MTLResourceStorageModeShared];res_add(k->c1buf);k->c1_gpu=k->c1buf.gpuAddress;k->c1off=256;float *extra=(float*)((uint8_t*)k->c1buf.contents+256);for(int i=0;i<4;i++)extra[i]=.125f;
  S.ssbo[5][0]=(p_bind_buf){.addr=(uint64_t)mem+512+scenario*256,.size=16};
  id<MTLComputePipelineState>cp=[g_dev newComputePipelineStateWithFunction:k->fn error:&err];assert(cp);
  cmd=[g_q commandBuffer];id<MTLComputeCommandEncoder>ce=[cmd computeCommandEncoder];[ce setComputePipelineState:cp];assert(encode_args(ce,k,5,0));[ce dispatchThreads:MTLSizeMake(1,1,1) threadsPerThreadgroup:MTLSizeMake(1,1,1)];[ce endEncoding];[cmd commit];[cmd waitUntilCompleted];if(cmd.error){NSLog(@"%@",cmd.error);return 1;}
  float *result=(float*)((uint8_t*)mem+512+scenario*256);for(int i=0;i<4;i++)assert(fabsf(result[i]-(c[i]+.125f+(scenario==3?.25f:0)))<.001f);
 }
 sampler_retirement_test();
 puts("PASS: sampler retirement; capability/layout selection; padded single argument; colors/cache; nonzero buffer offsets and dirty rebind; sparse resource kinds; direct-vs-encoder GPU results");return 0;
}}
