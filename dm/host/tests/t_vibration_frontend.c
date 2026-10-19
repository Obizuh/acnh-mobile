int test_haptic_available=1;int front_vibration_available(void){return test_haptic_available;}
#include <stdint.h>
uint64_t dm_acnh_live_buttons(void){return 0;}
void dm_acnh_keyboard_state(void*p){(void)p;}
int dm_touch_mouse_fill(void*p){(void)p;return 0;}
void hle_bind_system_event(void*p,int c){(void)p;(void)c;}
