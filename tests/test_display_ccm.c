#include "platform/common/hmi_display_ccm.h"
#include <assert.h>
#include <stdio.h>

int main(void)
{
    /* 归一：十六进制补码与负十进制落到同一个字段值；增益按 12 位、偏移按 9 位。 */
    assert(hmi_display_ccm_normalize(1u, 0xFFE5L) == -27);
    assert(hmi_display_ccm_normalize(1u, -27L) == -27);
    assert(hmi_display_ccm_normalize(4u, 0x0FE5L) == -27);
    assert(hmi_display_ccm_normalize(3u, 0xFFE5L) == -27);
    assert(hmi_display_ccm_normalize(3u, 511L) == -1);
    assert(hmi_display_ccm_normalize(0u, 0x100L) == 256);

    printf("display ccm ok\n");
    return 0;
}
