#pragma once
enum boot_device
{
    BD_SPINAND,
    BD_SDMC0,
    BD_SDMC1
};
enum boot_device aic_get_boot_device(void);
