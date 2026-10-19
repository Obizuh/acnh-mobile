#include "../hle_time.c"
#include <mach-o/getsect.h>
#include <mach-o/ldsyms.h>
struct dm_flags g_flag;uint64_t g_t0_ns;
module_t g_mod[8];int g_nmod;
void *module_sym(module_t *m,const char *s){(void)m;(void)s;return NULL;}
static int fails;
#define CHECK(s,c) do{int ok=!!(c);printf("%s: %s\n",s,ok?"PASS":"FAIL");fails+=!ok;}while(0)
int main(void){
 int64_t (*diff)(const HostClockSnapshot*,const HostClockSnapshot*)=NULL;
 Result (*span)(int64_t*,const HostClockSnapshot*,const HostClockSnapshot*)=NULL;
#if __has_feature(address_sanitizer)
 diff=h_time_UserClockDifference;span=h_time_SnapshotSpan;
#else
 unsigned long bytes=0;const hle_force_ent *ent=(const void*)getsectiondata(&_mh_execute_header,"__DATA","swforce",&bytes);
 for(size_t i=0;i<bytes/sizeof *ent;i++) {
 if(!strcmp(ent[i].name,"_ZN2nn4time48CalculateStandardUserSystemClockDifferenceByUserERKNS0_13ClockSnapshotES3_"))diff=ent[i].fn;
 if(!strcmp(ent[i].name,"_ZN2nn4time20CalculateSpanBetweenEPNS_8TimeSpanERKNS0_13ClockSnapshotES5_"))span=ent[i].fn;
 }
#endif
 CHECK("manual save clock-difference import is bound",diff);CHECK("snapshot-span import is bound",span);if(!diff||!span)return 1;
 HostClockSnapshot a={0},b={0};int64_t ns=777;
 CHECK("fresh host snapshot",!h_time_ClockSnapshot(&a)&&sizeof a==0xd0);
 b=a;b.steady.seconds+=600;b.user_time+=600;b.network_time+=600;
 CHECK("normal elapsed play is not user clock adjustment",diff(&a,&b)==0);
 CHECK("elapsed play uses steady clock",!span(&ns,&a,&b)&&ns==600000000000LL);
 b.user_context.offset+=3600;CHECK("forward user clock adjustment",diff(&a,&b)==3600000000000LL);
 b.user_context.offset=a.user_context.offset-60;CHECK("backward user clock adjustment",diff(&a,&b)==-60000000000LL);
 a.automatic=b.automatic=1;CHECK("automatic corrections are not manual adjustments",diff(&a,&b)==0);a.automatic=0;
 CHECK("mixed automatic/manual preserves adjustment",diff(&a,&b)==-60000000000LL);
 b.user_context.point.source[0]^=1;CHECK("different user clock source ignores offset difference",diff(&a,&b)==0);
 b.steady.source[0]^=1;CHECK("different steady source falls back to network time",!span(&ns,&a,&b)&&ns==600000000000LL);
 b.network_time=0;ns=777;CHECK("missing cross-source network time fails without writing output",span(&ns,&a,&b)!=0&&ns==777);
 a=(HostClockSnapshot){0};b=a;b.steady.seconds=INT64_MAX;CHECK("span overflow fails without undefined arithmetic",span(&ns,&a,&b)!=0&&ns==777);
 b.user_context.offset=INT64_MAX;a.user_context.offset=INT64_MIN;CHECK("positive extreme clock difference is bounded",diff(&a,&b)==INT64_MAX);
 CHECK("negative extreme clock difference is bounded",diff(&b,&a)==INT64_MIN);
 printf("failures=%d\n",fails);return !!fails;
}
