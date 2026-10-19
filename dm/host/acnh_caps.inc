// Local screenshot service. Protocol references: switchbrew/libnx capssu.c and sf/hipc.h.
// The game supplies the captured RGBA pixels; save those exact pixels, not a later frame.
#define ACNH_CAPS_HANDLE 0x63617073u
#define ACNH_CAPSU_HANDLE 0x63617075u
extern int dm_caps_photo_space(void);
extern int dm_caps_save_rgba(const void *, size_t, uint8_t [32]);
static Result acnh_caps_request(uint32_t handle, void *buf, size_t size) {
 if(!buf || size<0x80)return RES_FAIL;
 uint32_t *w=buf;unsigned type=w[0]&0xffff, pos=8;
 if(type==2){memset(buf,0,0x80);return RES_OK;}
 if(w[1]>>31){uint32_t special=w[2];pos+=4+((special&1)?8:0)+(((special>>1)&15)+((special>>5)&15))*4;}
 unsigned statics=(w[0]>>16)&15, sends=(w[0]>>20)&15, recvs=(w[0]>>24)&15, exchanges=(w[0]>>28)&15;
 unsigned descriptors=pos+statics*8;
 pos=descriptors+(sends+recvs+exchanges)*12;pos=(pos+15)&~15u;
 if(pos+16>size)return RES_FAIL;
 int control=type==5||type==7;
 int domain=!control && w[pos/4]!=0x49434653u;
 if(domain){if(pos+32>size)return RES_FAIL;if(((uint8_t*)buf)[pos]!=1)return RES_FAIL;}
 unsigned cmif=pos+(domain?16:0);
 if(cmif+16>size || w[cmif/4]!=0x49434653u)return RES_FAIL;
 unsigned cmd=w[cmif/4+2],token=w[cmif/4+3];uint8_t entry[32]={0};Result result=RES_FAIL;
 if(cmd==203||cmd==205||cmd==210)LOG("CAPS incoming cmd=%u static=%u send=%u recv=%u size=%zu cmif=%u",cmd,statics,sends,recvs,size,cmif);
 if(control && (cmd==0||cmd==3))result=RES_OK;
 else if((type==4||type==6)&&cmd==32 && cmif+32<=size)result=RES_OK;
 else if(handle==ACNH_CAPSU_HANDLE && (type==4||type==6) && cmd==130)result=dm_caps_photo_space()?RES_OK:RES_FAIL;
 else if(handle==ACNH_CAPS_HANDLE && (type==4||type==6)&&(cmd==203||cmd==205||cmd==210)&&cmif+16+0x50<=size && sends>=1){
  LOG("CAPS capture descriptors static=%u send=%u recv=%u size=%zu cmif=%u",statics,sends,recvs,size,cmif);
  unsigned d=descriptors+(sends-1)*12;
  if(d+12<=size){uint32_t *b=(uint32_t*)((char*)buf+d);
   uint64_t address=b[1]|((uint64_t)(b[2]>>28)<<32)|((uint64_t)((b[2]>>2)&0x3fffff)<<36);
   uint64_t bytes=b[0]|((uint64_t)((b[2]>>24)&15)<<32);
   LOG("CAPS capture buffer address=%#llx bytes=%llu",(unsigned long long)address,(unsigned long long)bytes);
   if(bytes==1280u*720u*4u || bytes==1280u*768u*4u || bytes==1920u*1080u*4u || bytes==1920u*1088u*4u)result=dm_caps_save_rgba((const void*)(uintptr_t)address,(size_t)bytes,entry)?RES_OK:RES_FAIL;
  }
 }
 LOG("CAPS local screenshot type=%u domain=%d cmd=%u result=%#x",type,domain,cmd,result);
 memset(buf,0,0x80);w[1]=(16+(domain?16:0)+16+32)/4;
 unsigned reply=16+(domain?16:0);w[reply/4]=0x4f434653u;w[reply/4+2]=result;w[reply/4+3]=token;
 if(control&&cmd==0)w[reply/4+4]=1;
 else if(!control && !result && cmd!=32)memcpy((char*)buf+reply+16,entry,sizeof entry);
 return RES_OK;
}
