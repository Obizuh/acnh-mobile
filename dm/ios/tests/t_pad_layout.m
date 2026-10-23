#import "../Sources/PadLayoutMath.h"
#include <assert.h>
int main(void){@autoreleasepool{
 CGRect b=CGRectMake(0,0,932,430),safe=CGRectMake(59,0,814,409);CGPoint original=CGPointMake(110,327);
 assert(CGPointEqualToPoint(pad_point(@{},@"stick",original,b,safe,77),original));
 NSMutableDictionary *draft=[@{} mutableCopy];draft[@"stick"]=@{@"dx":@.2,@"dy":@-.1,@"scale":@1.5};
 assert(pad_scale(draft,@"stick")==1.5);CGPoint p=pad_point(draft,@"stick",original,b,safe,77);assert(fabs(p.x-296.4)<.001 && fabs(p.y-284)<.001);
 assert(pad_scale(draft,@"button.0")==1);assert(CGPointEqualToPoint(pad_point(draft,@"button.0",original,b,safe,77),original));
 NSDictionary *saved=draft.copy;[draft removeAllObjects];assert(pad_scale(saved,@"stick")==1.5);assert(pad_scale(draft,@"stick")==1);assert(CGPointEqualToPoint(pad_point(draft,@"stick",original,b,safe,77),original));
 for(id bad in @[@"broken",@{@"scale":@100},@{@"scale":@(-100)},@{@"scale":@(NAN)},@{@"scale":@"invalid"}]){CGFloat n=pad_scale(@{@"stick":bad},@"stick");assert(n>=.5 && n<=2);}
 p=pad_point(@{@"stick":@{@"dx":@100,@"dy":@(-100)}},@"stick",original,b,safe,77);assert(p.x<=CGRectGetMaxX(safe)-79 && p.y>=79);
 NSString *suite=@"org.acnhmobile.layout-regression";NSUserDefaults *defaults=[[NSUserDefaults alloc]initWithSuiteName:suite];[defaults removePersistentDomainForName:suite];
 [defaults setObject:saved forKey:@"acnh"];[defaults setObject:@{} forKey:@"generic"];assert(pad_scale([defaults dictionaryForKey:@"acnh"],@"stick")==1.5);assert(pad_scale([defaults dictionaryForKey:@"generic"],@"stick")==1);[defaults removePersistentDomainForName:suite];
 puts("layout: exact defaults/reset, normalized movement, independent controls, scale bounds, malformed values and persistence passed");
}}
