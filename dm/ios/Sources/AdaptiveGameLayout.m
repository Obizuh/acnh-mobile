#import "AdaptiveGameLayout.h"
#import <objc/runtime.h>
static char hingeKey,observerKey;
BOOL acnh_has_hinge(UIView *view) {
 return [objc_getAssociatedObject(view.window.windowScene,&hingeKey) boolValue];
}
CGFloat acnh_view_display_scale(UIView *view) {
 CGFloat scale=acnh_has_hinge(view)?view.traitCollection.displayScale:view.window.windowScene.screen.nativeScale;
 return scale>0?scale:MAX(1,view.traitCollection.displayScale);
}
CGRect acnh_game_frame(UIView *view) {
 CGRect b=view.bounds;
 if(acnh_has_hinge(view))b=UIEdgeInsetsInsetRect(b,view.safeAreaInsets);
 return acnh_fit_game(b);
}
void acnh_observe_display(UIView *view) {
#if __IPHONE_OS_VERSION_MAX_ALLOWED >= 270100
 if(@available(iOS 27.1,*)) {
  if(objc_getAssociatedObject(view,&observerKey))return;
  __weak UIView *weakView=view;
  UIHingeInteraction *observer=[[UIHingeInteraction alloc] initWithUpdateHandler:^(UIHingeInteraction *interaction,UIHingeInteractionUpdate *update){
   (void)interaction;UIView *v=weakView;UIWindowScene *scene=v.window.windowScene;
   if(!v||!scene||!update.hinge)return;
   // Retain capability across display transitions; no model IDs, pixel-size tests or angle-based layout.
   if(![objc_getAssociatedObject(scene,&hingeKey) boolValue]){
    objc_setAssociatedObject(scene,&hingeKey,@YES,OBJC_ASSOCIATION_RETAIN_NONATOMIC);
    NSLog(@"[display] hinge-capable scene; adaptive controls enabled");
    [v setNeedsLayout];[v.window.rootViewController.view setNeedsLayout];
   }
  }];
  objc_setAssociatedObject(view,&observerKey,observer,OBJC_ASSOCIATION_RETAIN_NONATOMIC);[view addInteraction:observer];
 }
#endif
}
CGPoint acnh_control_point(UIView *view,CGPoint point,CGFloat radius) {
 if(!acnh_has_hinge(view))return point;
 CGRect safe=UIEdgeInsetsInsetRect(view.bounds,view.safeAreaInsets);CGRect regions[32];int count=0;
#if __IPHONE_OS_VERSION_MAX_ALLOWED >= 270100
 if(@available(iOS 27.1,*)) {
  for(UIViewReservedRegionKind *kind in @[[UIViewReservedRegionKind occlusionRegionKind],[UIViewReservedRegionKind divisionRegionKind]]){
   for(UIViewReservedRegion *region in [view reservedRegionsOfKind:kind]){
    if(region.active&&!CGRectIsEmpty(region.frame)&&!CGRectIsInfinite(region.frame)&&count<32)regions[count++]=region.frame;
   }
  }
 }
#endif
 return acnh_clear_control(point,safe,radius,regions,count);
}
