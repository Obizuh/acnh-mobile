#include "../hle_fs.c"
#include <mach-o/getsect.h>
#include <mach-o/ldsyms.h>
struct dm_flags g_flag;uint64_t g_t0_ns;
uint64_t now_ns(void){return clock_gettime_nsec_np(CLOCK_MONOTONIC);}
void dm_prof_add(int a,uint64_t b,uint64_t c){(void)a;(void)b;(void)c;}
void dm_guest_write_prepare(void *p,size_t n){(void)p;(void)n;}
static int fails;
#define CHECK(s,c) do{int ok=!!(c);printf("%s: %s\n",s,ok?"PASS":"FAIL");fails+=!ok;}while(0)
static void put(const char *p,const char *s){FILE *f=fopen(p,"w");if(!f)abort();fputs(s,f);fclose(f);}
static int same(const char *p,const char *s){char b[64]={0};FILE*f=fopen(p,"r");if(!f)return 0;fread(b,1,63,f);fclose(f);return !strcmp(b,s);}
int main(void){
 Result (*commit)(const char *const *,int)=NULL;
#if __has_feature(address_sanitizer)
 // ASan inserts redzones between Mach-O section globals; the normal build
 // checks the actual import table, while this build checks the implementation.
 commit=h_fs_CommitArray;
#else
 unsigned long bytes=0;const hle_force_ent *ent=(const void *)getsectiondata(&_mh_execute_header,"__DATA","swforce",&bytes);
 for(size_t i=0;i<bytes/sizeof *ent;i++)if(!strcmp(ent[i].name,"_ZN2nn2fs6CommitEPKPKci"))commit=ent[i].fn;
#endif
 CHECK("game array-Commit import has a host implementation",commit!=NULL);if(!commit)return 1;
 char dir[]="/private/tmp/acnh-save-array.XXXXXX";if(!mkdtemp(dir))return 2;setenv("DM_SAVE",dir,1);
 char p[1200],work[1200];snprintf(p,sizeof p,"%s/island",dir);put(p,"old");CHECK("mount device save",h_fs_MountDeviceSave("deviceSave")==0);
 snprintf(work,sizeof work,"%s.work/island",dir);put(work,"new");const char *names[]={"deviceSave","deviceSave"};
 CHECK("empty list succeeds without submitting",commit(NULL,0)==0&&same(p,"old"));
 uint64_t n=dm_fs_commit_count();CHECK("one mount atomically commits actual data",commit(names,1)==0&&same(p,"new")&&dm_fs_commit_count()==n+1);
 put(work,"twice");n=dm_fs_commit_count();CHECK("duplicate mount commits once",commit(names,2)==0&&same(p,"twice")&&dm_fs_commit_count()==n+1);
 put(work,"damaged");dm_fs_save_poison(ENOSPC);CHECK("failed-write transaction preserves prior save",commit(names,1)!=0&&same(p,"twice")&&same(work,"twice"));
 put(work,"recovered");CHECK("next valid transaction can save",commit(names,1)==0&&same(p,"recovered"));
 const char *other[]={"deviceSave","anotherVolume"};put(work,"uncommitted");
 CHECK("distinct volumes fail before changing any save",commit(other,2)!=0&&same(p,"recovered"));
 CHECK("invalid list/count fails",commit(NULL,1)!=0&&commit(names,-1)!=0);
 g_nmnt=0; /* Simulate a process restart, as the existing transaction tests do. */
 CHECK("restart/remount discards uncommitted changes",h_fs_MountDeviceSave("deviceSave")==0&&same(work,"recovered"));
 h_fs_Unmount("deviceSave");rm_tree(dir);char wd[1200];snprintf(wd,sizeof wd,"%s.work",dir);rm_tree(wd);
 printf("failures=%d\n",fails);return !!fails;
}
