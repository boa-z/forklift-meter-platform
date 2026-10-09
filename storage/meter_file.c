#include "storage/meter_file.h"
#include <errno.h>
#include <stdio.h>
#include <string.h>
#ifdef METER_RTTHREAD
#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>
#else
#include <fcntl.h>
#include <sys/stat.h>
#ifdef _WIN32
#include <io.h>
#define open _open
#define close _close
#define read _read
#define write _write
#define lseek _lseek
#define fsync _commit
#else
#include <unistd.h>
#endif
#endif
#ifndef O_BINARY
#define O_BINARY 0
#endif

static int open_slot(meter_file_t *f, size_t slot, bool writing, bool *created)
{
    char path[200];
    (void)snprintf(path, sizeof(path), "%s.%u", f->prefix, (unsigned)slot);
    *created = false;
    int fd = open(path, (writing ? O_RDWR : O_RDONLY) | O_BINARY, 0600);
    if (fd < 0 && writing && errno == ENOENT)
    {
        fd = open(path, O_RDWR | O_CREAT | O_EXCL | O_BINARY, 0600);
        *created = fd >= 0;
    }
    return fd;
}
static meter_io_result_t transfer(meter_file_t *f, size_t offset, uint8_t *dst, const uint8_t *src,
                                  size_t size)
{
    if (!f)
        return METER_IO_RANGE;
    /* 单次传输固定已验证的槽几何，不跨文件 I/O 重新读取可变上下文。 */
    const size_t slot_size = f->slot_size;
    if (!slot_size || offset > f->io.capacity ||
        size > f->io.capacity - offset || (!dst && !src && size))
        return METER_IO_RANGE;
    while (size)
    {
        const size_t within = offset % slot_size;
        size_t n = slot_size - within;
        if (n > size)
            n = size;
        bool created;
        int fd = open_slot(f, offset / slot_size, src != NULL, &created);
        if (fd < 0)
        {
            if (!src && errno == ENOENT)
                memset(dst, 0xFF, n);
            else
                return METER_IO_ERROR;
        }
        else
        {
            bool ok = true;
            if (created)
            {
                uint8_t blank[128];
                memset(blank, 0xFF, sizeof(blank));
                size_t remain = slot_size;
                while (remain && ok)
                {
                    const size_t part = remain < sizeof(blank) ? remain : sizeof(blank);
                    ok = write(fd, blank, (unsigned)part) == (int)part;
                    remain -= part;
                }
            }
            if (lseek(fd, (long)within, SEEK_SET) != (long)within)
                ok = false;
            if (ok && src)
            {
                ok = write(fd, src, (unsigned)n) == (int)n;
                if (fsync(fd) != 0)
                    ok = false;
            }
            else if (ok)
            {
                const int got = (int)read(fd, dst, (unsigned)n);
                if (got < 0)
                    ok = false;
                else if ((size_t)got < n)
                    memset(dst + got, 0xFF, n - (size_t)got);
            }
            if (close(fd) != 0)
                ok = false;
            if (!ok)
                return METER_IO_ERROR;
        }
        offset += n;
        size -= n;
        if (src)
            src += n;
        else
            dst += n;
    }
    return METER_IO_OK;
}
static meter_io_result_t file_read(void *ctx, size_t o, uint8_t *p, size_t n)
{
    return transfer(ctx, o, p, NULL, n);
}
static meter_io_result_t file_write(void *ctx, size_t o, const uint8_t *p, size_t n)
{
    return transfer(ctx, o, NULL, p, n);
}
static meter_io_result_t file_sync(void *ctx)
{
    (void)ctx;
    /* 每次写均检查 fsync 与 close；不宣称文件系统目录/FTL 具有额外掉电保证。 */
    return METER_IO_OK;
}
bool meter_file_init(meter_file_t *f, const char *prefix, size_t slot_size, const char *name, bool power_safe)
{
    if (!f || !prefix || !*prefix || strlen(prefix) >= sizeof(f->prefix) || !name || slot_size < 128u ||
        slot_size > 65536u || slot_size % 16u || power_safe)
        return false;
    memset(f, 0, sizeof(*f));
    memcpy(f->prefix, prefix, strlen(prefix) + 1u);
    f->slot_size = slot_size;
    f->io = (meter_nvm_io_t){f, file_read, file_write, file_sync, slot_size * 2u, 16u, name, false};
    return true;
}
