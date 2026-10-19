#include "../nvn/nvn_mtl.m"
void *front_metal_layer(void){return NULL;}
module_t *module_of(const void *p){(void)p;return NULL;}
module_t g_mod[MAX_MOD];int g_nmod;
void svc_thread_dump(void){}
int prog_key(const prog_t *p,unsigned stage,uint8_t key[8]){(void)p;(void)stage;(void)key;return 0;}
uint64_t now_ns(void){return bp_now();}
@interface TestDisplayUpdate:NSObject
@property(nonatomic) CFTimeInterval targetPresentationTimestamp;
@property(nonatomic,strong) id drawable;
@end
@implementation TestDisplayUpdate @end
static int failures;
#define CHECK(s,c) do{int ok=!!(c);printf("%s: %s\n",s,ok?"PASS":"FAIL");failures+=!ok;}while(0)
static void cadence(int hz,int game_hz,int ticks,int want,double start,tex_t t){
 g_pq_read=g_pq_in=g_pq_done=0;g_link_dropped=0;g_dm_present_link=1;
 NvMetalDisplayLinkDelegate *delegate=[NvMetalDisplayLinkDelegate new];TestDisplayUpdate *u=[TestDisplayUpdate new];
 int bursts=0;
 for(int i=0;i<ticks;i++){
  while(g_pq_in-g_pq_read<3){g_pq[g_pq_in%PQ]=(pq_item){.t=t,.period=1.0/game_hz,.frame=g_pq_in+1};g_pq_in++;}
  u.targetPresentationTimestamp=start+(double)i/hz;uint64_t before=g_pq_read;
  [delegate metalDisplayLink:nil needsUpdate:(CAMetalDisplayLinkUpdate *)u];
  bursts+=g_pq_read-before>1;
 }
 char label[128];snprintf(label,sizeof label,"%dHz callbacks, %dfps game: %llu frames, want %d",hz,game_hz,g_pq_read,want);
 CHECK(label,g_pq_read==want);CHECK("no coalesced game frames",g_link_dropped==0);CHECK("no frame bursts",bursts==0);
}
int main(void){@autoreleasepool{
 setenv("DM_PSO_MANIFEST","off",1);setenv("ACNH_RETIRE_VIEWS","1",1);mtl_init();
 MTLTextureDescriptor *d=[MTLTextureDescriptor texture2DDescriptorWithPixelFormat:MTLPixelFormatRGBA8Unorm width:4 height:4 mipmapped:NO];
 d.usage=MTLTextureUsageShaderRead|MTLTextureUsagePixelFormatView;
 id<MTLTexture> a=[g_dev newTextureWithDescriptor:d],b=[g_dev newTextureWithDescriptor:d];
 tex_t t={.serial=88001};t.d.width=t.d.height=4;t.d.depth=t.d.levels=1;t.d.target=NVN_TEXTURE_TARGET_2D;t.d.format=NVN_FORMAT_RGBA8;
 serial_put(t.serial,a);
 cadence(60,30,120,60,1,t);cadence(120,30,240,60,10,t);cadence(120,60,240,120,20,t);cadence(30,30,60,60,30,t);
 view_t v={.set_levels=1,.nlevels=1};mtl_reg_tex(2,&t,&v);
 pthread_mutex_lock(&g_rmu);id<MTLTexture> view=tex_by_id(2);pthread_mutex_unlock(&g_rmu);
 uint64_t old_reg=g_reg_gen;serial_put(88002,b);acnh_retire_stale_views();
 CHECK("unrelated resource keeps texture view",g_treg[2].view==(__bridge void *)view);
 CHECK("unrelated resource keeps binding cache generation",g_reg_gen==old_reg);
 serial_put(t.serial,b);acnh_retire_stale_views();CHECK("replaced texture retires old view",g_treg[2].view==NULL);
 CHECK("30fps requests 30Hz callbacks",nvn_acnh_link_mode(1,0,33333333)==3);
 CHECK("60fps retains 60Hz callbacks",nvn_acnh_link_mode(1,0,16666667)==1);
 CHECK("120Hz requested for 30fps becomes 30Hz",nvn_acnh_link_mode(2,0,33333333)==3);
 CHECK("disabled callbacks remain disabled",nvn_acnh_link_mode(0,1,33333333)==0);
 printf("failures=%d\n",failures);return failures!=0;
}}
