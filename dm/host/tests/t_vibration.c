// Game vibration API, transient preservation, mixing, rate/burst limits and lifecycle reset.
#ifndef HLE_HID_SOURCE
#define HLE_HID_SOURCE "../hle_hid.c"
#endif
#include HLE_HID_SOURCE
module_t g_mod[MAX_MOD]; int g_nmod;
#include <unistd.h>
#include <sys/wait.h>
static uint64_t clock_ns=1000000000ull;
uint64_t dm_guest_frame(void) { return 0; }
uint64_t dm_server_tick(void) { return UINT64_MAX; }
int dm_loadscreen_up(void) { return 0; }
void dm_hold_for_loadscreen(void) {}
uint64_t now_ns(void) { return clock_ns; }
uint64_t guest_ns(void) { return clock_ns; }
static int opmode=1;
int dm_opmode(void) { return opmode; }
int touch_fill(void *p, uint64_t n) { (void)p;(void)n;return 0; }
void dm_touch_set(const dm_touch *p,int n) { (void)p;(void)n; }
void uimsg_subscribe(void (*f)(const char *,const char *,const char *)) { (void)f; }
static int checks,fails;
#define CK(c) do{checks++;if(!(c)){fails++;printf("FAIL line=%d %s\n",__LINE__,#c);}}while(0)
extern int test_haptic_available;
int main(void){
 uint32_t id=0x20,handles[3]={0,0,0xcafebabe},bad=9;float v[4]={.7f,160,.4f,320},got[4];
 CK(h_hid_VibPermit()==1);CK(h_hid_VibHandles(handles,2,&id,STYLE_HANDHELD)==2);CK(handles[2]==0xcafebabe);CK(handles[0]!=handles[1]);
 uint32_t info[2];h_hid_VibInfo(info,&handles[1]);CK(info[0]==1&&info[1]==2);
 h_hid_SendVib(&handles[0],v);h_hid_VibActual(got,&handles[0]);CK(!memcmp(v,got,sizeof v));CK(fabsf(dm_vibration_take(clock_ns)-.7f)<.001f);CK(!dm_vibration_take(clock_ns));
 CK(h_hid_VibHandles(handles,0,&id,STYLE_HANDHELD)==0);CK(h_hid_VibHandles(NULL,2,&id,STYLE_HANDHELD)==0);CK(h_hid_VibHandles(handles,2,&bad,STYLE_HANDHELD)==0);
 id=0;CK(h_hid_VibHandles(handles,1,&id,STYLE_FULLKEY)==1);CK(h_hid_VibHandles(handles,2,&id,STYLE_JOYRIGHT)==1&&handles[0]==0x444d0001);
 test_haptic_available=0;CK(!h_hid_VibPermit());CK(!h_hid_VibHandles(handles,2,&id,STYLE_FULLKEY));test_haptic_available=1;
 dm_vibration_reset();CK(!dm_vibration_take(clock_ns));
 CK(h_hid_EnqueueVib(&handles[0],v)==RES_OK);CK(h_hid_FlushVib()==RES_OK);CK(dm_vibration_take(clock_ns)>.6f);
 dm_vibration_state state={0};uint64_t t=1000000000ull;float zero[4]={0,160,0,320};
 dm_vibration_state_send(&state,0,v,t);dm_vibration_state_send(&state,0,zero,t+1000000);CK(fabsf(dm_vibration_state_take(&state,t+50000000)-.7f)<.001f);
 dm_vibration_state_send(&state,0,v,t+60000000);CK(!dm_vibration_state_take(&state,t+100000000));CK(dm_vibration_state_take(&state,t+180000000)>.6f);
 memset(&state,0,sizeof state);int pulses=0;
 for(int k=0;k<200;k++){uint64_t now=t+k*10000000ull;dm_vibration_state_send(&state,k%2,v,now);pulses+=dm_vibration_state_take(&state,now)>0;}
 CK(pulses==8);CK(!dm_vibration_state_take(&state,t+2100000000ull));dm_vibration_state_send(&state,0,v,t+2500000000ull);CK(dm_vibration_state_take(&state,t+2500000000ull)>.6f);
 memset(&state,0,sizeof state);dm_vibration_state_send(&state,0,v,t);CK(!dm_vibration_state_take(&state,t+210000000));
 memset(&state,0,sizeof state);dm_vibration_state_send(&state,2,v,t);CK(!state.peak);
 float invalid[4]={NAN,INFINITY,-10,-2};dm_vibration_state_send(&state,0,invalid,t);CK(!state.peak);CK(state.values[0][0]==0&&state.values[0][1]==0&&state.values[0][2]==0&&state.values[0][3]==0);
 float large[4]={10,9000,10,9000};dm_vibration_state_send(&state,0,large,t);CK(state.values[0][0]==1&&state.values[0][1]==1250);CK(dm_vibration_state_take(&state,t)==1);
 memset(&state,0,sizeof state);dm_vibration_state_send(&state,0,v,t);dm_vibration_state_send(&state,1,large,t);CK(dm_vibration_state_take(&state,t)==1);
 memset(&state,0,sizeof state);float noise[4]={.01,160,.02,320};dm_vibration_state_send(&state,0,noise,t);CK(!dm_vibration_state_take(&state,t));
 dm_vibration_reset();h_hid_VibActual(got,&handles[0]);CK(got[0]==0&&got[2]==0);
 printf("haptics: %d checks, %d failures\n",checks,fails);return !!fails;
}
