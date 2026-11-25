#include "network.h"
#include "ui.h"

void publishState(Engine *engine, const char *status)
{
  Network *network = engine->ipc->network;
  char *buffer = network->snapshot;
  int length = uiFormat(engine->port, buffer, FRAME_SIZE - 512);
  if (!length)
  {
    engine->failed = 1;
    return;
  }
  int connected = 0;
  for (int index = 0; index < engine->ipc->meta.count; ++index)
    connected += network->actors[index].peer >= 0;
  int extra = snprintf(buffer + length, FRAME_SIZE - (size_t)length,
                       "Server: %s | Workers: %d/%d | TCP %s:%d\n",
                       status, connected, engine->ipc->meta.count,
                       engine->ipc->address, engine->ipc->port);
  if (extra < 0 || (size_t)extra >= FRAME_SIZE - (size_t)length)
  {
    engine->failed = 1;
    return;
  }
  network->snapshotLength = length + extra;
  ++network->snapshotRevision;
  for (int index = 0; index < CONNECTIONS; ++index)
    if (network->peers[index].observer && !network->peers[index].queued)
    {
      queueFrame(&network->peers[index], SNAPSHOT, buffer, (size_t)network->snapshotLength);
      network->peers[index].snapshotSeen = network->snapshotRevision;
    }
}

int observerMain(int argc, char **argv)
{
  Transport transport;
  int result = clientArguments(argc, argv, &transport, 0);
  if (result != 1)
    return result == 0 ? 2 : 0;
  if (!connectClient(&transport, -1, -1))
  {
    fprintf(stderr, "Cannot connect observer\n");
    closeTransport(&transport);
    return 3;
  }
  result = 0;
  while (!interrupted)
  {
    int kind;
    const unsigned char *data;
    size_t size;
    int received = clientFrame(&transport, &kind, &data, &size, 100);
    if (received < 0 || (received == 1 && kind == FINISH))
      break;
    if (!received)
      continue;
    if (kind != SNAPSHOT || size >= FRAME_SIZE || memchr(data, 0, size))
    {
      result = 3;
      break;
    }
    char snapshot[FRAME_SIZE];
    memcpy(snapshot, data, size);
    snapshot[size] = 0;
    if (!uiRender(snapshot))
    {
      result = 3;
      break;
    }
  }
  closeTransport(&transport);
  return result;
}
