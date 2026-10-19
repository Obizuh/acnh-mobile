#pragma once
#include <stdint.h>
#include <math.h>
#include <string.h>
// Host-side event coalescing; guest sample frequency never schedules UIKit work.
typedef struct {
    float values[2][4], peak;
    uint64_t last_signal, burst_start, last_pulse;
    unsigned pulses;
} dm_vibration_state;
static inline void dm_vibration_state_send(dm_vibration_state *s,unsigned side,const float value[4],uint64_t now) {
    if(side>1||!value)return;
    for(int i=0;i<4;i++)s->values[side][i]=isfinite(value[i])?fminf(fmaxf(value[i],0),i%2?1250:1):0;
    float peak=fmaxf(fmaxf(s->values[0][0],s->values[0][2]),fmaxf(s->values[1][0],s->values[1][2]));
    if(peak<=.03f)return;
    if(!s->last_signal||now<s->last_signal||now-s->last_signal>200000000ull){s->burst_start=now;s->pulses=0;}
    s->last_signal=now;s->peak=fmaxf(s->peak,peak);
}
static inline float dm_vibration_state_take(dm_vibration_state*s,uint64_t now) {
    if(!s->peak)return 0;
    if(now<s->last_signal||now-s->last_signal>200000000ull||now<s->burst_start||now-s->burst_start>1500000000ull||s->pulses>=8){s->peak=0;return 0;}
    if(s->last_pulse&&now>=s->last_pulse&&now-s->last_pulse<120000000ull)return 0;
    float peak=s->peak;s->peak=0;s->last_pulse=now;s->pulses++;return peak;
}
// Main-thread frontend polling and lifecycle reset. Non-iPhone hosts expose no devices.
float dm_vibration_take(uint64_t now);
void dm_vibration_reset(void);
int front_vibration_available(void);
