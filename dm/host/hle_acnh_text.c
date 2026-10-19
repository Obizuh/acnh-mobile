// 动森原生输入桥：系统输入的 UTF-16 经游戏已有的硬件键盘路径提交。
// ACNH native text bridge: submit system UTF-16 through the game's existing physical-keyboard path.
#include "hle.h"
#include <TargetConditionals.h>
#include <pthread.h>
static pthread_mutex_t text_mu=PTHREAD_MUTEX_INITIALIZER;
static uint16_t text_queue[256],text_current;
static size_t text_count,text_pos;
static int text_held,text_read;
static uint64_t text_next,text_press_at;
static int acnh_text_supported(void){return !strcmp(dm_main_build(),"ff1d1c05670db6021c85b624a710b963");}
int dm_acnh_text_submit(const uint16_t *units,size_t count){
    if(!acnh_text_supported()||!units||!count||count>256)return 0;
    pthread_mutex_lock(&text_mu);
    if(text_pos<text_count||text_held){pthread_mutex_unlock(&text_mu);return 0;}
    memcpy(text_queue,units,count*2);text_count=count;text_pos=0;text_held=text_read=0;text_next=0;
    pthread_mutex_unlock(&text_mu);LOG("ACNH native text: queued %zu UTF16 units",count);return 1;
}
void dm_acnh_text_cancel(void){pthread_mutex_lock(&text_mu);text_count=text_pos=0;text_held=text_read=0;pthread_mutex_unlock(&text_mu);}
void dm_acnh_keyboard_state(void *state){
    if(!acnh_text_supported())return;
    static int script_done;static uint64_t start;
    uint64_t now=now_ns();if(!start)start=now;
#if !TARGET_OS_IPHONE
    if(now-start>10000000000ull){void dm_acnh_movie_probe(void);dm_acnh_movie_probe();}
#endif
    static uint64_t diag; if(getenv("ACNH_KEYBOARD_DIAG") && now-diag>1000000000ull){diag=now;LOG("ACNH keyboard polled at %.2fs",(now-start)/1e9);}
    const char *script=getenv("ACNH_TEXT_SCRIPT"),*at=getenv("ACNH_TEXT_AT");
    if(script&&!script_done && now-start>(uint64_t)(at?atof(at):30)*1000000000ull){uint16_t units[256];size_t swkbd_u8_to_u16(const char *,size_t,uint16_t *,size_t);size_t n=swkbd_u8_to_u16(script,255,units,256);dm_acnh_text_submit(units,n);script_done=1;}
    pthread_mutex_lock(&text_mu);
    if(text_held && !text_read && now-text_press_at>100000000ull){text_held=0;text_next=now+70000000ull;}
    if(text_held && text_read){text_held=text_read=0;text_pos++;text_next=now+70000000ull;}
    else if(!text_held && text_pos<text_count && now>=text_next){text_current=text_queue[text_pos];text_held=1;text_read=0;text_press_at=now;}
    if(text_pos<text_count||text_held){((uint32_t *)state)[3]=1;if(text_held)((uint8_t *)state)[16]|=1u<<4;}
    pthread_mutex_unlock(&text_mu);
}
HLE_FORCE("_ZN2nn3hid10GetKeyCodeEPtiNS_4util10BitFlagSetILi32ENS0_16KeyboardModifierEEENS2_7BitPackIjNS0_14KeyboardLayoutEEE","动森原生 Unicode 输入；其他按键保持 SDK 转换",void,h_acnh_GetKeyCode,uint16_t *out,int key,uint32_t mods,uint32_t layout){
    pthread_mutex_lock(&text_mu);
    if(acnh_text_supported() && key==4 && text_held){*out=text_current;if(!text_read)LOG("ACNH native text: delivered U+%04x",text_current);text_read=1;pthread_mutex_unlock(&text_mu);return;}
    pthread_mutex_unlock(&text_mu);
    static void (*orig)(uint16_t *,int,uint32_t,uint32_t);if(!orig)orig=guest_sym("_ZN2nn3hid10GetKeyCodeEPtiNS_4util10BitFlagSetILi32ENS0_16KeyboardModifierEEENS2_7BitPackIjNS0_14KeyboardLayoutEEE");
    if(orig)orig(out,key,mods,layout);else *out=0;
}
