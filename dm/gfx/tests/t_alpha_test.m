#include "../nvn/nvn_mtl.m"
int prog_key(const prog_t*p,unsigned s,uint8_t*k){return 0;}
void*front_metal_layer(void){return NULL;}module_t*module_of(const void*p){return NULL;}module_t g_mod[MAX_MOD];int g_nmod;void svc_thread_dump(void){}uint64_t now_ns(void){return bp_now();}
int main(int argc,char**argv){@autoreleasepool{
 if(argc!=2){fprintf(stderr,"Usage: t_alpha_test <patched fragment.metallib>\n");return 2;}
 mtl_init();arena_begin();NSError*err=nil;
 id<MTLLibrary>lib=[g_dev newLibraryWithURL:[NSURL fileURLWithPath:@(argv[1])] error:&err];if(!lib){NSLog(@"%@",err);return 1;}
 NSString*vs=@"#include <metal_stdlib>\nusing namespace metal;struct O{float4 p [[position]];float4 a [[user(locn1)]];float4 b [[user(locn2)]];};vertex O v(uint i [[vertex_id]]){float2 p[3]={float2(-1,-1),float2(3,-1),float2(-1,3)};O o;o.p=float4(p[i],0,1);o.a=o.b=float4(.5);return o;}";
 id<MTLLibrary>vl=[g_dev newLibraryWithSource:vs options:nil error:&err];NvShader*sh=[NvShader new];sh->fn=[lib newFunctionWithName:@"main0"];sh->alpha_test=1;
 MTLRenderPipelineDescriptor*pd=[MTLRenderPipelineDescriptor new];pd.vertexFunction=[vl newFunctionWithName:@"v"];pd.fragmentFunction=sh->fn;pd.colorAttachments[0].pixelFormat=MTLPixelFormatRGBA8Unorm;
 id<MTLRenderPipelineState>pso=[g_dev newRenderPipelineStateWithDescriptor:pd error:&err];if(!pso){NSLog(@"%@",err);return 1;}
 id<MTLBuffer>cb=[g_dev newBufferWithLength:65536 options:MTLResourceStorageModeShared];float*c=cb.contents;c[0]=c[1]=c[2]=c[3]=1;
 MTLTextureDescriptor*td=[MTLTextureDescriptor texture2DDescriptorWithPixelFormat:MTLPixelFormatRGBA32Float width:1 height:1 mipmapped:NO];td.storageMode=MTLStorageModeShared;td.usage=MTLTextureUsageShaderRead;
 id<MTLTexture>noise=[g_dev newTextureWithDescriptor:td];float n[4]={1,1,1,1};[noise replaceRegion:MTLRegionMake2D(0,0,1,1) mipmapLevel:0 withBytes:n bytesPerRow:16];
 MTLSamplerDescriptor*sd=[MTLSamplerDescriptor new];sd.supportArgumentBuffers=YES;id<MTLSamplerState>sam=[g_dev newSamplerStateWithDescriptor:sd];
 id<MTLTexture>tex[3];id<MTLBuffer>args[3];id<MTLArgumentEncoder>ae=[sh->fn newArgumentEncoderWithBufferIndex:0];
 for(int a=0;a<3;a++){tex[a]=[g_dev newTextureWithDescriptor:td];float pixel[4]={1,0,0,a*.5f};[tex[a] replaceRegion:MTLRegionMake2D(0,0,1,1) mipmapLevel:0 withBytes:pixel bytesPerRow:16];args[a]=[g_dev newBufferWithLength:ae.encodedLength options:MTLResourceStorageModeShared];[ae setArgumentBuffer:args[a] offset:0];[ae setBuffer:cb offset:0 atIndex:0];[ae setTexture:noise atIndex:1];[ae setSamplerState:sam atIndex:2];[ae setTexture:tex[a] atIndex:3];[ae setSamplerState:sam atIndex:4];}
 td=[MTLTextureDescriptor texture2DDescriptorWithPixelFormat:MTLPixelFormatRGBA8Unorm width:24 height:2 mipmapped:NO];td.storageMode=MTLStorageModeShared;td.usage=MTLTextureUsageRenderTarget;id<MTLTexture>out=[g_dev newTextureWithDescriptor:td];
 MTLRenderPassDescriptor*rp=[MTLRenderPassDescriptor new];rp.colorAttachments[0].texture=out;rp.colorAttachments[0].loadAction=MTLLoadActionClear;rp.colorAttachments[0].storeAction=MTLStoreActionStore;rp.colorAttachments[0].clearColor=MTLClearColorMake(0,0,1,1);
 id<MTLCommandBuffer>cmd=[g_q commandBuffer];RENC en=[cmd renderCommandEncoderWithDescriptor:rp];[en setRenderPipelineState:pso];[en useResource:cb usage:MTLResourceUsageRead stages:MTLRenderStageFragment];[en useResource:noise usage:MTLResourceUsageRead stages:MTLRenderStageFragment];for(int a=0;a<3;a++)[en useResource:tex[a] usage:MTLResourceUsageRead stages:MTLRenderStageFragment];
 // Two reference values in one in-flight encoder catch accidental shared-state overwrites.
 for(int row=0;row<2;row++)for(int f=1;f<=8;f++)for(int a=0;a<3;a++){float ref=row?0:.5f;state_op(OP_ALPHA_REF,&ref);S.color.alpha_func=f;[en setFragmentBuffer:args[a] offset:0 atIndex:0];bind_alpha_test(en,sh);[en setScissorRect:(MTLScissorRect){(f-1)*3+a,row,1,1}];[en drawPrimitives:MTLPrimitiveTypeTriangle vertexStart:0 vertexCount:3];}
 [en endEncoding];[cmd commit];[cmd waitUntilCompleted];if(cmd.error){NSLog(@"%@",cmd.error);return 1;}
 // Independent expected truth tables, columns alpha 0, 0.5, 1.
 const int expected[2][8][3]={{{0,0,0},{1,0,0},{0,1,0},{1,1,0},{0,0,1},{1,0,1},{0,1,1},{1,1,1}},{{0,0,0},{0,0,0},{1,0,0},{1,0,0},{0,1,1},{0,1,1},{1,1,1},{1,1,1}}};
 unsigned char pixels[192];[out getBytes:pixels bytesPerRow:96 fromRegion:MTLRegionMake2D(0,0,24,2) mipmapLevel:0];int fail=0;
 for(int row=0;row<2;row++)for(int f=0;f<8;f++)for(int a=0;a<3;a++){int i=(row*24+f*3+a)*4;int pass=pixels[i]>250&&pixels[i+2]<5;int reject=pixels[i]<5&&pixels[i+2]>250;if((expected[row][f][a]?!pass:!reject)){printf("FAIL row=%d func=%d alpha=%g pixel=%d,%d,%d\n",row,f+1,a*.5,pixels[i],pixels[i+1],pixels[i+2]);fail++;}}
 printf("Alpha test GPU: 48 checks, %d failures\n",fail);return !!fail;
}}
