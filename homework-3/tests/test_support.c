#include "test_support.h"
#include "utils.h"
#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>

static int serial;
static volatile sig_atomic_t cancelled;

static void cancelTests(int signalNumber)
{
  (void)signalNumber;
  cancelled = 1;
}

void testPause(int milliseconds)
{
  struct timespec remaining = {milliseconds / 1000,
                               (milliseconds % 1000) * 1000000L};
  while (nanosleep(&remaining, &remaining) < 0 && errno == EINTR)
  {
  }
}

static double secondsNow(void)
{
  struct timespec current;
  clock_gettime(CLOCK_MONOTONIC, &current);
  return current.tv_sec + current.tv_nsec / 1000000000.0;
}

static pid_t spawn(char *const arguments[], const char *output,
                   const char *errors)
{
  pid_t child = fork();
  if (child != 0)
    return child;
  int out = open(output, O_WRONLY | O_CREAT | O_TRUNC, 0600);
  int err = open(errors, O_WRONLY | O_CREAT | O_TRUNC, 0600);
  if (out < 0 || err < 0 || dup2(out, STDOUT_FILENO) < 0 ||
      dup2(err, STDERR_FILENO) < 0)
    _exit(125);
  close(out);
  close(err);
  execv(arguments[0], arguments);
  _exit(125);
}

static int waitProcess(pid_t *process, int *status, double timeout)
{
  double deadline = secondsNow() + timeout;
  while (*process > 0)
  {
    pid_t result = waitpid(*process, status, WNOHANG);
    if (result == *process)
    {
      *process = 0;
      return 1;
    }
    if (result < 0 && errno != EINTR)
      return 0;
    if (cancelled || secondsNow() >= deadline)
      return 0;
    testPause(10);
  }
  return 1;
}

static int readText(const char *path, char *text, size_t capacity)
{
  FILE *file = fopen(path, "r");
  text[0] = 0;
  if (!file)
    return 0;
  size_t length = fread(text, 1, capacity - 1, file);
  int valid = !ferror(file) && (length < capacity - 1 || fgetc(file) == EOF);
  text[length] = 0;
  fclose(file);
  return valid;
}

static int roleCommand(const char *role, char *program, size_t capacity)
{
  const char *roles[] = {"ship", "tug", "berth", "crane", "warehouse"};
  for (int index = 0; index < 5; ++index)
  {
    if (!strcmp(role, roles[index]))
    {
      snprintf(program, capacity, "./port_%s", roles[index]);
      return 1;
    }
  }
  return 0;
}

int commandStatus(char *const arguments[])
{
  pid_t process = spawn(arguments, "/dev/null", "/dev/null");
  if (process < 0)
    return 125;
  int status = 0;
  if (!waitProcess(&process, &status, 2))
  {
    kill(process, SIGKILL);
    waitpid(process, NULL, 0);
    return 124;
  }
  return WIFEXITED(status) ? WEXITSTATUS(status) : 125;
}

int waitEvent(TestRun *run, const char *event)
{
  double deadline = secondsNow() + 5;
  do
  {
    readText(run->outputPath, run->text, sizeof(run->text));
    if (strstr(run->text, event))
      return 1;
    if (run->server > 0 &&
        waitpid(run->server, &run->serverStatus, WNOHANG) == run->server)
      run->server = 0;
    if (!run->server || cancelled)
      return 0;
    testPause(10);
  } while (secondsNow() < deadline);
  return 0;
}

int startRun(TestRun *run, const char *mode, const char *config, int delay,
             int reuse, int wrongRole)
{
  SOFT_ASSERT(run != NULL, "TestRun is required", 0);
  signal(SIGINT, cancelTests);
  signal(SIGTERM, cancelTests);
  memset(run, 0, sizeof(*run));
  snprintf(run->directory, sizeof(run->directory), "/tmp/port_c_test_%ld_%d",
           (long)getpid(), ++serial);
  if (mkdir(run->directory, 0700) < 0)
    return 0;
  snprintf(run->session, sizeof(run->session), "ct%ld_%d", (long)getpid(),
           reuse ? 0 : serial);
  snprintf(run->outputPath, sizeof(run->outputPath), "%s/output",
           run->directory);
  snprintf(run->errorPath, sizeof(run->errorPath), "%s/errors", run->directory);
  snprintf(run->logPath, sizeof(run->logPath), "%s/events", run->directory);
  char delayText[16];
  snprintf(delayText, sizeof(delayText), "%d", delay);
  char *arguments[] = {"./port_sim", (char *)mode, (char *)config,
                       delayText, run->session, run->logPath,
                       NULL};
  run->server = spawn(arguments, run->outputPath, run->errorPath);
  if (run->server <= 0)
  {
    run->server = 0;
    return 0;
  }
  if (strcmp(mode, "shm") && strcmp(mode, "fifo"))
    return 1;
  if (!waitEvent(run, "READY workers="))
    return 0;
  int expected = 0, found = 0;
  char roles[TEST_ACTORS][16] = {{0}};
  double deadline = secondsNow() + 5;
  do
  {
    readText(run->outputPath, run->text, sizeof(run->text));
    const char *ready = strstr(run->text, "READY workers=");
    if (ready)
      sscanf(ready, "READY workers=%d", &expected);
    const char *position = run->text;
    found = 0;
    memset(roles, 0, sizeof(roles));
    while ((position = strstr(position, "ACTOR index=")) != NULL)
    {
      int index;
      char role[16];
      if (sscanf(position, "ACTOR index=%d role=%15s", &index, role) == 2 &&
          index >= 0 && index < TEST_ACTORS && !roles[index][0])
      {
        snprintf(roles[index], sizeof(roles[index]), "%s", role);
        ++found;
      }
      ++position;
    }
    if (expected > 0 && expected <= TEST_ACTORS && found == expected)
      break;
    testPause(10);
  } while (secondsNow() < deadline);
  if (!expected || expected > TEST_ACTORS || found != expected)
    return 0;
  if (wrongRole)
  {
    char *wrong[] = {"./port_tug", run->session, "0", NULL};
    if (commandStatus(wrong) != 2)
      return 0;
  }
  for (int index = 0; index < expected; ++index)
  {
    char program[40], number[16], output[180], errors[180];
    if (!roleCommand(roles[index], program, sizeof(program)))
      return 0;
    snprintf(number, sizeof(number), "%d", index);
    snprintf(output, sizeof(output), "%s/actor_%d_out", run->directory, index);
    snprintf(errors, sizeof(errors), "%s/actor_%d_err", run->directory, index);
    char *worker[] = {program, run->session, number, NULL};
    pid_t process = spawn(worker, output, errors);
    if (process < 0)
      return 0;
    run->workers[run->workerCount++] = process;
  }
  return 1;
}

