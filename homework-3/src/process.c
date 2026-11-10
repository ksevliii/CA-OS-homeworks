#include "process.h"
#include "ui.h"

static int expectedRole = -1;

static void usage(void)
{
  const char *text =
      "Port simulation (C11 / POSIX, Linux)\n"
      "  ./port_sim fork-named CONFIG [DELAY_MS [SESSION [LOG]]]\n"
      "  ./port_sim fork-unnamed CONFIG [DELAY_MS [SESSION [LOG]]]\n"
      "  ./port_sim shm CONFIG [DELAY_MS [SESSION [LOG]]]\n"
      "  ./port_sim fifo CONFIG [DELAY_MS [SESSION [LOG]]]\n"
      "  ./port_sim worker SESSION ACTOR_INDEX\n"
      "  ./port_sim CONFIG [DELAY_MS]   (fork-named compatibility syntax)\n"
      "Independent shm/fifo: start the server first, then every worker "
      "printed\n"
      "in READY. All workers must connect within 120 seconds. DELAY_MS: "
      "0..1000.\n"
      "maxTime=0: run until all ships depart or SIGINT/SIGTERM.\n";
  fullWrite(STDOUT_FILENO, text, strlen(text));
}

int processMain(int argc, char **argv)
{
  interrupted = 0;
  struct sigaction action;
  memset(&action, 0, sizeof(action));
  action.sa_handler = onSigint;
  sigemptyset(&action.sa_mask);
  sigaction(SIGINT, &action, NULL);
  sigaction(SIGTERM, &action, NULL);
  signal(SIGPIPE, SIG_IGN);
  if (argc == 2 && (!strcmp(argv[1], "--help") || !strcmp(argv[1], "-h")))
  {
    usage();
    return 0;
  }
  if (argc > 1 && !strcmp(argv[1], "worker"))
  {
    int actor;
    if (argc != 4 || !validSession(argv[2]) ||
        !number(argv[3], ACTORS - 1, &actor))
    {
      usage();
      return 2;
    }
    Transport transport;
    transportInit(&transport, argv[2], 2);
    if (!attachTransport(&transport, actor))
    {
      perror("Cannot connect worker");
      closeTransport(&transport);
      return 3;
    }
    if (expectedRole >= 0 && transport.meta.role[actor] != expectedRole)
    {
      fprintf(stderr,
              "Participant executable does not match the session role\n");
      closeTransport(&transport);
      return 2;
    }
    int result = workerLoop(&transport, actor);
    closeTransport(&transport);
    return result;
  }
  int mode = 0, offset = 1;
  const char *modes[] = {"fork-named", "fork-unnamed", "shm", "fifo"};
  for (int index = 0; index < 4; ++index)
    if (argc > 1 && !strcmp(argv[1], modes[index]))
    {
      mode = index;
      offset = 2;
      break;
    }
  if (argc <= offset || argc > offset + 4)
  {
    usage();
    return 2;
  }
  int delay = 100;
  if (argc > offset + 1 && !number(argv[offset + 1], 1000, &delay))
  {
    fprintf(stderr, "Invalid delay\n");
    return 2;
  }
  char generated[25];
  snprintf(generated, sizeof(generated), "run_%ld", (long)getpid());
  const char *session = argc > offset + 2 ? argv[offset + 2] : generated;
  const char *logPath = argc > offset + 3 ? argv[offset + 3] : "port.log";
  if (!validSession(session))
  {
    fprintf(stderr, "Invalid session identifier\n");
    return 2;
  }
  Port port;
  portInit(&port);
  if (!configLoad(&port, argv[offset]))
  {
    fprintf(stderr,
            "Invalid configuration: %s (see README for bounds and format)\n",
            argv[offset]);
    portFree(&port);
    return 2;
  }
  port.delayMs = delay;
  Logger log;
  if (!loggerOpen(&log, logPath))
  {
    portFree(&port);
    return 3;
  }
  port.log = &log;
  Transport transport;
  transportInit(&transport, session, mode);
  Engine engine;
  memset(&engine, 0, sizeof(engine));
  engine.port = &port;
  engine.ipc = &transport;
  engine.seed = port.disp.seed;
  buildActors(&engine);
  if (!createTransport(&transport))
  {
    perror("IPC initialization failed");
    closeTransport(&transport);
    loggerClose(&log);
    portFree(&port);
    return 3;
  }
  PORT_EVENT(
      &engine,
      "IPC=%s session=%s actors=%d dispatcher_pid=%ld seed=%u strategy=%d "
      "reservation=%d",
      modes[mode], session, transport.meta.count, (long)getpid(), engine.seed,
      port.strategy, port.reservation);
  int ok = 1;
  if (mode < 2)
  {
    for (int index = 0; index < transport.meta.count; ++index)
    {
      pid_t pid = fork();
      if (pid < 0)
      {
        perror("fork");
        ok = 0;
        break;
      }
      if (!pid)
      {
        transport.owner = 0;
        transport.childCount = 0;
        int result = workerLoop(&transport, index);
        closeTransport(&transport);
        _exit(result);
      }
      transport.children[transport.childCount++] = pid;
    }
  }
  else
  {
    PORT_EVENT(
        &engine,
        "READY workers=%d; launch: ./port_sim worker %s INDEX (INDEX=0..%d)",
        transport.meta.count, session, transport.meta.count - 1);
    static const char *roles[] = {"ship", "tug", "berth", "crane", "warehouse"};
    for (int index = 0; index < transport.meta.count; ++index)
      PORT_EVENT(&engine, "ACTOR index=%d role=%s id=%d", index,
                 roles[transport.meta.role[index]], transport.meta.id[index]);
  }
  if (ok)
    ok = waitReady(&transport);
  int result = 3;
  if (ok && !interrupted)
    result = runModel(&engine);
  else
  {
    PORT_EVENT(&engine,
               "Startup cancelled: signal, missing worker, or IPC failure");
    summary(&engine, "startup incomplete");
  }
  shutdownActors(&transport);
  closeTransport(&transport);
  loggerClose(&log);
  portFree(&port);
  return result;
}

int processRoleMain(int argc, char **argv, int role)
{
  if (argc == 2 && (!strcmp(argv[1], "--help") || !strcmp(argv[1], "-h")))
  {
    fprintf(stdout, "%s SESSION ACTOR_INDEX\n", argv[0]);
    return 0;
  }
  if (argc != 3)
  {
    fprintf(stderr, "Usage: %s SESSION ACTOR_INDEX\n", argv[0]);
    return 2;
  }
  expectedRole = role;
  char *arguments[] = {argv[0], "worker", argv[1], argv[2], NULL};
  return processMain(4, arguments);
}

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

void event(Engine *engine, const char *text)
{
  char line[600];
  int length = snprintf(line, sizeof(line), "[T=%06d] %s\n",
                        engine->port->currentTime, text);
  if (length < 0 || (size_t)length >= sizeof(line))
  {
    engine->failed = 1;
    return;
  }
  uiAddEvent(engine->port, line);
  if ((!engine->port->uiActive &&
       !fullWrite(STDOUT_FILENO, line, (size_t)length)) ||
      (engine->port->log && !loggerWrite(engine->port->log, line, length)))
    engine->failed = 1;
}

int validSession(const char *session)
{
  if (!session || !*session || strlen(session) > 24)
    return 0;
  for (; *session; ++session)
    if (!isalnum((unsigned char)*session) && *session != '_')
      return 0;
  return 1;
}
