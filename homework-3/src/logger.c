#include "logger.h"
#include "utils.h"
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

bool loggerOpen(Logger *log, const char *path)
{
  SOFT_ASSERT(log != NULL, "Logger pointer is NULL", false);
  SOFT_ASSERT(path != NULL, "Log path is NULL", false);
  log->fd = -1;
  log->opened = false;
  int fd = open(path, O_WRONLY | O_CREAT | O_NONBLOCK, 0666);
  SOFT_ASSERT(fd >= 0, "loggerOpen: open failed", false);
  struct stat info;
  if (fstat(fd, &info) < 0 || !S_ISREG(info.st_mode) || ftruncate(fd, 0) < 0)
  {
    fprintf(stderr, "Log must be a writable regular file\n");
    close(fd);
    return false;
  }
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
  size_t left = (size_t)len;
  while (left)
  {
    ssize_t written = write(log->fd, buf, left);
    if (written < 0 && errno == EINTR)
      continue;
    if (written <= 0)
      return false;
    buf += written;
    left -= (size_t)written;
  }

  return true;
}

bool loggerWriteStr(const Logger *log, const char *str)
{
  SOFT_ASSERT(str != NULL, "String to write is NULL", false);
  size_t length = strlen(str);
  if (length > INT_MAX)
    return false;
  if (!length)
    return true;
  return loggerWrite(log, str, (int)length);
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
