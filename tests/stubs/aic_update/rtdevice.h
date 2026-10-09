#pragma once
#include <rtthread.h>
struct rt_mtd_nand_device
{
    struct rt_device parent;
    uint32_t page_size, pages_per_block, block_total, block_start, block_end;
};
int rt_mtd_nand_check_block(struct rt_mtd_nand_device *, uint32_t);
int rt_mtd_nand_read(struct rt_mtd_nand_device *, uint32_t, void *, size_t, void *, size_t);
