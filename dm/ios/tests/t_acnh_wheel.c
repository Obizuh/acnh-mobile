#include "../Sources/AcnhTouchLogic.h"
#include <assert.h>
#include <stdio.h>
static void check(unsigned key){
 AcnhWheel w;acnh_wheel_begin_button(&w,0,key);
 assert(acnh_wheel_sample(&w,.05).buttons==(1ull<<key));
 acnh_wheel_move(&w,50,0,40);
 assert(acnh_wheel_sample(&w,.4).axis.x==1);
 acnh_wheel_end(&w,.5);
 assert(acnh_wheel_sample(&w,.55).axis.x==1);
 assert(acnh_wheel_sample(&w,.6).buttons==1);
 assert(!acnh_wheel_sample(&w,.8).active);
 acnh_wheel_begin_button(&w,1,key);acnh_wheel_end(&w,1.02);
 assert(acnh_wheel_sample(&w,1.05).buttons==(1ull<<key));
 assert(!acnh_wheel_sample(&w,1.2).active);
 acnh_wheel_begin_button(&w,2,key);acnh_wheel_move(&w,0,-50,40);
 assert(acnh_wheel_sample(&w,2.4).axis.y==1);
 acnh_wheel_move(&w,0,0,40);acnh_wheel_end(&w,2.5);
 assert(!acnh_wheel_sample(&w,2.6).active);
 acnh_wheel_begin_button(&w,3,key);acnh_wheel_move(&w,50,0,40);acnh_wheel_cancel(&w);
 assert(!acnh_wheel_sample(&w,3.6).active);
}
int main(void){check(13);check(9);puts("tool/reaction wheel timing, tap, direction, recenter and cancellation passed");}
