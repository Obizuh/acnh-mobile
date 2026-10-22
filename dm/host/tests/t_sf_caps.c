#include "../hle_sf.c"
#include <assert.h>
static int captured;
int dm_caps_photo_space(void){return 1;}
int dm_caps_save_rgba(const void *p,size_t n,uint8_t entry[32]){(void)p;(void)n;memset(entry,0x42,32);captured++;return 1;}
int main(void){
 uint32_t h=0,copy=0;uint16_t size=9;assert(!h_sf_Connect(&h,"caps:su"));assert(!h_sf_PtrBuf(&size,h)&&!size);assert(!h_sf_Dup(&copy,h));
 uint32_t m[128]={0};m[0]=4|(1u<<20);m[1]=32;unsigned d=8;uint64_t address=0x12345000;
 m[d/4]=1280*720*4;m[d/4+1]=(uint32_t)address;m[d/4+2]=0;
 m[8]=0x49434653;m[10]=203;assert(!h_sf_Send(copy,m,sizeof m));assert(captured==1 && m[6]==0 && ((uint8_t*)m)[32]==0x42);
 h_sf_Close(h);h_sf_Close(copy);assert(sf_slot(h)<0&&sf_slot(copy)<0);
 assert(h_sf_Connect(&h,"unavailable")!=0);
 puts("PASS: capture service uses official session creation, pointer buffer, duplication, request routing and close");return 0;
}
