#include "../hle_movie.c"
#include <time.h>
module_t g_mod[MAX_MOD];int g_nmod;
void *guest_sym(const char *n){(void)n;return NULL;}
uint64_t now_ns(void){struct timespec t;clock_gettime(CLOCK_MONOTONIC,&t);return (uint64_t)t.tv_sec*1000000000+t.tv_nsec;}
static void *late_send(void *p){void **self=p;h_movie_SendIn(self,0,0,1,0,0);return NULL;}
int main(void){
 mdec_t d={.type=3,.w=16,.h=16,.started=1,.ninfree=MD_NIN};void *self[2]={0,&d};
 pthread_mutex_init(&d.mu,0);pthread_mutex_init(&d.dmu,0);pthread_mutex_init(&d.wmu,0);pthread_cond_init(&d.wcv,0);
 for(int i=0;i<MD_NIN;i++){d.in[i]=calloc(1,MD_INCAP);d.infree[i]=i;}
 pthread_create(&d.wk,0,md_worker,&d);
 int fail=0;
 for(int run=0;run<1000;run++){
  h_movie_Start(self);int idx,left;h_movie_AcqIn(self,&idx,&left);
  h_movie_Stop(self);int r=h_movie_AcqIn(self,&idx,&left);if(!r||idx!=-1)fail++;
  if(h_movie_SendIn(self,0,0,1,0,0)==0)fail++;
  h_movie_Flush(self);pthread_t t;pthread_create(&t,0,late_send,self);pthread_join(t,0);
  if(d.vpx_on||d.dec_n||d.npend)fail++;
 }
 h_movie_Start(self);int idx,left;if(h_movie_AcqIn(self,&idx,&left)||h_movie_SendIn(self,idx,0,0,0,4))fail++;
 int64_t pts;uint32_t flags;if(h_movie_AcqOut(self,&idx,&pts,&flags,&left)||flags!=4)fail++;
 h_movie_RelOut(self,idx);h_movie_Stop(self);md_job(&d,MDJ_QUIT);pthread_join(d.wk,0);
 for(int i=0;i<MD_NIN;i++)free(d.in[i]);
 printf("movie stop/late-send/flush/restart/EOS checks: failures=%d\n",fail);return !!fail;
}
