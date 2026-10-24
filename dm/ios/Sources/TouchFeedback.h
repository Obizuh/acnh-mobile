#pragma once
#import <UIKit/UIKit.h>
#include <stdlib.h>
#include "TouchFeedbackLogic.h"
static inline UIImpactFeedbackGenerator *touch_feedback_create(BOOL editing){
 const char *value=getenv("DM_TOUCH_HAPTICS");
 if(editing || (value && !atoi(value)))return nil;
 UIImpactFeedbackGenerator *feedback=[[UIImpactFeedbackGenerator alloc] initWithStyle:UIImpactFeedbackStyleLight];
 [feedback prepare];return feedback;
}
static inline void touch_feedback_update(UIImpactFeedbackGenerator *feedback,TouchFeedbackEdges *state,uint64_t buttons,unsigned sticks,BOOL swap){
 if(touch_feedback_edge(state,buttons,sticks,swap)){[feedback impactOccurredWithIntensity:.6];[feedback prepare];}
}
