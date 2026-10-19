// 仅本机诊断：从显式指定文件取带序号的短按命令；不进入 iOS 包。
// Local diagnostics only: read sequenced short-press commands from an explicit file; excluded from iOS.
#include "dm.h"
#include "front.h"
#include <fcntl.h>
#include <unistd.h>
void dm_gfx_shot_at(long k,const char *path);
uint64_t dm_acnh_live_buttons(void){
 static int fd=-2;static uint64_t sequence,until,mask;static int touching;
 if(fd==-2){const char *p=getenv("ACNH_LIVE_KEYS");fd=p?open(p,O_RDONLY):-1;}
 if(fd<0)return 0;
 uint64_t command[5];uint64_t now=now_ns();
 if(pread(fd,command,sizeof command,0)==sizeof command && command[0]!=sequence){sequence=command[0];if(command[1]&(1ull<<62)){const char *path=getenv("ACNH_LIVE_SHOT");if(path)dm_gfx_shot_at(1,path);}mask=command[1]&0xffffffffu;until=now+command[2];if(touching){dm_touch_set(NULL,0);touching=0;}if(command[1]>>63){dm_touch t={.id=0,.x=(float)command[3]/1000000,.y=(float)command[4]/1000000};dm_touch_set(&t,1);touching=1;}LOG("ACNH live command %llu mask=%#llx",(unsigned long long)sequence,(unsigned long long)mask);}
 if(touching && now>=until){dm_touch_set(NULL,0);touching=0;}
 return now<until?mask:0;
}
