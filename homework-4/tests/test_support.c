#include "test_support.h"
#include <arpa/inet.h>
#include <fcntl.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>

void testPause(int milliseconds)
{
  struct timespec duration = {milliseconds / 1000, (long)(milliseconds % 1000) * 1000000};
  nanosleep(&duration, NULL);
}

pid_t launch(char *const arguments[], const char *output)
{
  pid_t process = fork();
  if (process != 0)
    return process;
  int descriptor = open(output, O_WRONLY | O_CREAT | O_TRUNC, 0600);
  if (descriptor < 0 || dup2(descriptor, STDOUT_FILENO) < 0 ||
      dup2(descriptor, STDERR_FILENO) < 0)
    _exit(127);
  close(descriptor);
  execv(arguments[0], arguments);
  _exit(127);
}

int waitText(const char *path, const char *text, int seconds)
{
  for (int attempt = 0; attempt <= seconds * 50; ++attempt)
  {
    FILE *file = fopen(path, "r");
    if (file)
    {
      char line[2000];
      int found = 0;
      while (fgets(line, sizeof(line), file))
        if (strstr(line, text))
          found = 1;
      fclose(file);
      if (found)
        return 1;
    }
    testPause(20);
  }
  fprintf(stderr, "Missing '%s' in %s\n", text, path);
  return 0;
}

int finishProcess(pid_t *process, int expected, int seconds)
{
  if (*process <= 0)
    return 1;
  for (int attempt = 0; attempt < seconds * 50; ++attempt)
  {
    int status;
    if (waitpid(*process, &status, WNOHANG) == *process)
    {
      *process = 0;
      return WIFEXITED(status) && WEXITSTATUS(status) == expected;
    }
    testPause(20);
  }
  fprintf(stderr, "Process %ld exceeded deadline\n", (long)*process);
  return 0;
}

void stopProcess(pid_t *process, int signalNumber)
{
  if (*process > 0)
  {
    kill(*process, SIGCONT);
    kill(*process, signalNumber);
    if (!finishProcess(process, signalNumber == SIGINT ? 0 : 3, 1) && *process > 0)
    {
      kill(*process, SIGKILL);
      waitpid(*process, NULL, 0);
      *process = 0;
    }
  }
}

void stopRun(TestRun *run)
{
  stopProcess(&run->server, SIGTERM);
  for (int index = 0; index < run->count; ++index)
    stopProcess(&run->workers[index], SIGTERM);
  for (int index = 0; index < 3; ++index)
    stopProcess(&run->observers[index], SIGTERM);
}

pid_t startWorker(TestRun *run, int actor)
{
  const char *programs[] = {"./port_ship", "./port_tug", "./port_berth",
                            "./port_crane", "./port_warehouse"};
  char port[16], slot[16], output[180];
  snprintf(port, sizeof(port), "%d", run->port);
  snprintf(slot, sizeof(slot), "%d", actor);
  snprintf(output, sizeof(output), "%s/worker_%d.txt", run->directory, actor);
  char *arguments[] = {(char *)programs[run->role[actor]], "127.0.0.1", port, slot, NULL};
  run->workers[actor] = launch(arguments, output);
  return run->workers[actor];
}

pid_t startObserver(TestRun *run, int index)
{
  char port[16], output[180];
  snprintf(port, sizeof(port), "%d", run->port);
  snprintf(output, sizeof(output), "%s/observer_%d.txt", run->directory, index);
  char *arguments[] = {"./port_observer", "127.0.0.1", port, NULL};
  run->observers[index] = launch(arguments, output);
  return run->observers[index];
}

static int availablePort(void)
{
  int descriptor = socket(AF_INET, SOCK_STREAM, 0);
  struct sockaddr_in address = {.sin_family = AF_INET, .sin_addr.s_addr = htonl(INADDR_LOOPBACK)};
  socklen_t size = sizeof(address);
  int port = 0;
  if (descriptor >= 0 && bind(descriptor, (struct sockaddr *)&address, size) == 0 &&
      getsockname(descriptor, (struct sockaddr *)&address, &size) == 0)
    port = ntohs(address.sin_port);
  if (descriptor >= 0)
    close(descriptor);
  return port;
}

int startRun(TestRun *run, const char *config, int delay, int workers)
{
  memset(run, 0, sizeof(*run));
  snprintf(run->directory, sizeof(run->directory), "/tmp/port_tcp_test_XXXXXX");
  if (!mkdtemp(run->directory) || !(run->port = availablePort()))
    return 0;
  snprintf(run->log, sizeof(run->log), "%s/port.log", run->directory);
  snprintf(run->serverOutput, sizeof(run->serverOutput), "%s/server.txt", run->directory);
  FILE *file = fopen(config, "r");
  int horizon, tugs, berths, cranes, capacity, entry, cargo, ships, seed;
  if (!file)
    return 0;
  int count = fscanf(file, "%d%d%d%d%d%d%d%d%d", &horizon, &tugs, &berths, &cranes,
                     &capacity, &entry, &cargo, &ships, &seed);
  fclose(file);
  if (count != 9)
    return 0;
  int counts[] = {ships, tugs, berths, cranes, 1};
  for (int role = 0; role < 5; ++role)
    for (int index = 0; index < counts[role]; ++index)
      run->role[run->count++] = role;
  char port[16], delayText[16];
  snprintf(port, sizeof(port), "%d", run->port);
  snprintf(delayText, sizeof(delayText), "%d", delay);
  char *arguments[] = {"./port_server", "127.0.0.1", port, (char *)config, delayText, run->log, NULL};
  run->server = launch(arguments, run->serverOutput);
  if (run->server <= 0 || !waitText(run->log, "READY", 5))
    return 0;
  if (workers)
    for (int index = 0; index < run->count; ++index)
      if (startWorker(run, index) <= 0)
        return 0;
  return 1;
}

int finishRun(TestRun *run)
{
  if (!finishProcess(&run->server, 0, 15))
    return 0;
  for (int index = 0; index < run->count; ++index)
    if (!finishProcess(&run->workers[index], 0, 5))
      return 0;
  for (int index = 0; index < 3; ++index)
    if (!finishProcess(&run->observers[index], 0, 5))
      return 0;
  return waitText(run->log, "Проверка ограничений ресурсов: пройдена", 0);
}