void stopRun(TestRun *run)
{
  sig_atomic_t savedCancellation = cancelled;
  cancelled = 0;
  if (run->server > 0)
  {
    kill(run->server, SIGCONT);
    kill(run->server, SIGTERM);
  }
  for (int index = 0; index < run->workerCount; ++index)
  {
    if (run->workers[index] > 0)
    {
      kill(run->workers[index], SIGCONT);
      kill(run->workers[index], SIGTERM);
    }
  }
  int status;
  if (run->server > 0 && !waitProcess(&run->server, &status, 3))
  {
    kill(run->server, SIGKILL);
    waitpid(run->server, NULL, 0);
    run->server = 0;
  }
  for (int index = 0; index < run->workerCount; ++index)
  {
    if (run->workers[index] > 0 &&
        !waitProcess(&run->workers[index], &status, 3))
    {
      kill(run->workers[index], SIGKILL);
      waitpid(run->workers[index], NULL, 0);
      run->workers[index] = 0;
    }
    char path[180];
    snprintf(path, sizeof(path), "%s/actor_%d_out", run->directory, index);
    unlink(path);
    snprintf(path, sizeof(path), "%s/actor_%d_err", run->directory, index);
    unlink(path);
  }
  unlink(run->outputPath);
  unlink(run->errorPath);
  unlink(run->logPath);
  rmdir(run->directory);
  cancelled = savedCancellation;
}

int finishRun(TestRun *run, int expectedCode)
{
  int valid = waitProcess(&run->server, &run->serverStatus, 30);
  if (!valid || !WIFEXITED(run->serverStatus) ||
      WEXITSTATUS(run->serverStatus) != expectedCode)
  {
    fprintf(stderr, "Dispatcher exit mismatch or timeout, session=%s\n",
            run->session);
    stopRun(run);
    return 0;
  }
  readText(run->outputPath, run->text, sizeof(run->text));
  char *buffer = malloc(sizeof(run->text));
  if (!buffer)
  {
    stopRun(run);
    return 0;
  }
  valid = readText(run->logPath, buffer, sizeof(run->text)) &&
          !strcmp(buffer, run->text);
  if (!readText(run->errorPath, buffer, sizeof(run->text)) ||
      strstr(buffer, "AddressSanitizer") || strstr(buffer, "runtime error:"))
    valid = 0;
  for (int index = 0; index < run->workerCount; ++index)
  {
    int status;
    if (run->workers[index] > 0)
    {
      if (!waitProcess(&run->workers[index], &status, 3) ||
          !WIFEXITED(status) || WEXITSTATUS(status) != 0)
        valid = 0;
    }
    char path[180];
    snprintf(path, sizeof(path), "%s/actor_%d_err", run->directory, index);
    if (!readText(path, buffer, sizeof(run->text)) ||
        strstr(buffer, "AddressSanitizer") || strstr(buffer, "runtime error:"))
      valid = 0;
  }
  free(buffer);
  char ipcPath[160];
  snprintf(ipcPath, sizeof(ipcPath), "/tmp/port3_%lu_%s",
           (unsigned long)getuid(), run->session);
  if (access(ipcPath, F_OK) == 0)
    valid = 0;
  snprintf(ipcPath, sizeof(ipcPath), "/dev/shm/port3_%lu_%s",
           (unsigned long)getuid(), run->session);
  if (access(ipcPath, F_OK) == 0)
    valid = 0;
  if (expectedCode == 0 &&
      !strstr(run->text, "Проверка ограничений ресурсов: пройдена"))
    valid = 0;
  const char *position = run->text;
  while (position && *position)
  {
    const char *warehouse = strstr(position, "warehouse=");
    const char *used = strstr(position, "used=");
    const char *candidate =
        warehouse && (!used || warehouse < used) ? warehouse : used;
    if (!candidate)
      break;
    int occupied, reserved, capacity;
    if (sscanf(candidate, "%*[^=]=%d reserved=%d capacity=%d", &occupied,
               &reserved, &capacity) == 3 &&
        occupied + reserved > capacity)
      valid = 0;
    position = candidate + 1;
  }
  stopRun(run);
  return valid;
}
