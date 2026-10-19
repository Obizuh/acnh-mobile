// ACNH direct-read .data filesystem. Immutable index, read-only part descriptors,
// and independent bounded file slices; no unpacked tree or per-file disk cache.
#include "acnh_data.h"
#include <sys/mman.h>
#include <CommonCrypto/CommonDigest.h>
#include <limits.h>
typedef struct {uint64_t name,size;} acnh_part;
typedef struct {uint64_t path,offset,size;uint32_t part,reserved;uint8_t sha256[32];} acnh_file;
typedef struct {char root[1024];void *map;size_t bytes,nfiles,nparts;const acnh_part *parts;const acnh_file *files;const char *strings;size_t nstrings;int fds[100];} acnh_pack;
_Static_assert(sizeof(acnh_part)==16 && sizeof(acnh_file)==64,"ACNH direct index ABI");
static _Atomic(acnh_pack *) g_acnh_pack;
static const acnh_file *g_acnh_slices[4096];
static pthread_mutex_t g_acnh_pack_lock=PTHREAD_MUTEX_INITIALIZER;
static int acnh_fail(char *why,size_t cap,const char *text){if(why&&cap)snprintf(why,cap,"%s",text);return 0;}
static int acnh_path(const char *src,char out[1024]){
 size_t n=0;while(*src){while(*src=='/')src++;if(!*src)break;const char *e=strchr(src,'/');if(!e)e=src+strlen(src);size_t k=e-src;
 if(k==1&&*src=='.'){src=e;continue;}if(k>=0x301||(k==2&&!memcmp(src,"..",2))||memchr(src,'\\',k)||n+k+2>1024){errno=ENOENT;return 0;}
 if(n)out[n++]='/';memcpy(out+n,src,k);n+=k;src=e;}out[n]=0;return 1;
}
static const char *acnh_string(acnh_pack *p,uint64_t off){if(off>=p->nstrings)return NULL;const char *s=p->strings+off;return memchr(s,0,p->nstrings-off)?s:NULL;}
static size_t acnh_lower(acnh_pack *p,const char *path){size_t a=0,b=p->nfiles;while(a<b){size_t m=a+(b-a)/2;if(strcmp(p->strings+p->files[m].path,path)<0)a=m+1;else b=m;}return a;}
const char *dm_acnh_data_root(void){acnh_pack*p=atomic_load(&g_acnh_pack);return p?p->root:NULL;}
int dm_acnh_data_mount(const char *root,char *why,size_t cap){
 pthread_mutex_lock(&g_acnh_pack_lock);acnh_pack *old=atomic_load(&g_acnh_pack);if(old){int ok=!strcmp(old->root,root);pthread_mutex_unlock(&g_acnh_pack_lock);return ok?1:acnh_fail(why,cap,"已有另一个数据包正在使用");}
 acnh_pack*p=calloc(1,sizeof *p);int index=-1;const char *error="数据包索引无效";if(!p)goto fail;for(int i=0;i<100;i++)p->fds[i]=-1;
 char name[1200];if(strlen(root)>=sizeof p->root)goto fail;strlcpy(p->root,root,sizeof p->root);snprintf(name,sizeof name,"%s/ACNH-index.data",root);
 index=open(name,O_RDONLY|O_NOFOLLOW);struct stat st;if(index<0||fstat(index,&st)||!S_ISREG(st.st_mode)||st.st_size<96||st.st_size>128*1024*1024){error="缺少或无法读取 ACNH-index.data";goto fail;}
 p->bytes=(size_t)st.st_size;p->map=mmap(NULL,p->bytes,PROT_READ,MAP_PRIVATE,index,0);close(index);index=-1;if(p->map==MAP_FAILED){p->map=NULL;goto fail;}
 const uint8_t*h=p->map;uint32_t version,np;uint64_t nf,po,fo,so,ss,total;
 memcpy(&version,h+8,4);memcpy(&np,h+12,4);memcpy(&nf,h+16,8);memcpy(&po,h+24,8);memcpy(&fo,h+32,8);memcpy(&so,h+40,8);memcpy(&ss,h+48,8);memcpy(&total,h+56,8);
 if(memcmp(h,"ACNHDAT1",8)||version!=1||!np||np>100||!nf||nf>500000||total!=p->bytes||po!=96||fo!=po+(uint64_t)np*16||so!=fo+nf*64||so>total||ss!=total-so)goto fail;
 uint8_t digest[32];CC_SHA256(h+96,(CC_LONG)(p->bytes-96),digest);if(memcmp(digest,h+64,32)){error="数据包索引校验失败";goto fail;}
 p->nparts=np;p->nfiles=nf;p->parts=(const void*)(h+po);p->files=(const void*)(h+fo);p->strings=(const void*)(h+so);p->nstrings=ss;
 for(size_t i=0;i<np;i++){
  const char*s=acnh_string(p,p->parts[i].name);if(!s||!*s||strchr(s,'/')||strchr(s,'\\')||strlen(s)<6||strcmp(s+strlen(s)-5,".data")||p->parts[i].size>INT64_MAX)goto fail;
  snprintf(name,sizeof name,"%s/%s",root,s);int fd=open(name,O_RDONLY|O_NOFOLLOW);if(fd<0||fstat(fd,&st)||!S_ISREG(st.st_mode)||(uint64_t)st.st_size!=p->parts[i].size){if(fd>=0)close(fd);error="数据分卷缺失或尚未复制完整";goto fail;}p->fds[i]=fd;
 }
 int marker=0;
 for(size_t i=0;i<nf;i++){
  const acnh_file*f=&p->files[i];const char*s=acnh_string(p,f->path);char clean[1024];if(!s||!*s||!acnh_path(s,clean)||strcmp(s,clean)||f->reserved||f->part>=np||f->offset>p->parts[f->part].size||f->size>p->parts[f->part].size-f->offset)goto fail;
  if(i&&strcmp(p->strings+p->files[i-1].path,s)>=0)goto fail;
  if(!strcmp(s,"Shader/Park_UBER_All.Nin_NX_NVN.szs"))marker=1;
 }
 if(!marker){error="这不是动森资源包";goto fail;}
 atomic_store(&g_acnh_pack,p);pthread_mutex_unlock(&g_acnh_pack_lock);return 1;
 fail:if(index>=0)close(index);if(p){for(int i=0;i<100;i++)if(p->fds[i]>=0)close(p->fds[i]);if(p->map)munmap(p->map,p->bytes);free(p);}pthread_mutex_unlock(&g_acnh_pack_lock);return acnh_fail(why,cap,error);
}
int dm_acnh_data_stat(const char *path,int *directory,int64_t *size){
 acnh_pack*p=atomic_load(&g_acnh_pack);char q[1024];if(!p||!acnh_path(path,q))return 0;
 if(!q[0]){*directory=1;*size=0;return 1;}size_t i=acnh_lower(p,q);if(i<p->nfiles&&!strcmp(q,p->strings+p->files[i].path)){*directory=0;*size=p->files[i].size;return 1;}
 size_t n=strlen(q);if(n+1>=sizeof q)return 0;q[n++]='/';q[n]=0;i=acnh_lower(p,q);
 if(i<p->nfiles&&!strncmp(q,p->strings+p->files[i].path,n)){*directory=1;*size=0;return 1;}errno=ENOENT;return 0;
}
int dm_acnh_data_open(const char *path){
 acnh_pack*p=atomic_load(&g_acnh_pack);char q[1024];if(!p||!acnh_path(path,q))return -1;size_t i=acnh_lower(p,q);if(i==p->nfiles||strcmp(q,p->strings+p->files[i].path)){errno=ENOENT;return -1;}
 int fd=fcntl(p->fds[p->files[i].part],F_DUPFD_CLOEXEC,3);if(fd<0)return -1;if(fd>=4096){close(fd);errno=EMFILE;return -1;}
 pthread_mutex_lock(&g_acnh_pack_lock);g_acnh_slices[fd]=&p->files[i];pthread_mutex_unlock(&g_acnh_pack_lock);return fd;
}
static const acnh_file *acnh_fd(int fd){if(fd<0||fd>=4096)return NULL;pthread_mutex_lock(&g_acnh_pack_lock);const acnh_file*f=g_acnh_slices[fd];pthread_mutex_unlock(&g_acnh_pack_lock);return f;}
int dm_acnh_data_fd_size(int fd,int64_t *size){const acnh_file*f=acnh_fd(fd);if(!f)return 0;*size=f->size;return 1;}
ssize_t dm_acnh_data_pread(int fd,void *buf,size_t n,off_t offset,int *handled){const acnh_file*f=acnh_fd(fd);*handled=f!=NULL;if(!f)return 0;if(offset<0){errno=EINVAL;return -1;}if((uint64_t)offset>=f->size)return 0;if(n>f->size-(uint64_t)offset)n=(size_t)(f->size-(uint64_t)offset);return pread(fd,buf,n,(off_t)(f->offset+(uint64_t)offset));}
void dm_acnh_data_close(int fd){if(fd<0||fd>=4096)return;pthread_mutex_lock(&g_acnh_pack_lock);g_acnh_slices[fd]=NULL;pthread_mutex_unlock(&g_acnh_pack_lock);}
int dm_acnh_data_list(const char *path,dm_acnh_data_emit emit,void *opaque){
 acnh_pack*p=atomic_load(&g_acnh_pack);char prefix[1024],last[0x301]={0};int dir;int64_t size;
 if(!p||!dm_acnh_data_stat(path,&dir,&size)||!dir||!acnh_path(path,prefix))return 0;
 size_t n=strlen(prefix);if(n){if(n+1>=sizeof prefix)return 0;prefix[n++]='/';prefix[n]=0;}
 for(size_t i=acnh_lower(p,prefix);i<p->nfiles;i++){
  const char*s=p->strings+p->files[i].path;if(strncmp(s,prefix,n))break;s+=n;const char*slash=strchr(s,'/');size_t k=slash?(size_t)(slash-s):strlen(s);char name[0x301];memcpy(name,s,k);name[k]=0;if(!strcmp(name,last))continue;strlcpy(last,name,sizeof last);
  if(!emit(opaque,name,slash!=NULL,slash?0:(int64_t)p->files[i].size))return 0;
 }return 1;
}
