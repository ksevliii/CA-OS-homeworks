#include "test_support.h"
#include "utils.h"
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <unistd.h>

static TestRun run;
static int passed;
static char temporary[] = "/tmp/port_inputs_XXXXXX";

typedef struct
{
  const char *name;
  int departed;
} Fixture;

static void pass(const char *name)
{
  printf("PASS %d: %s\n", ++passed, name);
  fflush(stdout);
}

static int contains(const char *text)
{
  if (strstr(run.text, text))
    return 1;
  fprintf(stderr, "Missing event: %s\n", text);
  return 0;
}

static int assignedShip(const char *event, int ordinal)
{
  const char *line = run.text;
  while (*line)
  {
    const char *end = strchr(line, '\n');
    const char *found = strstr(line, event);
    if (found && (!end || found < end) && --ordinal == 0)
    {
      int ship;
      if (sscanf(line, "[T=%*d] Ship #%d", &ship) == 1)
        return ship;
    }
    if (!end)
      break;
    line = end + 1;
  }
  return -1;
}

static int checkFixture(const Fixture *fixture)
{
  char expected[64];
  if (fixture->departed >= 0)
  {
    snprintf(expected, sizeof(expected), "departed=%d pending=0",
             fixture->departed);
    if (!contains(expected))
      return 0;
  }
  if (!strcmp(fixture->name, "loading_multi"))
    return contains("loading started") && contains("cranes=2") &&
           contains("reserves berth");
  if (!strcmp(fixture->name, "warehouse_wait"))
    return contains("suspended") && contains("resumed");
  if (!strcmp(fixture->name, "shortest_first"))
    return assignedShip("compatible crane", 1) == 1;
  if (!strcmp(fixture->name, "order_id"))
    return assignedShip("assigned, entering", 1) == 2 &&
           assignedShip("assigned, entering", 2) == 0;
  if (!strcmp(fixture->name, "order_fcfs"))
    return assignedShip("assigned, entering", 1) == 2 &&
           assignedShip("assigned, entering", 2) == 1;
  if (!strcmp(fixture->name, "time_limit"))
    return contains("simulation time limit") && contains("not finished");
  if (!strcmp(fixture->name, "no_export"))
    return contains("pending=1 partial=1");
  if (!strcmp(fixture->name, "max_actors"))
    return contains("actors=81");
  if (!strcmp(fixture->name, "late_arrival"))
    return contains("arrived=0 departed=0 pending=1");
  if (!strcmp(fixture->name, "compatibility"))
    return contains("tug #1 assigned, entering") &&
           contains("berth #1 assigned") &&
           contains("compatible crane #1 assigned");
  if (strstr(fixture->name, "loading_partial") ||
      strstr(fixture->name, "unloading_dependency"))
    return contains("unmooring to the port waiting area");
  return 1;
}

static int runFixture(const char *mode, const Fixture *fixture)
{
  char config[160], label[200];
  snprintf(config, sizeof(config), "examples/%s.txt", fixture->name);
  if (!startRun(&run, mode, config, 0, 0, 0))
  {
    stopRun(&run);
    return 0;
  }
  if (!finishRun(&run, 0) || !checkFixture(fixture))
  {
    fprintf(stderr, "%s\n", run.text);
    return 0;
  }
  snprintf(label, sizeof(label), "%s: %s", mode, fixture->name);
  pass(label);
  return 1;
}

