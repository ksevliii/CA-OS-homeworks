#include "test_support.h"
#include <arpa/inet.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

static TestRun run;
static int passed;

static int check(int condition, const char *name)
{
  if (!condition)
  {
    fprintf(stderr, "FAIL: %s; evidence: %s\n", name, run.directory);
    stopRun(&run);
    return 0;
  }
  printf("PASS %d: %s\n", ++passed, name);
  return 1;
}

static int fixture(const char *name, const char *expected)
{
  char config[160];
  snprintf(config, sizeof(config), "examples/%s.txt", name);
  return check(startRun(&run, config, 0, 1) && finishRun(&run) &&
                   waitText(run.log, expected, 0),
               name);
}

static int observers(void)
{
  if (!startRun(&run, "examples/manual.txt", 100, 0))
    return check(0, "multiple observers");
  startObserver(&run, 0);
  startObserver(&run, 1);
  char output[180];
  snprintf(output, sizeof(output), "%s/observer_0.txt", run.directory);
  if (!waitText(output, "WAITING FOR WORKERS", 3))
    return check(0, "multiple observers");
  stopProcess(&run.observers[0], SIGINT);
  testPause(100);
  startObserver(&run, 0);
  for (int index = 0; index < run.count; ++index)
    startWorker(&run, index);
  if (!waitText(run.log, "unloading started", 5))
    return check(0, "multiple observers");
  startObserver(&run, 2);
  if (!finishRun(&run))
    return check(0, "multiple observers");
  for (int index = 0; index < 3; ++index)
  {
    snprintf(output, sizeof(output), "%s/observer_%d.txt", run.directory, index);
    if (!waitText(output, "FINISHED", 0) || !waitText(output, "FINAL arrived=1 departed=1", 0))
      return check(0, "multiple observers");
  }
  return check(1, "three observers, late join, disconnect/reconnect, final state");
}

static int reconnectWorker(int actor)
{
  if (!startRun(&run, "examples/manual.txt", 150, 1))
    return check(0, "worker reconnect");
  startObserver(&run, 0);
  char output[180], disconnected[100];
  snprintf(output, sizeof(output), "%s/worker_%d.txt", run.directory, actor);
  if (!waitText(output, "started", 5))
    return check(0, "worker reconnect");
  stopProcess(&run.workers[actor], SIGKILL);
  snprintf(disconnected, sizeof(disconnected), "Worker %d disconnected", actor);
  if (!waitText(run.serverOutput, disconnected, 3))
    return check(0, "worker reconnect");
  testPause(150);
  if (kill(run.server, 0) < 0 || startWorker(&run, actor) <= 0)
    return check(0, "worker reconnect");
  char label[120];
  snprintf(label, sizeof(label), "worker %d reconnects; cargo committed once", actor);
  return check(finishRun(&run) && waitText(run.log, "cargo_operations=1 moved=8", 0), label);
}

static int serverStop(void)
{
  if (!startRun(&run, "examples/manual.txt", 1000, 1))
    return check(0, "server shutdown");
  startObserver(&run, 0);
  if (!waitText(run.log, "assigned, entering", 5))
    return check(0, "server shutdown");
  kill(run.server, SIGINT);
  return check(finishRun(&run) && waitText(run.log, "Interrupted by user signal", 0),
               "SIGINT stops server, active workers and observer");
}

static int rejectDuplicate(void)
{
  if (!startRun(&run, "examples/manual.txt", 100, 0))
    return check(0, "registration validation");
  startWorker(&run, 0);
  if (!waitText(run.serverOutput, "Worker 0 connected", 3))
    return check(0, "registration validation");
  char port[16], output[180];
  snprintf(port, sizeof(port), "%d", run.port);
  snprintf(output, sizeof(output), "%s/rejected.txt", run.directory);
  char *duplicate[] = {"./port_ship", "127.0.0.1", port, "0", NULL};
  pid_t process = launch(duplicate, output);
  if (!finishProcess(&process, 3, 5))
    return check(0, "registration validation");
  char *wrongRole[] = {"./port_crane", "127.0.0.1", port, "1", NULL};
  process = launch(wrongRole, output);
  if (!finishProcess(&process, 3, 5))
    return check(0, "registration validation");
  for (int index = 1; index < run.count; ++index)
    startWorker(&run, index);
  return check(finishRun(&run), "duplicate and wrong role rejected; server continues");
}

static int badFrame(void)
{
  if (!startRun(&run, "examples/manual.txt", 0, 0))
    return check(0, "malformed TCP frame");
  int descriptor = socket(AF_INET, SOCK_STREAM, 0);
  struct sockaddr_in address = {.sin_family = AF_INET, .sin_port = htons((unsigned short)run.port), .sin_addr.s_addr = htonl(INADDR_LOOPBACK)};
  if (descriptor < 0 || connect(descriptor, (struct sockaddr *)&address, sizeof(address)) < 0)
    return check(0, "malformed TCP frame");
  uint32_t header[] = {htonl(1), htonl(999999)};
  send(descriptor, header, sizeof(header), 0);
  close(descriptor);
  for (int index = 0; index < run.count; ++index)
    startWorker(&run, index);
  return check(finishRun(&run), "oversized TCP frame rejected without stopping server");
}

static int invalidInput(void)
{
  char output[180];
  snprintf(output, sizeof(output), "%s/invalid_cli.txt", run.directory);
  char *arguments[] = {"./port_server", "127.0.0.1", "0", "examples/manual.txt", NULL};
  pid_t process = launch(arguments, output);
  if (!finishProcess(&process, 2, 3))
    return check(0, "invalid endpoint and config");
  char config[180];
  snprintf(config, sizeof(config), "%s/invalid_config.txt", run.directory);
  FILE *file = fopen(config, "w");
  if (!file)
    return check(0, "invalid endpoint and config");
  fputs("100 17 garbage", file);
  fclose(file);
  char *bad[] = {"./port_server", "127.0.0.1", "45679", config, NULL};
  process = launch(bad, output);
  return check(finishProcess(&process, 2, 3), "invalid endpoint and incomplete configuration");
}

int main(void)
{
  if (!fixture("manual", "departed=1 pending=0") ||
      !fixture("loading_multi", "departed=2 pending=0") ||
      !fixture("warehouse_wait", "departed=2 pending=0") ||
      !fixture("loading_partial_reserved", "departed=2 pending=0") ||
      !fixture("unloading_dependency", "departed=2 pending=0") ||
      !fixture("time_limit", "simulation time limit") ||
      !fixture("zero_ships", "arrived=0 departed=0") || !observers())
    return 1;
  for (int actor = 0; actor < 5; ++actor)
    if (!reconnectWorker(actor))
      return 1;
  if (!serverStop() || !rejectDuplicate() || !badFrame() ||
      !invalidInput() || !check(testFrames(), "fragmented and concatenated frames"))
    return 1;
  printf("ALL %d GROUPS PASSED\n", passed);
  return 0;
}
