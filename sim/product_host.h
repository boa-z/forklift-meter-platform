#ifndef METER_PRODUCT_HOST_H
#define METER_PRODUCT_HOST_H
#include "runtime/meter_runtime.h"
/** @brief Host 通用生命周期；step 只投递合成帧，全部回调在唯一 UI 线程串行执行。 */
int meter_product_host(int argc, char **argv, void (*step)(meter_runtime_t *, uint32_t));
#endif
