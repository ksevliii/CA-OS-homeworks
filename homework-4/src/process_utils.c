#include "process.h"

volatile sig_atomic_t interrupted;

void onSigint(int signalNumber)
{ // для Ctrl+C прерывания
  (void)signalNumber;
  interrupted = 1;
}

void pauseMs(int milliseconds)
{
  struct timespec remainingTime = {milliseconds / 1000,
                                   (long)(milliseconds % 1000) * 1000000L};
  while (nanosleep(&remainingTime, &remainingTime) < 0 && errno == EINTR &&
         !interrupted)
  {
  }
}

double monotonic(void)
{
  struct timespec currentTime;
  clock_gettime(CLOCK_MONOTONIC, &currentTime);
  return (double)currentTime.tv_sec + (double)currentTime.tv_nsec / 1e9;
}

int fullWrite(int descriptor, const void *data, size_t size)
{
  const char *cursor = data;
  while (size)
  {
    ssize_t written = write(descriptor, cursor, size);
    if (written < 0 && errno == EINTR && !interrupted)
      continue;
    if (written <= 0)
      return 0;
    cursor += written;
    size -= (size_t)written;
  }
  return 1;
}

int number(const char *text, int maximum, int *value)
{
  SOFT_ASSERT(value != NULL, "Number output pointer is required", 0);
  if (!text || !*text)
    return 0;
  int parsed = 0;
  for (; *text; ++text)
  {
    if (!isdigit((unsigned char)*text) ||
        parsed > (maximum - (*text - '0')) / 10)
      return 0;
    parsed = parsed * 10 + *text - '0';
    if (parsed > maximum)
      return 0;
  }
  *value = parsed;
  return 1;
}
