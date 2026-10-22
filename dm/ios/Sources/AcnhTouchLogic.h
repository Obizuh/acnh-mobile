// Touch axes and tool-wheel timing, advanced by the main-thread clock; cancel never confirms.
#pragma once
#include <math.h>
#include <stdint.h>
#include <stdbool.h>
typedef struct {float x,y;} AcnhAxis;
static inline AcnhAxis acnh_axis(float dx,float dy,float radius){
 float n=hypotf(dx,dy),dead=radius*.12f;if(n<=dead||radius<=0)return (AcnhAxis){0,0};
 float strength=fminf(1,(n-dead)/(radius-dead));return (AcnhAxis){dx/n*strength,-dy/n*strength};
}
typedef struct {bool active,held,directed;double began,released;AcnhAxis axis;} AcnhWheel;
typedef struct {bool active;uint64_t buttons;AcnhAxis axis;} AcnhWheelOutput;
static inline void acnh_wheel_begin(AcnhWheel *w,double now){*w=(AcnhWheel){.active=true,.held=true,.began=now};}
static inline void acnh_wheel_move(AcnhWheel *w,float dx,float dy,float radius){
 if(!w->held)return;AcnhAxis a=acnh_axis(dx,dy,radius);w->directed=hypotf(a.x,a.y)>.2f;w->axis=w->directed?a:(AcnhAxis){0,0};
}
static inline void acnh_wheel_end(AcnhWheel *w,double now){w->held=false;w->released=now;}
static inline void acnh_wheel_cancel(AcnhWheel *w){*w=(AcnhWheel){0};}
static inline AcnhWheelOutput acnh_wheel_sample(AcnhWheel *w,double now){
 AcnhWheelOutput out={.active=w->active};if(!w->active)return out;
 if(now<w->began+.12){out.buttons=1ull<<13;return out;}
 if(!w->held&&!w->directed){acnh_wheel_cancel(w);return (AcnhWheelOutput){0};}
 if(now<w->began+.35)return out;
 double confirm=fmax(w->began+.45,w->released+.08);
 if(w->held||now<confirm){out.axis=w->axis;return out;}
 if(now<confirm+.14){out.buttons=1ull;return out;}
 acnh_wheel_cancel(w);return (AcnhWheelOutput){0};
}
