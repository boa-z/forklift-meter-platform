#define LOG_TAG "hmi.color"
#define LOG_LVL LOG_LVL_INFO
#include "platform/rtthread/hmi_board_color.h"
#include "platform/common/hmi_display_ccm.h"
#include "platform/common/hmi_display_gamma.h"
#include <finsh.h>
#include <mpp_fb.h>
#include <rtthread.h>
#include <stdlib.h>
#include <string.h>
#include <ulog.h>

/* 来自 Kconfig；没有该项的构建保持 SDK 默认表。 */
#ifndef AIC_HMI_DISPLAY_GAMMA
#define AIC_HMI_DISPLAY_GAMMA HMI_GAMMA_PERCENT_NEUTRAL
#endif
/* 开机默认 CCM 表：12 个空格分隔的系数，写法与 hmi_color ccm 的回显一致；
   空串表示不动 SDK 默认表（单位阵）。 */
#ifndef AIC_HMI_DISPLAY_CCM
#define AIC_HMI_DISPLAY_CCM ""
#endif

static unsigned active_percent = HMI_GAMMA_PERCENT_NEUTRAL;

/* gamma 由 DE 全局控制寄存器使能，不挑图层；SDK 的色彩调整（亮度/对比度/饱和度）按文档只作用于
   视频层，改不到 LVGL 所在的 UI 层，所以实机色彩只能靠这张表（或 CCM）校准。三个通道用同一条曲线。 */
bool hmi_board_color_apply(unsigned percent)
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
    if (ok)
    {
        active_percent = percent;
    }
    return ok;
}

unsigned hmi_board_color_percent(void)
{
    return active_percent;
}

/* CCM 与 gamma 一样是 DE 全局模块（作用于混合后的整幅画面，含 UI 图层），上电默认单位阵且已使能。
   注意 CCM 没有影子寄存器，写入当场生效（台架上会看到瞬时跳变，可接受）。 */
bool hmi_board_color_ccm_apply(const int32_t table[HMI_CCM_TAPS])
{
    if (table == NULL)
    {
        return false;
    }
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

bool hmi_board_color_ccm_read(int32_t table[HMI_CCM_TAPS], bool *enabled)
{
    if (table == NULL)
    {
        return false;
    }
    struct mpp_fb *fb = mpp_fb_open();
    if (fb == NULL)
    {
        return false;
    }
    struct aicfb_ccm_config ccm;
    memset(&ccm, 0, sizeof(ccm));
    const bool ok = mpp_fb_ioctl(fb, AICFB_GET_CCM_CONFIG, &ccm) == 0;
    mpp_fb_close(fb);
    if (!ok)
    {
        return false;
    }
    memcpy(table, ccm.ccm_table, sizeof(ccm.ccm_table));
    if (enabled != NULL)
    {
        *enabled = ccm.enable != 0u;
    }
    return true;
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

/* 解析 Kconfig 里的开机默认表：12 个空格分隔的系数，写法与 hmi_color ccm 的回显一致，
   台架调好的那行可以直接粘进 CONFIG_AIC_HMI_DISPLAY_CCM。空串或格式不对返回 false。 */
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
        if (hmi_board_color_apply((unsigned)AIC_HMI_DISPLAY_GAMMA))
        {
            LOG_I("display gamma %u%% applied", (unsigned)AIC_HMI_DISPLAY_GAMMA);
        }
        else
        {
            LOG_E("display gamma %u%% could not be applied", (unsigned)AIC_HMI_DISPLAY_GAMMA);
        }
    }
    /* 整机固件在这里固化调好的 CCM 表；台架仍可用 hmi_color ccm 改，reset 回到这张表。 */
    int32_t table[HMI_CCM_TAPS];
    if (!ccm_default_table(table))
    {
        return;
    }
    if (hmi_board_color_ccm_apply(table))
    {
        LOG_I("display ccm default applied");
    }
    else
    {
        LOG_E("display ccm default could not be applied");
    }
}

/* 台架调色：hmi_color 查看；hmi_color 120 调 gamma；hmi_color sat 115 或
   hmi_color ccm <r|g|b> <4 个系数> 逐行调 CCM。全部即时生效、断电不保留；开机默认表来自
   CONFIG_AIC_HMI_DISPLAY_CCM，reset 复原到它、identity 清成单位阵。
   之所以逐行设置而不是整表 12 个系数一条命令：板端 finsh 单行限 80 字符、参数限 10 个
   （FINSH_CMD_SIZE / FINSH_ARG_MAX），整表一条命令既超长也超参数数，会被截断误执行。 */
static void hmi_color_usage(void)
{
    rt_kprintf("usage: hmi_color                show gamma, ccm and the build default table\n");
    rt_kprintf("       hmi_color 50..300        set gamma percent (100 = DE default)\n");
    rt_kprintf("       hmi_color sat 50..200    build ccm from saturation (100 = identity)\n");
    rt_kprintf("       hmi_color ccm            show the ccm table\n");
    rt_kprintf("       hmi_color ccm r|g|b C0 C1 C2 OFF  set one row (Q8 gain / 9-bit offset)\n");
    rt_kprintf("       hmi_color ccm reset      restore the build default (identity when unset)\n");
    rt_kprintf("       hmi_color ccm identity   clear all correction (identity table)\n");
    rt_kprintf("       taps: decimal, or hex with 0x/leading 0 (e.g. 0xFFD8 = -40)\n");
}

