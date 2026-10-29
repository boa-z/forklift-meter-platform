#define LOG_TAG "hmi.color"
#define LOG_LVL LOG_LVL_INFO
#include "platform/rtthread/hmi_board_color.h"
#include "platform/common/hmi_display_ccm.h"
#include "platform/common/hmi_display_gamma.h"
#include <mpp_fb.h>
#include <rtthread.h>
#include <stdlib.h>
#include <string.h>
#include <ulog.h>

/* 来自 Kconfig；没有该项的构建保持 SDK 默认表。 */
#ifndef AIC_HMI_DISPLAY_GAMMA
#define AIC_HMI_DISPLAY_GAMMA HMI_GAMMA_PERCENT_NEUTRAL
#endif
/* 开机默认 CCM 表：12 个空格分隔的系数（Display Lab 的 hmi_color ccm 回显可直接粘贴）；
   空串表示不动 SDK 默认表（单位阵）。 */
#ifndef AIC_HMI_DISPLAY_CCM
#define AIC_HMI_DISPLAY_CCM ""
#endif

/* gamma 由 DE 全局控制寄存器使能，不挑图层；SDK 的色彩调整（亮度/对比度/饱和度）按文档只作用于
   视频层，改不到 LVGL 所在的 UI 层，所以实机色彩只能靠这张表（或 CCM）校准。三个通道用同一条曲线。 */
static bool apply_gamma(unsigned percent)
{
    uint32_t words[HMI_GAMMA_WORDS];
    if (!hmi_display_gamma_build(percent, words))
    {
        return false;
    }
    struct mpp_fb *fb = mpp_fb_open();
    if (fb == NULL)
    {
        return false;
    }
    struct aicfb_gamma_config gamma;
    memset(&gamma, 0, sizeof(gamma));
    /* 先取当前配置，保留驱动已有的状态，只覆盖查找表与使能。 */
    (void)mpp_fb_ioctl(fb, AICFB_GET_GAMMA_CONFIG, &gamma);
    for (unsigned channel = 0u; channel < 3u; ++channel)
    {
        memcpy(gamma.gamma_lut[channel], words, sizeof(words));
    }
    gamma.enable = 1u;
    const bool ok = mpp_fb_ioctl(fb, AICFB_UPDATE_GAMMA_CONFIG, &gamma) == 0;
    mpp_fb_close(fb);
    return ok;
}

/* CCM 与 gamma 一样是 DE 全局模块（作用于混合后的整幅画面，含 UI 图层），上电默认单位阵且已使能。 */
static bool apply_ccm(const int32_t table[HMI_CCM_TAPS])
{
    struct mpp_fb *fb = mpp_fb_open();
    if (fb == NULL)
    {
        return false;
    }
    struct aicfb_ccm_config ccm;
    memset(&ccm, 0, sizeof(ccm));
    /* 先取当前配置，保留驱动已有状态，只覆盖系数与使能。 */
    (void)mpp_fb_ioctl(fb, AICFB_GET_CCM_CONFIG, &ccm);
    memcpy(ccm.ccm_table, table, sizeof(ccm.ccm_table));
    ccm.enable = 1u;
    const bool ok = mpp_fb_ioctl(fb, AICFB_UPDATE_CCM_CONFIG, &ccm) == 0;
    mpp_fb_close(fb);
    return ok;
}

/* 解析一个 CCM 系数：0x 前缀、前导 0 或含 a..f 的写法按十六进制（0xFFE5 这种补码就是负值），
   其余按十进制。这样 0100 不会被 strtol 当成八进制，disp_ccm.h 里粘出来的值也能直接用。 */
static bool parse_ccm_tap(const char *token, long *value)
{
    bool hex = ((token[0] == '0') && ((token[1] == 'x') || (token[1] == 'X'))) ||
               ((token[0] == '0') && (token[1] != '\0'));
    for (const char *c = token; (*c != '\0') && !hex; ++c)
    {
        hex = ((*c >= 'a') && (*c <= 'f')) || ((*c >= 'A') && (*c <= 'F'));
    }
    char *end = NULL;
    const long parsed = strtol(token, &end, hex ? 16 : 10);
    if ((end == token) || (*end != '\0'))
    {
        return false;
    }
    *value = parsed;
    return true;
}

/* 解析 Kconfig 里的开机默认表：12 个空格分隔的系数，台架调好的那行可以直接粘进
   CONFIG_AIC_HMI_DISPLAY_CCM。空串或格式不对返回 false。 */
static bool ccm_default_table(int32_t table[HMI_CCM_TAPS])
{
    const char *text = AIC_HMI_DISPLAY_CCM;
    if ((text == NULL) || (text[0] == '\0'))
    {
        return false;
    }
    char buffer[160];
    const size_t length = strlen(text);
    if (length >= sizeof(buffer))
    {
        LOG_E("display ccm default: %u characters exceed the %u byte buffer", (unsigned)length,
              (unsigned)sizeof(buffer));
        return false;
    }
    memcpy(buffer, text, length + 1u);
    char *save = NULL;
    char *token = strtok_r(buffer, " \t", &save);
    for (unsigned index = 0u; index < (unsigned)HMI_CCM_TAPS; ++index)
    {
        long value = 0;
        if ((token == NULL) || !parse_ccm_tap(token, &value))
        {
            LOG_E("display ccm default: value %u is missing or invalid", index);
            return false;
        }
        table[index] = hmi_display_ccm_normalize(index, value);
        token = strtok_r(NULL, " \t", &save);
    }
    if (token != NULL)
    {
        LOG_E("display ccm default: expected %u values, extra values present", (unsigned)HMI_CCM_TAPS);
        return false;
    }
    return true;
}

void hmi_board_color_init(void)
{
    if ((unsigned)AIC_HMI_DISPLAY_GAMMA != (unsigned)HMI_GAMMA_PERCENT_NEUTRAL)
    {
        if (apply_gamma((unsigned)AIC_HMI_DISPLAY_GAMMA))
        {
            LOG_I("display gamma %u%% applied", (unsigned)AIC_HMI_DISPLAY_GAMMA);
        }
        else
        {
            LOG_E("display gamma %u%% could not be applied", (unsigned)AIC_HMI_DISPLAY_GAMMA);
        }
    }
    /* 整机固件在这里固化调好的 CCM 表。 */
    int32_t table[HMI_CCM_TAPS];
    if (!ccm_default_table(table))
    {
        return;
    }
    if (apply_ccm(table))
    {
        LOG_I("display ccm default applied");
    }
    else
    {
        LOG_E("display ccm default could not be applied");
    }
}