static int runAction(const char *mode, const char *action)
{
  int reuse = !strcmp(action, "reuse");
  int wrongRole = !strcmp(action, "rolecheck");
  const char *config = reuse       ? "examples/zero_ships.txt"
                       : wrongRole ? "examples/complete.txt"
                                   : "examples/unlimited_blocked.txt";
  if (!startRun(&run, mode, config, reuse || wrongRole ? 0 : 10, reuse,
                wrongRole))
  {
    stopRun(&run);
    return 0;
  }
  int expected = 0;
  if (!reuse && !wrongRole)
  {
    if (!waitEvent(&run, "Ship #0 arrived:"))
    {
      stopRun(&run);
      return 0;
    }
    if (!strcmp(action, "crash"))
    {
      if (run.workerCount == 0)
      {
        stopRun(&run);
        return 0;
      }
      kill(run.workers[0], SIGKILL);
      waitpid(run.workers[0], NULL, 0);
      run.workers[0] = 0;
      expected = 3;
    }
    else if (!strcmp(action, "duplicate"))
    {
      char *arguments[] = {"./port_ship", run.session, "0", NULL};
      if (commandStatus(arguments) != 3)
      {
        stopRun(&run);
        return 0;
      }
      kill(run.server, SIGINT);
    }
    else if (!strcmp(action, "stopped_shutdown"))
    {
      kill(run.workers[0], SIGSTOP);
      testPause(50);
      kill(run.server, SIGINT);
    }
    else if (!strcmp(action, "stalled"))
    {
      kill(run.workers[0], SIGSTOP);
      testPause(6000);
      int status;
      if (waitpid(run.server, &status, WNOHANG) != 0)
      {
        run.server = 0;
        stopRun(&run);
        return 0;
      }
      kill(run.workers[0], SIGCONT);
      kill(run.server, SIGINT);
    }
    else
    {
      kill(run.server, !strcmp(action, "SIGTERM") ? SIGTERM : SIGINT);
    }
  }
  if (!finishRun(&run, expected))
    return 0;
  if (!expected && !reuse && !wrongRole &&
      !contains("Interrupted by user signal"))
    return 0;
  char label[160];
  snprintf(label, sizeof(label), "%s: %s", mode, action);
  pass(label);
  return 1;
}

static int reject(const char *path)
{
  char *arguments[] = {"./port_sim", "fork-named", (char *)path, "0", NULL};
  return commandStatus(arguments) == 2;
}

static int writeFile(const char *path, const void *data, size_t length)
{
  FILE *file = fopen(path, "wb");
  if (!file)
    return 0;
  int valid = fwrite(data, 1, length, file) == length;
  if (fclose(file))
    valid = 0;
  return valid;
}

static int testInput(void)
{
  char path[160], valid[2048];
  snprintf(path, sizeof(path), "%s/invalid", temporary);
  FILE *source = fopen("examples/complete.txt", "r");
  if (!source)
    return 0;
  size_t length = fread(valid, 1, sizeof(valid) - 1, source);
  fclose(source);
  valid[length] = 0;
  const char *bad[] = {"", "-1", "+1", "NaN", "99999999999999999999",
                       "1.0", "1e2"};
  for (size_t index = 0; index < sizeof(bad) / sizeof(bad[0]); ++index)
    if (!writeFile(path, bad[index], strlen(bad[index])) || !reject(path))
      return 0;
  pass("invalid numeric tokens");
  const size_t cuts[] = {1, 5, 15, 24, 35, 46, 53};
  for (size_t index = 0; index < sizeof(cuts) / sizeof(cuts[0]); ++index)
    if (!writeFile(path, valid, cuts[index]) || !reject(path))
      return 0;
  pass("truncated configuration");
  const char *patterns[] = {"300 2 2 2 20", "300 2 2 2 20", "1 1 1 1",
                            "0 1 1 1", "0 0 30 0", "0 0 30 0"};
  const char *replacements[] = {"300 17 2 2 20", "300 2 2 2 0", "0 1 1 1",
                                "0 2 1 1", "3 0 30 0", "0 2 30 0"};
  for (size_t index = 0; index < sizeof(patterns) / sizeof(patterns[0]);
       ++index)
  {
    const char *found = strstr(valid, patterns[index]);
    if (!found)
      return 0;
    char input[2200];
    snprintf(input, sizeof(input), "%.*s%s%s", (int)(found - valid), valid,
             replacements[index], found + strlen(patterns[index]));
    if (!writeFile(path, input, strlen(input)) || !reject(path))
      return 0;
  }
  char inconsistent[2200];
  snprintf(inconsistent, sizeof(inconsistent),
           "%s\nEXT 1 0 1 4 1 1 0 1 0 0 7 7 10 10 0 1 0 1 0 1\n", valid);
  if (!writeFile(path, inconsistent, strlen(inconsistent)) || !reject(path))
    return 0;
  pass("semantic bounds, duplicate IDs and inconsistent warehouse mask");
  const char *tails[] = {"rubbish", "EXT", "EXT 0", "EXT 3 0 1 4 1 3 0 0 0 0"};
  for (size_t index = 0; index < sizeof(tails) / sizeof(tails[0]); ++index)
  {
    char input[2200];
    snprintf(input, sizeof(input), "%s\n%s\n", valid, tails[index]);
    if (!writeFile(path, input, strlen(input)) || !reject(path))
      return 0;
  }
  pass("extra data and incomplete EXT");
  if (!writeFile(path, valid, length + 1) || !reject(path))
    return 0;
  FILE *large = fopen(path, "wb");
  if (!large)
    return 0;
  for (int index = 0; index < 65536; ++index)
    fputc('9', large);
  fclose(large);
  if (!reject(path))
    return 0;
  pass("embedded NUL and file-size limit");
  srand(123);
  for (int sample = 0; sample < 100; ++sample)
  {
    FILE *file = fopen(path, "wb");
    if (!file)
      return 0;
    fputs("invalid ", file);
    for (int index = 0, count = rand() % 2048; index < count; ++index)
      fputc(rand() % 256, file);
    fclose(file);
    if (!reject(path))
      return 0;
    char input[2200];
    snprintf(input, sizeof(input), "%s\nEXT %d\n", valid, rand());
    if (!writeFile(path, input, strlen(input)) || !reject(path))
      return 0;
  }
  pass("200 generated malformed files in C");
  unlink(path);
  if (mkfifo(path, 0600) < 0 || !reject(path))
    return 0;
  unlink(path);
  if (mkdir(path, 0700) < 0 || !reject(path))
    return 0;
  rmdir(path);
  pass("FIFO and directory rejected as configuration");
  char *delay[] = {"./port_sim", "examples/complete.txt", "-1", NULL};
  char *session[] = {"./port_sim", "fork-named", "examples/complete.txt",
                     "0", "../bad", NULL};
  if (commandStatus(delay) != 2 || commandStatus(session) != 2)
    return 0;
  pass("invalid CLI arguments");
  if (mkfifo(path, 0600) < 0)
    return 0;
  char *log[] = {"./port_sim", "fork-named", "examples/complete.txt",
                 "0", "badlog", path,
                 NULL};
  int status = commandStatus(log);
  unlink(path);
  if (status != 3)
    return 0;
  pass("FIFO log rejected without blocking");
  return 1;
}