/* 一次解析一行（3 个增益 + 1 个偏移），写进整表的 row 行。 */
static bool parse_ccm_row(int row, char **tokens, int32_t table[HMI_CCM_TAPS])
{
    for (unsigned column = 0u; column < 4u; ++column)
    {
        long value = 0;
        if (!parse_ccm_tap(tokens[column], &value))
        {
            return false;
        }
        const unsigned index = ((unsigned)row * 4u) + column;
        table[index] = hmi_display_ccm_normalize(index, value);
    }
    return true;
}

/* r/g/b 行名转行号；不是单个字母返回 -1。 */
static int ccm_row_of(const char *token)
{
    if ((token[0] == '\0') || (token[1] != '\0'))
    {
        return -1;
    }
    switch (token[0])
    {
    case 'r':
    case 'R':
        return 0;
    case 'g':
    case 'G':
        return 1;
    case 'b':
    case 'B':
        return 2;
    default:
        return -1;
    }
}

/* 12 个系数按 16 位补码打成 0x 形式，和 disp_ccm.h 的写法一致，方便来回粘贴。 */
static void print_ccm_taps(const int32_t table[HMI_CCM_TAPS])
{
    for (unsigned i = 0u; i < (unsigned)HMI_CCM_TAPS; ++i)
    {
        rt_kprintf(" 0x%04X", (unsigned)(uint16_t)table[i]);
    }
}

static void print_ccm(bool with_table)
{
    int32_t table[HMI_CCM_TAPS];
    bool enabled = false;
    if (!hmi_board_color_ccm_read(table, &enabled))
    {
        rt_kprintf("display ccm: unavailable\n");
        return;
    }
    rt_kprintf("display ccm=%s, %s", enabled ? "on" : "off",
               hmi_display_ccm_is_identity(table) ? "identity" : "custom");
    if (with_table)
    {
        print_ccm_taps(table);
    }
    rt_kprintf("\n");
}

/* 回显 Kconfig 里的开机默认表，用来确认固件是否带上了调好的矩阵；未配置时明确说 none。 */
static void print_ccm_default(void)
{
    int32_t table[HMI_CCM_TAPS];
    if (!ccm_default_table(table))
    {
        rt_kprintf("display ccm default=none\n");
        return;
    }
    rt_kprintf("display ccm default=%s", hmi_display_ccm_is_identity(table) ? "identity" : "custom");
    print_ccm_taps(table);
    rt_kprintf("\n");
}

static int hmi_color(int argc, char **argv)
{
    if ((argc >= 2) && (strcmp(argv[1], "ccm") == 0))
    {
        if (argc == 2)
        {
            print_ccm(true);
            print_ccm_default();
            return 0;
        }
        if ((argc == 3) && (strcmp(argv[2], "reset") == 0))
        {
            int32_t table[HMI_CCM_TAPS];
            const bool configured = ccm_default_table(table);
            if (!configured)
            {
                hmi_display_ccm_identity(table);
            }
            if (!hmi_board_color_ccm_apply(table))
            {
                rt_kprintf("display ccm: apply failed\n");
                return -1;
            }
            rt_kprintf("display ccm: reset to %s\n",
                       configured ? "build default" : "identity (no build default)");
            print_ccm(true);
            return 0;
        }
        if ((argc == 3) && (strcmp(argv[2], "identity") == 0))
        {
            int32_t table[HMI_CCM_TAPS];
            hmi_display_ccm_identity(table);
            if (!hmi_board_color_ccm_apply(table))
            {
                rt_kprintf("display ccm: apply failed\n");
                return -1;
            }
            print_ccm(true);
            return 0;
        }
        /* 逐行设置：先读回当前整表，只覆盖选中行，再整体写回，其余行保持不动。 */
        const int row = (argc == 7) ? ccm_row_of(argv[2]) : -1;
        if (row >= 0)
        {
            int32_t table[HMI_CCM_TAPS];
            if (!hmi_board_color_ccm_read(table, NULL))
            {
                rt_kprintf("display ccm: unavailable\n");
                return -1;
            }
            if (!parse_ccm_row(row, &argv[3], table) || !hmi_board_color_ccm_apply(table))
            {
                hmi_color_usage();
                return -1;
            }
            print_ccm(true);
            return 0;
        }
        hmi_color_usage();
        return -1;
    }
    if ((argc == 3) && (strcmp(argv[1], "sat") == 0))
    {
        const long percent = strtol(argv[2], NULL, 10);
        int32_t table[HMI_CCM_TAPS];
        if ((percent < HMI_CCM_SATURATION_MIN) || (percent > HMI_CCM_SATURATION_MAX) ||
            !hmi_display_ccm_from_saturation((unsigned)percent, table) ||
            !hmi_board_color_ccm_apply(table))
        {
            hmi_color_usage();
            return -1;
        }
        print_ccm(true);
        return 0;
    }
    if (argc >= 2)
    {
        const long value = strtol(argv[1], NULL, 10);
        if ((value < HMI_GAMMA_PERCENT_MIN) || (value > HMI_GAMMA_PERCENT_MAX) ||
            !hmi_board_color_apply((unsigned)value))
        {
            hmi_color_usage();
            return -1;
        }
    }
    rt_kprintf("display gamma=%u%% (100 = DE default; build default %u%%)\n", hmi_board_color_percent(),
               (unsigned)AIC_HMI_DISPLAY_GAMMA);
    print_ccm(false);
    print_ccm_default();
    return 0;
}
MSH_CMD_EXPORT(hmi_color, Show or set display gamma and ccm);
