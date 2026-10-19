#include "../hle_offline.c"
void hle_bind_system_event(void *p,int on){(void)p;(void)on;}

int dm_caps_photo_space(void){return 0;}
int dm_caps_save_rgba(const void*p,size_t n,uint8_t e[32]){(void)p;(void)n;(void)e;return 0;}
static int failures,checks;
#define CHECK(c) do { checks++;if(!(c)){failures++;printf("FAIL line %d\n",__LINE__);} }while(0)
static void request(uint32_t *w,unsigned type,int domain,unsigned cmd,unsigned pid){
 memset(w,0,256);w[0]=type;w[1]=24|(pid?0x80000000u:0);unsigned p=pid?32:16;
 if(pid)w[2]=1;
 if(domain){w[p/4]=1|(32<<16);w[p/4+1]=1;p+=16;}
 w[p/4]=0x49434653;w[p/4+2]=cmd;w[p/4+3]=0x1234;
}
int main(void){
 uint32_t h=77,w[64],old[64];
 CHECK(h_hipc_Connect(&h,"olsc:u")==0&&h==0x6f6c7363u);
 uint32_t handle=h;CHECK(h_hipc_Connect(&h,"other")==RES_FAIL&&h==0);
 CHECK(h_hipc_Connect(NULL,"olsc:u")==RES_FAIL);
 request(w,7,0,3,0);CHECK(h_hipc_Send(handle,w,sizeof w)==0&&w[4]==0x4f434653&&w[6]==0&&w[7]==0x1234&&w[8]==0);
 request(w,5,0,0,0);CHECK(h_hipc_Send(handle,w,sizeof w)==0&&w[6]==0&&w[8]==1);
 request(w,6,0,0,1);CHECK(h_hipc_Send(handle,w,sizeof w)==0&&w[6]==0);
 request(w,6,0,13,0);CHECK(h_hipc_Send(handle,w,sizeof w)==0&&w[6]==0&&w[8]==0&&w[9]==0&&w[10]==0);
 request(w,6,1,13,1);CHECK(h_hipc_Send(handle,w,sizeof w)==0&&w[4]==0&&w[8]==0x4f434653&&w[10]==0&&w[12]==0);
 request(w,6,1,14,0);CHECK(h_hipc_Send(handle,w,sizeof w)==0&&w[10]==0);
 request(w,6,1,14,0);w[12]=1;CHECK(h_hipc_Send(handle,w,sizeof w)==0&&w[10]==RES_FAIL);
 for(unsigned cmd=15;cmd<=27;cmd++){
  request(w,6,0,cmd,0);CHECK(h_hipc_Send(handle,w,sizeof w)==0&&w[6]==RES_FAIL);
 }
 request(w,6,0,13,0);memcpy(old,w,sizeof w);CHECK(h_hipc_Send(123,w,sizeof w)==RES_FAIL&&!memcmp(old,w,sizeof w));
 CHECK(h_hipc_Send(handle,w,127)==RES_FAIL&&!memcmp(old,w,sizeof w));
 CHECK(h_hipc_Send(handle,NULL,256)==RES_FAIL);
 memset(w,0,sizeof w);w[0]=0xffff0006;CHECK(h_hipc_Send(handle,w,128)==RES_FAIL);
 request(w,6,0,13,0);w[4]=0x12345678;CHECK(h_hipc_Send(handle,w,sizeof w)==RES_FAIL);
 printf("OLSC offline protocol: %d checks, %d failures\n",checks,failures);return !!failures;
}
