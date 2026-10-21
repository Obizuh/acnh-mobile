#include "../hle_nfp.c"
static int starts,stops,bindings,checks;
static uint64_t token;
void front_amiibo_file_start(uint64_t t){starts++;token=t;}
void front_amiibo_file_stop(void){stops++;}
void front_nfc_probe_start(void){}
void front_nfc_probe_stop(void){}
void hle_bind_system_event(void *event,int clear){(void)event;if(clear)abort();bindings++;}
void hle_destroy_system_event(void *event){(void)event;abort();}
void *guest_sym(const char *name){(void)name;return NULL;}
uint32_t hle_hid_connected_npad(void){return 32;}
#define CHECK(x) do{checks++;if(!(x)){fprintf(stderr,"failed line %d\n",__LINE__);return 1;}}while(0)
int main(int argc,char **argv){
 unsetenv("ACNH_NFP_EVENT_SELFTEST");NfpDeviceHandle dev={0};int n=99;uint32_t npad=0;unsigned char event[128]={0},buffer[96];
 CHECK(h_nfp_GetLibraryState()==0);h_nfp_Init();CHECK(h_nfp_GetLibraryState()==1);
 CHECK(h_nfp_List(&dev,&n,1)==0 && n==1 && dev.id==NFP_DEVICE_ID);
 CHECK(h_nfp_NpadId(&npad,&dev)==0 && npad==32);
 h_nfp_AttachAvail(event);CHECK(bindings==1);
 for(int i=0;i<64;i++){
  CHECK(h_nfp_Start(&dev)==0 && h_nfp_State(&dev)==NFP_STATE_SEARCH);
  CHECK(starts==i+1);
  memset(buffer,0xa5,sizeof buffer);CHECK(h_nfp_TagInfo(buffer,&dev)==NFP_RES_TAG_NOT_FOUND);
  for(int j=0;j<64;j++)CHECK(buffer[j]==0xa5);
  CHECK(h_nfp_Stop(&dev)==0 && h_nfp_State(&dev)==NFP_STATE_INIT);
  CHECK(stops==i+1);
 }
 CHECK(h_nfp_Start(&dev)==0);h_nfp_Finalize();CHECK(h_nfp_GetLibraryState()==0 && h_nfp_State(&dev)==NFP_STATE_INIT);CHECK(stops==65);
 h_nfp_Init();CHECK(h_nfp_Start(&dev)==0);CHECK(starts==66);h_nfp_Stop(&dev);h_nfp_Finalize();
 uint64_t size=99;CHECK(h_nfp_GetArea(buffer,&size,&dev,sizeof buffer)==NFP_RES_TAG_NOT_FOUND && size==0);

 unsigned char raw[573]={0};raw[0]=4;raw[1]=0x12;raw[2]=0x34;raw[3]=0x88^raw[0]^raw[1]^raw[2];
 raw[4]=1;raw[5]=2;raw[6]=3;raw[7]=4;raw[8]=1^2^3^4;raw[9]=0x48;
 raw[12]=0xf1;raw[13]=0x10;raw[14]=0xff;raw[15]=0xee;
 const uint8_t id[8]={1,2,3,1,4,5,6,2};memcpy(raw+0x54,id,8);
 CHECK(hle_nfp_import_file(token,raw,540)==-1); // finalized
 h_nfp_Init();h_nfp_Start(&dev);uint64_t old=token;
 CHECK(hle_nfp_import_file(token,NULL,540)==-2);
 for(size_t len=0;len<=573;len++)if(len!=532 && len!=540 && len!=572)CHECK(hle_nfp_import_file(token,raw,len)==-2);
 for(int offset=0;offset<16;offset++){if(offset==10 || offset==11)continue;raw[offset]^=0xff;CHECK(hle_nfp_import_file(token,raw,540)==-2);raw[offset]^=0xff;}
 raw[0x5b]=0;CHECK(hle_nfp_import_file(token,raw,540)==-2);raw[0x5b]=2;
 CHECK(h_nfp_State(&dev)==NFP_STATE_SEARCH);CHECK(hle_nfp_import_file(token,raw,540)==0);
 CHECK(h_nfp_State(&dev)==NFP_STATE_FOUND);CHECK(hle_nfp_import_file(token,raw,540)==-1);
 memset(buffer,0xa5,sizeof buffer);CHECK(h_nfp_TagInfo(buffer,&dev)==0);
 CHECK(buffer[0]==4 && buffer[3]==1 && buffer[6]==4 && buffer[10]==7 && buffer[0x20]==1 && buffer[0x24]==2);
 for(int i=0x58;i<96;i++)CHECK(buffer[i]==0xa5);
 CHECK(h_nfp_ModelInfo(buffer,&dev)==NFP_RES_TAG_NOT_FOUND);
 CHECK(h_nfp_Mount(&dev,0,3)==NFP_RES_TAG_NOT_FOUND);CHECK(h_nfp_Mount(&dev,1,1)==NFP_RES_TAG_NOT_FOUND);
 CHECK(h_nfp_Mount(&dev,0,1)==0 && h_nfp_State(&dev)==NFP_STATE_MOUNTED);
 memset(buffer,0xa5,sizeof buffer);CHECK(h_nfp_ModelInfo(buffer,&dev)==0);
 const uint8_t model[7]={1,2,3,6,5,4,1};CHECK(!memcmp(buffer,model,7));
 for(int i=7;i<64;i++)CHECK(buffer[i]==0);for(int i=64;i<96;i++)CHECK(buffer[i]==0xa5);
 CHECK(h_nfp_OpenArea(&dev,0)==NFP_RES_TAG_NOT_FOUND);CHECK(h_nfp_Flush(&dev)==NFP_RES_TAG_NOT_FOUND);
 CHECK(h_nfp_Unmount(&dev)==0 && h_nfp_State(&dev)==NFP_STATE_FOUND);
 h_nfp_Stop(&dev);h_nfp_Start(&dev);CHECK(token!=old);CHECK(hle_nfp_import_file(old,raw,540)==-1);
 CHECK(hle_nfp_import_file(token,raw,532)==0);h_nfp_Stop(&dev);h_nfp_Start(&dev);CHECK(hle_nfp_import_file(token,raw,572)==0);
 h_nfp_Stop(&dev);h_nfp_Start(&dev);
 raw[12]=0xe1;raw[13]=0x10;raw[14]=0x3e;raw[15]=0;
 CHECK(hle_nfp_import_file(token,raw,540)==0); // generic container backup remains supported
 h_nfp_Finalize();CHECK(hle_nfp_import_file(token,raw,540)==-1);

 // External fixtures stay private. Reference output follows the documented
 // NFP wire fields (opaque character bytes; only model number byte-swapped).
 int files=0;
 for(int f=1;f<argc;f++){
  FILE *fp=fopen(argv[f],"rb");CHECK(fp!=NULL);size_t len=fread(raw,1,sizeof raw,fp);CHECK(!ferror(fp));fclose(fp);
  h_nfp_Init();h_nfp_Start(&dev);CHECK(hle_nfp_import_file(token,raw,len)==0);
  CHECK(h_nfp_Mount(&dev,0,1)==0);CHECK(h_nfp_ModelInfo(buffer,&dev)==0);
  CHECK(!memcmp(buffer,raw+0x54,3));CHECK(buffer[3]==raw[0x5a]);
  CHECK(buffer[4]==raw[0x59] && buffer[5]==raw[0x58] && buffer[6]==raw[0x57]);
  h_nfp_Finalize();files++;
 }
 if(files)printf("Private raw amiibo fixtures: %d passed\n",files);
 printf("NFP transport lifecycle: %d checks passed\n",checks);return 0;
}
