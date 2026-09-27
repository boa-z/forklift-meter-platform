#include "ui/common/widgets/needle/meter_needle.h"
#include <math.h>
void meter_needle_points(lv_point_precise_t p[2], float angle, int diameter, int length)
{
    float c = diameter / 2.0f, rad = angle * 0.01745329252f;
    p[0].x = c;
    p[0].y = c;
    p[1].x = c + cosf(rad) * length;
    p[1].y = c + sinf(rad) * length;
}
