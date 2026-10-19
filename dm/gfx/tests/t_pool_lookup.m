#include "../nvn/nvn_mtl.m"
int prog_key(const prog_t *p, unsigned stage, uint8_t key[8]){(void)p;(void)stage;(void)key;return 0;}
void *front_metal_layer(void){return NULL;}module_t *module_of(const void*p){(void)p;return NULL;}module_t g_mod[MAX_MOD];int g_nmod;void svc_thread_dump(void){}uint64_t now_ns(void){return bp_now();}
static int fails,checks;
#define CHECK(c) do{checks++;if(!(c)){fails++;printf("FAIL line %d\n",__LINE__);}}while(0)
static uint64_t test_addr,test_gpu;static void *test_buf;
static void*reader(void*unused){(void)unused;for(int i=0;i<10000;i++){void*b;uint64_t off,gpu,a=test_addr+(i%2?4096:8192);if(!mtl_gpu_addr(a,32,&b,&off,&gpu)||b!=test_buf||off!=a-test_addr||gpu!=test_gpu+off)return(void*)1;}return NULL;}
int main(void){@autoreleasepool{
 mtl_init();void*raw=NULL;CHECK(posix_memalign(&raw,16384,65536)==0);memset(raw,0,65536);uint64_t a=(uint64_t)raw;void *b1=NULL,*b2=NULL,*b=NULL;uint64_t o,g1,g2,g;
 // Real Metal buffers with overlapping logical pools. Length changes select a
 // different first matching pool; a range-only last-hit cache would be wrong.
 mtl_pool_add(raw,16384);mtl_pool_add(raw,49152);
 CHECK(mtl_gpu_addr(a+8192,1,&b1,&o,&g1)&&o==8192);id<MTLBuffer> keep1=(__bridge id<MTLBuffer>)b1;
 CHECK(mtl_gpu_addr(a+8192,20000,&b2,&o,&g2)&&o==8192&&b2!=b1);id<MTLBuffer>keep2=(__bridge id<MTLBuffer>)b2;
 for(int i=0;i<20;i++){CHECK(mtl_gpu_addr(a+8192,1,&b,&o,&g)&&b==b1&&g==g1);CHECK(mtl_gpu_addr(a+8192,20000,&b,&o,&g)&&b==b2&&g==g2);}
 CHECK(!mtl_gpu_addr(UINT64_MAX-1,16,&b,&o,&g));CHECK(!mtl_addr(a+49152,1,&b,&o));
 CHECK(mtl_addr(a+8192,1,&b,&o)&&b==b1&&o==8192);
 mtl_pool_del(raw);CHECK(mtl_gpu_addr(a+8192,1,&b,&o,&g)&&b==b2&&g==g2);
 test_addr=a;test_gpu=g2-8192;test_buf=b2;pthread_t t[4];for(int i=0;i<4;i++)pthread_create(&t[i],NULL,reader,NULL);for(int i=0;i<4;i++){void*result;pthread_join(t[i],&result);CHECK(result==NULL);}
 mtl_pool_del(raw);CHECK(!mtl_gpu_addr(a+8192,1,&b,&o,&g));
 // Cached miss must disappear when a pool is added at the same address.
 mtl_pool_add(raw,65536);CHECK(mtl_gpu_addr(a+8192,1,&b,&o,&g)&&b!=b1&&b!=b2&&o==8192);
 CHECK(mtl_addr(a+65536,0,&b,&o)&&o==65536);CHECK(!mtl_addr(a+65536,1,&b,&o));
 mtl_pool_del(raw);CHECK(!mtl_gpu_addr(a+8192,1,&b,&o,&g));mtl_drain();(void)keep1;(void)keep2;
 printf("pool lookup cache: %d checks, %d failures\n",checks,fails);return !!fails;
}}
