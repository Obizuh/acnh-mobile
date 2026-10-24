#include "../Sources/TouchFeedbackLogic.h"
#include <assert.h>
#include <stdio.h>
int main(void){TouchFeedbackEdges s={0};assert(!touch_feedback_edge(&s,0,0,0));assert(touch_feedback_edge(&s,1,0,0));assert(!touch_feedback_edge(&s,1,0,0));assert(!touch_feedback_edge(&s,0,0,0));assert(touch_feedback_edge(&s,1,0,0));assert(touch_feedback_edge(&s,1,1,0));assert(!touch_feedback_edge(&s,1,1,0));assert(touch_feedback_edge(&s,1,3,0));assert(!touch_feedback_edge(&s,0,0,0));assert(touch_feedback_edge(&s,0,0,1));assert(!touch_feedback_edge(&s,0,0,1));puts("touch feedback press/release/hold/multi-touch edges passed");}
