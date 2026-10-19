// 覆盖直接项、连续位图、边界位和损坏输入；不依赖游戏文件。
// Cover direct entries, consecutive bitmaps, edge bits, and malformed input without game files.
#include "../dm_relr.h"
#include <assert.h>
#include <stdio.h>
int main(void) {
    uint64_t image[256] = {0}; uint64_t base = (uintptr_t)image, n = 0;
    image[0] = 64; image[1] = 1 | (UINT64_C(1)<<1) | (UINT64_C(1)<<63); image[2] = 3;
    image[8] = 9; image[9] = 17; image[71] = 23; image[72] = 31;
    assert(!dm_relr_apply((uint8_t *)image, sizeof image, 0, 24, &n) && n == 4);
    assert(image[8] == base+9 && image[9] == base+17 && image[71] == base+23 && image[72] == base+31);
    assert(image[10] == 0);
    image[0] = 3; assert(dm_relr_apply((uint8_t *)image,sizeof image,0,8,&n));
    image[0] = sizeof image; assert(dm_relr_apply((uint8_t *)image,sizeof image,0,8,&n));
    image[0] = 66; assert(dm_relr_apply((uint8_t *)image,sizeof image,0,8,&n));
    assert(dm_relr_apply((uint8_t *)image,sizeof image,0,7,&n));
    assert(dm_relr_apply((uint8_t *)image,sizeof image,sizeof image,8,&n));
    image[0] = sizeof image - 8; image[1] = 3;
    assert(dm_relr_apply((uint8_t *)image,sizeof image,0,16,&n));
    puts("RELR tests passed");
}