int main(void)
{
  SOFT_ASSERT(access("./port_sim", X_OK) == 0,
              "Run make test from the project root", 2);
  SOFT_ASSERT(mkdtemp(temporary) != NULL, "Cannot create input-test directory",
              2);
  const char *modes[] = {"fork-named", "fork-unnamed", "shm", "fifo"};
  const Fixture fixtures[] = {{"complete", 3},
                              {"loading_multi", 2},
                              {"shortest_first", 2},
                              {"zero_ships", 0},
                              {"warehouse_wait", 2},
                              {"time_limit", -1},
                              {"no_export", -1},
                              {"order_id", 3},
                              {"order_fcfs", 3},
                              {"zero_cargo", 1},
                              {"max_actors", -1},
                              {"late_arrival", -1},
                              {"compatibility", 1},
                              {"manual", 1},
                              {"loading_dependency", 2},
                              {"loading_dependency_reserved", 2},
                              {"loading_partial", 2},
                              {"loading_partial_reserved", 2},
                              {"unloading_dependency", 2},
                              {"unloading_dependency_reserved", 2}};
  for (int mode = 0; mode < 4; ++mode)
  {
    for (size_t fixture = 0; fixture < sizeof(fixtures) / sizeof(fixtures[0]);
         ++fixture)
      if (!runFixture(modes[mode], &fixtures[fixture]))
        goto failure;
    if (!runAction(modes[mode], "SIGINT") ||
        !runAction(modes[mode], "SIGTERM") ||
        !runAction(modes[mode], "reuse") || !runAction(modes[mode], "reuse"))
      goto failure;
  }
  for (int mode = 2; mode < 4; ++mode)
    if (!runAction(modes[mode], "rolecheck") ||
        !runAction(modes[mode], "crash") ||
        !runAction(modes[mode], "duplicate") ||
        !runAction(modes[mode], "stopped_shutdown"))
      goto failure;
  if (!runAction("shm", "stalled") || !testInput())
    goto failure;
  rmdir(temporary);
  printf("ALL %d GROUPS PASSED\n", passed);
  return 0;
failure:
  stopRun(&run);
  fprintf(stderr, "FAIL after %d groups; input files: %s\n%s\n", passed,
          temporary, run.text);
  return 1;
}
