#pragma once
#include <stdint.h>
#include <stdbool.h>
typedef struct {uint64_t buttons;unsigned sticks;bool swap;} TouchFeedbackEdges;
static inline bool touch_feedback_edge(TouchFeedbackEdges *state,uint64_t buttons,unsigned sticks,bool swap){
 bool pressed=(buttons&~state->buttons)||(sticks&~state->sticks)||(swap&&!state->swap);
 *state=(TouchFeedbackEdges){buttons,sticks,swap};return pressed;
}
