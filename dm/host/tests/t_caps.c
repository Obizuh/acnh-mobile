#include "../hle_offline.c"
int dm_caps_photo_space(void){return 1;}
void hle_bind_system_event(void*p,int n){(void)p;(void)n;}
static int saves,checks,failures,save_ok=1;
int dm_caps_save_rgba(const void*p,size_t n,uint8_t e[32]){saves++;if((uintptr_t)p!=0x7071234000ull||(n!=1280u*720u*4u&&n!=1280u*768u*4u))return 0;memset(e,0x5a,32);return save_ok;}
#define CHECK(c) do{checks++;if(!(c)){failures++;printf("FAIL %d\n",__LINE__);}}while(0)
static unsigned request(uint32_t*w,int domain,int pid,unsigned cmd,unsigned sends){
 memset(w,0,256);w[0]=6|(sends<<20);unsigned p=8;if(pid){w[1]=0x80000000;w[2]=1;p+=12;}
 for(unsigned i=0;i<sends;i++){uint64_t a=0x7071234000ull;w[p/4]=1280u*720u*4u;w[p/4+1]=(uint32_t)a;w[p/4+2]=1|((a>>36)<<2)|(((a>>32)&15)<<28);p+=12;}
 p=(p+15)&~15u;if(domain){w[p/4]=1|(0x60<<16);w[p/4+1]=1;p+=16;}
 w[p/4]=0x49434653;w[p/4+2]=cmd;w[p/4+3]=0x1234;return p;
}
int main(void){uint32_t h=0,w[64];CHECK(!h_hipc_Connect(&h,"caps:su")&&h==ACNH_CAPS_HANDLE);
 for(int domain=0;domain<2;domain++)for(int pid=0;pid<2;pid++){
  unsigned r=domain?32:16;request(w,domain,pid,32,0);CHECK(!h_hipc_Send(h,w,sizeof w)&&!w[r/4+2]);
  for(unsigned c=203;c<=210;c++){if(c!=203&&c!=205&&c!=210)continue;request(w,domain,pid,c,c==203?1:2);int before=saves;CHECK(!h_hipc_Send(h,w,sizeof w)&&!w[r/4+2]&&w[r/4+3]==0x1234&&w[r/4+4]==0x5a5a5a5a&&saves==before+1);}
  request(w,domain,pid,999,0);CHECK(!h_hipc_Send(h,w,sizeof w)&&w[r/4+2]==RES_FAIL);
  request(w,domain,pid,203,0);CHECK(!h_hipc_Send(h,w,sizeof w)&&w[r/4+2]==RES_FAIL);
 }
 request(w,0,1,205,2);w[(20+12)/4]=1280u*768u*4u;CHECK(!h_hipc_Send(h,w,sizeof w)&&!w[6]);
 request(w,0,1,203,1);save_ok=0;CHECK(!h_hipc_Send(h,w,sizeof w)&&w[6]==RES_FAIL);
 request(w,0,0,32,0);CHECK(h_hipc_Send(h,w,127)==RES_FAIL);CHECK(h_hipc_Send(h,NULL,256)==RES_FAIL);
 memset(w,0,sizeof w);w[0]=0xffff0006;CHECK(h_hipc_Send(h,w,128)==RES_FAIL);
 uint32_t u=0;CHECK(!h_hipc_Connect(&u,"caps:u")&&u==ACNH_CAPSU_HANDLE);request(w,0,1,32,0);CHECK(!h_hipc_Send(u,w,sizeof w)&&!w[6]);request(w,0,1,130,0);CHECK(!h_hipc_Send(u,w,sizeof w)&&!w[6]);request(w,0,1,203,1);CHECK(!h_hipc_Send(u,w,sizeof w)&&w[6]==RES_FAIL);
 printf("CAPS: %d checks, %d failures, %d save requests\n",checks,failures,saves);return !!failures;}
