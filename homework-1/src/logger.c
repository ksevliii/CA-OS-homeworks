#include "logger.h"
#include "utils.h"
#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>

bool loggerOpen(Logger *log, const char *path)
{
    SOFT_ASSERT(log != NULL, "Logger pointer is NULL", false);
    SOFT_ASSERT(path != NULL, "Log path is NULL", false);
    log->fd = -1;
    log->opened = false;
    int fd = open(path, O_WRONLY | O_CREAT | O_TRUNC, 0666);
    SOFT_ASSERT(fd >= 0, "loggerOpen: open failed", false);
    log->fd = fd;
    log->opened = true;
    return true;
}

bool loggerWrite(const Logger *log, const char *buf, int len)
{
    SOFT_ASSERT(log != NULL, "Logger pointer is NULL", false);
    SOFT_ASSERT(buf != NULL, "Write buffer is NULL", false);
    SOFT_ASSERT(log->opened, "Logger not opened", false);
    SOFT_ASSERT(len > 0, "Write length <= 0", false);
    ssize_t written = write(log->fd, buf, (size_t)len);
    SOFT_ASSERT(written == len, "loggerWrite: write failed", false);

    return true;
}

bool loggerWriteStr(const Logger *log, const char *str)
{
    SOFT_ASSERT(str != NULL, "String to write is NULL", false);
    return loggerWrite(log, str, (int)strlen(str));
}

void loggerClose(Logger *log)
{
    if (log == NULL)
    {
        fprintf(stderr, "SOFT_ASSERT failed: log != NULL\n");
        perror("Logger pointer is NULL");
        return;
    }
    if (!log->opened)
        return;
    if (close(log->fd) < 0)
    {
        perror("loggerClose: close failed");
    }
    log->fd = -1;
    log->opened = false;
}