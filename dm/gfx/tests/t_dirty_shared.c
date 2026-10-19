#include "../../host/dm.h"
#include "../nvn/nvn_dirty.h"
#include <stdatomic.h>
#include <sys/mman.h>
#include <unistd.h>
module_t g_mod[MAX_MOD];int g_nmod;
module_t *module_of(const void *p){(void)p;return NULL;}
void crash_install(void);
static volatile unsigned char *mem;
static _Atomic int done;
static void *writer(void *p){(void)p;unsigned char x=0;while(!atomic_load(&done))*mem=++x;return NULL;}
int main(void){
 crash_install();size_t pg=getpagesize();mem=mmap(0,pg*3,PROT_READ|PROT_WRITE,MAP_ANON|MAP_PRIVATE,-1,0);
 void *a=(void *)mem,*tmp=(void *)(mem+2*pg),*adj=(void *)(mem+4096);
 nvn_dirty_add_range(tmp,pg);nvn_dirty_add_range(a,pg);
 // Recycle the earlier registry slot into a pool sharing an already protected
 // host page. The old implementation forgot that page's write protection.
 nvn_dirty_protect(a,pg);nvn_dirty_remove_range(tmp);nvn_dirty_add_range(adj,4096);
 uint64_t stamp=nvn_dirty_now();mem[4096]=42;
 if(!nvn_dirty_since(a,pg,stamp)||!nvn_dirty_since(adj,4096,stamp))return 2;
 nvn_dirty_remove_range(adj);
 if(!nvn_dirty_tracked(a,pg))return 3;
 nvn_dirty_add_range(tmp,pg);
 pthread_t t;pthread_create(&t,0,writer,0);
 for(int i=0;i<5000;i++){
  nvn_dirty_protect(a,pg);nvn_dirty_remove_range(tmp);nvn_dirty_add_range(adj,4096);
  nvn_dirty_protect(adj,4096);nvn_dirty_remove_range(adj);nvn_dirty_add_range(tmp,pg);
 }
 atomic_store(&done,1);pthread_join(t,0);nvn_dirty_remove_range(a);nvn_dirty_remove_range(tmp);
 int live,peak,cap;nvn_dirty_stat(&live,&peak,&cap);printf("5000 concurrent pool swaps and protected writes: live=%d peak=%d\n",live,peak);return live!=0;
}
