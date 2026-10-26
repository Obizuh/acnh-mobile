#include "../Sources/AdaptiveLayoutMath.h"
#include <assert.h>
#include <stdio.h>
static void fit(CGRect bounds){
 CGRect f=acnh_fit_game(bounds);
 assert(fabs(f.size.width/f.size.height-16./9.)<1e-10);
 assert(CGRectContainsRect(bounds,f));
 assert(fabs(f.size.width-bounds.size.width)<1e-8||fabs(f.size.height-bounds.size.height)<1e-8);
 assert(fabs(CGRectGetMidX(f)-CGRectGetMidX(bounds))<1.01);
 assert(fabs(CGRectGetMidY(f)-CGRectGetMidY(bounds))<1.01);
}
int main(void){
 // Official outer/inner pixel bounds, landscape phones/tablets, portrait and window transitions.
 CGSize screens[]={{2034,1398},{2670,1878},{932,430},{1024,768},{1366,1024},{1398,2034}};
 for(unsigned i=0;i<sizeof screens/sizeof *screens;i++){
  fit(CGRectMake(0,0,screens[i].width,screens[i].height));
  fit(CGRectMake(37,13,screens[i].width-74,screens[i].height-26));
 }
 for(int i=0;i<640;i++)fit(CGRectMake(0,0,678+i/3.,466+i/4.));
 assert(acnh_inset_control_x(.096,0)==.096);
 assert(acnh_inset_control_x(.904,0)==.904);
 assert(fabs(acnh_inset_control_x(.096,1)-.121)<1e-9);
 assert(fabs(acnh_inset_control_x(.904,1)-.879)<1e-9);
 assert(acnh_inset_control_x(.5,1)==.5);
 CGRect safe=CGRectMake(0,0,678,381.375),hole=CGRectMake(305,0,68,40);
 CGPoint center=CGPointMake(339,22),p=acnh_clear_control(center,safe,20,&hole,1);
 assert(!acnh_control_blocked(p,20,&hole,1));assert(CGRectContainsPoint(safe,p));
 assert(hypot(p.x-center.x,p.y-center.y)>0);
 p=acnh_clear_control(CGPointMake(-99,999),safe,36,NULL,0);
 assert(p.x>=39&&p.y<=CGRectGetMaxY(safe)-39);
 puts("adaptive layout: 652 fit cases, legacy/inset positions and reserved-region checks passed");
}
