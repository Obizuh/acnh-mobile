#include "../hle_time.c"
#include <mach-o/getsect.h>
#include <mach-o/ldsyms.h>
struct dm_flags g_flag;uint64_t g_t0_ns;
module_t g_mod[8];int g_nmod;
void *module_sym(module_t *m,const char *s){(void)m;(void)s;return NULL;}
static int fails;
#define CHECK(s,c) do{int ok=!!(c);printf("%s: %s\n",s,ok?"PASS":"FAIL");fails+=!ok;}while(0)
static HostZoneRule load(const char *s){char name[0x24]={0};strlcpy(name,s,sizeof name);HostZoneRule r={0};CHECK("load named zone",h_time_LoadRule(&r,name)==RES_OK);return r;}
int main(void){
CalendarTime c;CalendarAdditionalInfo i;int n;int64_t out[2];
{HostZoneRule r=load("Asia/Shanghai");int64_t t=1792367400;CHECK("Asia/Shanghai calendar",!h_time_ToCalendarRule(&c,&i,&t,&r)&&c.year==2026&&c.month==10&&c.day==19&&c.hour==7&&c.minute==50);CHECK("offset and weekday",i.offset==28800&&i.wday==1&&i.yday==291);CHECK("calendar roundtrip",!h_time_ToPosixRule(&n,out,2,&c,&r)&&n==1&&out[0]==t);}
{HostZoneRule r=load("UTC");int64_t t=1792368600;CHECK("UTC calendar",!h_time_ToCalendarRule(&c,&i,&t,&r)&&c.year==2026&&c.month==10&&c.day==19&&c.hour==0&&c.minute==10);CHECK("offset and weekday",i.offset==0&&i.wday==1&&i.yday==291);CHECK("calendar roundtrip",!h_time_ToPosixRule(&n,out,2,&c,&r)&&n==1&&out[0]==t);}
{HostZoneRule r=load("Asia/Tokyo");int64_t t=1767297600;CHECK("Asia/Tokyo calendar",!h_time_ToCalendarRule(&c,&i,&t,&r)&&c.year==2026&&c.month==1&&c.day==2&&c.hour==5&&c.minute==0);CHECK("offset and weekday",i.offset==32400&&i.wday==5&&i.yday==1);CHECK("calendar roundtrip",!h_time_ToPosixRule(&n,out,2,&c,&r)&&n==1&&out[0]==t);}
{HostZoneRule r=load("Asia/Kathmandu");int64_t t=1780281000;CHECK("Asia/Kathmandu calendar",!h_time_ToCalendarRule(&c,&i,&t,&r)&&c.year==2026&&c.month==6&&c.day==1&&c.hour==8&&c.minute==15);CHECK("offset and weekday",i.offset==20700&&i.wday==1&&i.yday==151);CHECK("calendar roundtrip",!h_time_ToPosixRule(&n,out,2,&c,&r)&&n==1&&out[0]==t);}
{HostZoneRule r=load("America/New_York");int64_t t=1767268800;CHECK("America/New_York calendar",!h_time_ToCalendarRule(&c,&i,&t,&r)&&c.year==2026&&c.month==1&&c.day==1&&c.hour==7&&c.minute==0);CHECK("offset and weekday",i.offset==-18000&&i.wday==4&&i.yday==0);CHECK("calendar roundtrip",!h_time_ToPosixRule(&n,out,2,&c,&r)&&n==1&&out[0]==t);}
{HostZoneRule r=load("America/New_York");int64_t t=1782907200;CHECK("America/New_York calendar",!h_time_ToCalendarRule(&c,&i,&t,&r)&&c.year==2026&&c.month==7&&c.day==1&&c.hour==8&&c.minute==0);CHECK("offset and weekday",i.offset==-14400&&i.wday==3&&i.yday==181);CHECK("calendar roundtrip",!h_time_ToPosixRule(&n,out,2,&c,&r)&&n==1&&out[0]==t);}
{HostZoneRule r=load("Australia/Sydney");int64_t t=1767268800;CHECK("Australia/Sydney calendar",!h_time_ToCalendarRule(&c,&i,&t,&r)&&c.year==2026&&c.month==1&&c.day==1&&c.hour==23&&c.minute==0);CHECK("offset and weekday",i.offset==39600&&i.wday==4&&i.yday==0);CHECK("calendar roundtrip",!h_time_ToPosixRule(&n,out,2,&c,&r)&&n==1&&out[0]==t);}
{HostZoneRule r=load("Australia/Sydney");int64_t t=1782907200;CHECK("Australia/Sydney calendar",!h_time_ToCalendarRule(&c,&i,&t,&r)&&c.year==2026&&c.month==7&&c.day==1&&c.hour==22&&c.minute==0);CHECK("offset and weekday",i.offset==36000&&i.wday==3&&i.yday==181);CHECK("calendar roundtrip",!h_time_ToPosixRule(&n,out,2,&c,&r)&&n==1&&out[0]==t);}
HostZoneRule r=load("America/New_York");c=(CalendarTime){2026,11,1,1,30,0,0};
CHECK("fall overlap yields both ordered times",!h_time_ToPosixRule(&n,out,2,&c,&r)&&n==2&&out[0]==1793511000LL&&out[1]==1793514600LL);
c=(CalendarTime){2026,3,8,2,30,0,0};CHECK("spring gap has no matching time",!h_time_ToPosixRule(&n,out,2,&c,&r)&&n==0);
c=(CalendarTime){2026,2,30,12,0,0,0};CHECK("invalid date rejected",h_time_ToPosixRule(&n,out,2,&c,&r)!=0);
c=(CalendarTime){2026,11,1,1,30,0,0};CHECK("capacity one is respected",!h_time_ToPosixRule(&n,out,1,&c,&r)&&n==1&&out[0]==1793511000LL);
CHECK("capacity zero writes no timestamp",!h_time_ToPosixRule(&n,NULL,0,&c,&r)&&n==0);
r.magic=0;int64_t t=0;CHECK("foreign rule rejected",h_time_ToCalendarRule(&c,&i,&t,&r)!=0);
char bad[36]="No/Such_Zone";CHECK("unknown timezone rejected",h_time_LoadRule(&r,bad)!=0);
char loc[36];setenv("TZ","Asia/Shanghai",1);CHECK("full zone identifier",!h_time_LocName(loc)&&!strcmp(loc,"Asia/Shanghai"));HostClockSnapshot snap;CHECK("snapshot and rule use same zone",!h_time_ClockSnapshot(&snap)&&!strcmp(snap.location,loc));
printf("failures=%d\n",fails);return !!fails;}
