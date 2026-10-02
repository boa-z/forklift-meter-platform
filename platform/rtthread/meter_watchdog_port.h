#ifndef METER_WATCHDOG_PORT_H
#define METER_WATCHDOG_PORT_H
#include <stdbool.h>
#include <stdint.h>

/** @brief 监督状态快照，供 MSH 诊断与文档复核；不返回指针，不参与判据。 */
typedef struct
{
    bool device_present, armed, supervising;
    uint32_t feeds, withheld, stalls, transitions;
    uint32_t stalled_channel;
    uint16_t timeout_s;
} meter_watchdog_report_t;

/** @brief 绑定 "wdt" 设备并启动监督线程；由 INIT_APP_EXPORT 自动调用。 */
int meter_watchdog_port_init(void);

/** @brief 复制当前监督状态；设备缺失或未启用时返回 false。 */
bool meter_watchdog_report(meter_watchdog_report_t *out);
#endif
