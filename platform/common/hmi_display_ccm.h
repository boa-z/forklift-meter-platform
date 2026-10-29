#ifndef HMI_DISPLAY_CCM_H
#define HMI_DISPLAY_CCM_H
#include <stdint.h>

/* 显示引擎（DE）的色彩矩阵（CCM）：3x3 增益 + 3x1 偏移，表序
   [C00 C01 C02 OFFSET0 C10 C11 C12 OFFSET1 C20 C21 C22 OFFSET2]。
   增益是 12 位有符号数、8 位小数（0x100 = 1.0），偏移是 9 位有符号整数，单位是输出码值。
   硬件按“(C0*R + C1*G + C2*B) / 256 + OFFSET”求值并截断到 0..255，作用在混合后的整幅画面上
   （含 UI 图层）。D13x 上电时就是单位阵且已使能，写单位阵等于什么都不做。 */
enum
{
    HMI_CCM_TAPS = 12
};

/** @brief 把外部写法归一进字段：增益按 12 位、偏移按 9 位取补码，返回带符号值。
 *  十六进制补码（0xFFE5）与负十进制（-27）会落到同一个字段值。 */
int32_t hmi_display_ccm_normalize(unsigned index, long value);
#endif
