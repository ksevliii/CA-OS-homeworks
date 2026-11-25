#include "network.h"

static void usage(void)
{
  const char *text =
      "TCP port simulation (Linux / C11)\n"
      "  ./port_server BIND_IP PORT CONFIG [DELAY_MS [LOG]]\n"
      "  ./port_dispatcher BIND_IP PORT CONFIG [DELAY_MS [LOG]]\n"
      "  ./port_ship SERVER_IP PORT ACTOR_INDEX\n"
      "  ./port_tug SERVER_IP PORT ACTOR_INDEX\n"
      "  ./port_berth SERVER_IP PORT ACTOR_INDEX\n"
      "  ./port_crane SERVER_IP PORT ACTOR_INDEX\n"
      "  ./port_warehouse SERVER_IP PORT ACTOR_INDEX\n"
      "  ./port_observer SERVER_IP PORT\n"
      "DELAY_MS: 0..1000; maxTime=0: unlimited; Ctrl+C stops this program.\n";
  fullWrite(STDOUT_FILENO, text, strlen(text));
}

static void installSignals(void)
{
  interrupted = 0;
  struct sigaction action;
  memset(&action, 0, sizeof(action));
  action.sa_handler = onSigint;
  sigemptyset(&action.sa_mask);
  sigaction(SIGINT, &action, NULL);
  sigaction(SIGTERM, &action, NULL);
  signal(SIGPIPE, SIG_IGN);
  setvbuf(stdout, NULL, _IONBF, 0);
}

int clientArguments(int argc, char **argv, Transport *transport, int hasActor)
{
  installSignals();
  if (argc == 2 && (!strcmp(argv[1], "--help") || !strcmp(argv[1], "-h")))
  {
    usage();
    return -1;
  }
  int port;
  struct sockaddr_in address;
  if (argc != (hasActor ? 4 : 3) || !number(argv[2], 65535, &port) ||
      !endpoint(argv[1], port, &address))
  {
    usage();
    return 0;
  }
  transportInit(transport, argv[1], port);
  return 1;
}

int processRoleMain(int argc, char **argv, int role)
{
  Transport transport;
  int parsed = clientArguments(argc, argv, &transport, 1);
  if (parsed != 1)
    return parsed == 0 ? 2 : 0;
  int actor;
  if (!number(argv[3], ACTORS - 1, &actor))
    return 2;
  transport.role = role;
  if (!attachTransport(&transport, actor))
  {
    fprintf(stderr, "Cannot connect worker: endpoint, role or index rejected\n");
    closeTransport(&transport);
    return 3;
  }
  int result = workerLoop(&transport, actor);
  closeTransport(&transport);
  return result;
}

int processMain(int argc, char **argv)
{
  installSignals();
  if (argc == 2 && (!strcmp(argv[1], "--help") || !strcmp(argv[1], "-h")))
  {
    usage();
    return 0;
  }
  int portNumber, delay = 100;
  struct sockaddr_in address;
  if (argc < 4 || argc > 6 || !number(argv[2], 65535, &portNumber) ||
      !endpoint(argv[1], portNumber, &address) ||
      (argc > 4 && !number(argv[4], 1000, &delay)))
  {
    usage();
    return 2;
  }
  Port port;
  portInit(&port);
  if (!configLoad(&port, argv[3]))
  {
    portFree(&port);
    return 2;
  }
  port.delayMs = delay;
  Logger log;
  if (!loggerOpen(&log, argc > 5 ? argv[5] : "port.log"))
  {
    portFree(&port);
    return 3;
  }
  port.log = &log;
  Transport transport;
  transportInit(&transport, argv[1], portNumber);
  Engine engine = {.port = &port, .ipc = &transport, .seed = port.disp.seed};
  buildActors(&engine);
  if (!createTransport(&transport))
  {
    perror("Cannot listen on TCP endpoint");
    closeTransport(&transport);
    loggerClose(&log);
    portFree(&port);
    return 3;
  }
  transport.network->engine = &engine;
  PORT_EVENT(&engine, "READY TCP=%s:%d workers=%d seed=%u strategy=%d reservation=%d",
             argv[1], portNumber, transport.meta.count, engine.seed, port.strategy, port.reservation);
  const char *roles[] = {"ship", "tug", "berth", "crane", "warehouse"};
  for (int index = 0; index < transport.meta.count; ++index)
    PORT_EVENT(&engine, "ACTOR index=%d role=%s id=%d", index,
               roles[transport.meta.role[index]], transport.meta.id[index]);
  publishState(&engine, "WAITING FOR WORKERS");
  int result = 3;
  if (waitReady(&transport) && !interrupted)
    result = runModel(&engine);
  else
    summary(&engine, interrupted ? "Interrupted by user signal" : "startup failure");
  publishState(&engine, "FINISHED");
  shutdownActors(&transport);
  closeTransport(&transport);
  loggerClose(&log);
  portFree(&port);
  return result;
}
