#pragma once
#include <stdint.h>
// A one-shot probe on a field tap or R press. Inspect only the in-game keyboard,
// which is shared by player names, island names and chat; never sample every frame.
static inline int acnh_text_keyboard(const uint8_t *pixels) {
    int brown=0,tan=0,yellow=0;
    for(int y=50;y<=53;y++)for(int x=40;x<150;x++){
        const uint8_t *p=pixels+4*(y*192+x);
        brown+=p[0]>125&&p[0]<200&&p[1]>95&&p[1]<175&&p[2]>60&&p[2]<135&&p[0]>p[1]+8&&p[1]>p[2]+15;
    }
    for(int row=0;row<3;row++)for(int y=60+row*9;y<=62+row*9;y++)for(int x=35;x<150;x++){
        const uint8_t *p=pixels+4*(y*192+x);
        tan+=p[0]>195&&p[1]>155&&p[1]<220&&p[2]>100&&p[2]<180&&p[0]>p[1]+12&&p[1]>p[2]+25;
    }
    for(int y=92;y<=95;y++)for(int x=60;x<145;x++){
        const uint8_t *p=pixels+4*(y*192+x);
        yellow+=p[0]>200&&p[1]>145&&p[1]<215&&p[2]<150&&p[0]>p[1]+20&&p[1]>p[2]+45;
    }
    return brown>=330&&tan>=550&&yellow>=250;
}
