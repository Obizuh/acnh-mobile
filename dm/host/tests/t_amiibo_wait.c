#include <assert.h>
#include <stdio.h>
#include "../../ios/Sources/AmiiboImportWait.h"
static unsigned mask;
static int calls,gfxcalls;static unsigned gfxmask;
void dm_gfx_suspend_for(int reason,int on){assert(reason==3);gfxcalls++;if(on)gfxmask|=1u<<reason;else gfxmask&=~(1u<<reason);}
void guest_clock_freeze_for(int reason,int on){assert(reason==3);calls++;if(on)mask|=1u<<reason;else mask&=~(1u<<reason);}
int main(void){
 AmiiboImportWait wait={0};gfxmask=1;mask=(1u<<1)|(1u<<2); // background + keyboard independent
 amiibo_import_wait_end(&wait);assert(calls==0);
 for(int i=0;i<64;i++){
  int before=calls;amiibo_import_wait_begin(&wait);assert(wait.held && (mask&(1u<<3)) && calls==before+1);
  // Repeated file selection/retry does not nest or prematurely clear the hold.
  for(int j=0;j<100;j++)amiibo_import_wait_begin(&wait);
  assert(calls==before+1 && gfxcalls==calls && gfxmask==(1u|(1u<<3)) && wait.held);
  amiibo_import_wait_end(&wait);assert(!wait.held && mask==((1u<<1)|(1u<<2)) && calls==before+2);
  amiibo_import_wait_end(&wait);assert(calls==before+2 && gfxcalls==calls && gfxmask==1);
 }
 puts("amiibo selection pause: 64 retry/cleanup cycles passed; background and keyboard preserved");
}
