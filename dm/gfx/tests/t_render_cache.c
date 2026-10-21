#include "../nvn/nvn_render_cache.h"
#include <stdio.h>
int main(void){nvn_render_cache c={0};double viewport[]={0,0,1280,720,0,1};
if(!nvn_render_cache_changed(&c,0,viewport,sizeof viewport))return 1;
for(int i=0;i<10000;i++)if(nvn_render_cache_changed(&c,0,viewport,sizeof viewport))return 2;
viewport[2]=1920;if(!nvn_render_cache_changed(&c,0,viewport,sizeof viewport))return 3;
for(unsigned i=1;i<9;i++)if(!nvn_render_cache_changed(&c,i,viewport,sizeof viewport))return 4;
nvn_render_cache_reset(&c);for(unsigned i=0;i<9;i++)if(!nvn_render_cache_changed(&c,i,viewport,sizeof viewport))return 5;
puts("PASS: identical state elided; changed states, independent slots and new encoders always rebound");return 0;}
