#include "network.h"

static int drainOutput(Transport *transport)
{
  double deadline = monotonic() + 1;
  do
  {
    int queued = 0;
    for (int index = 0; index < CONNECTIONS; ++index)
      queued |= transport->network->peers[index].queued != 0;
    if (!queued)
      return 1;
    networkPump(transport, 10);
  } while (monotonic() < deadline);
  return 0;
}

void shutdownActors(Transport *transport)
{
  if (!transport->network)
    return;
  Network *network = transport->network;
  network->closing = 1;
  for (int index = 0; index < transport->meta.count; ++index)
    network->actors[index].sent = 1;
  drainOutput(transport);
  for (int index = 0; index < CONNECTIONS; ++index)
  {
    NetPeer *peer = &network->peers[index];
    if (peer->fd < 0)
      continue;
    if (peer->queued)
    {
      disconnectPeer(transport, index);
      continue;
    }
    if (peer->observer && network->snapshotLength)
      queueFrame(peer, SNAPSHOT, network->snapshot, (size_t)network->snapshotLength);
  }
  drainOutput(transport);
  for (int index = 0; index < CONNECTIONS; ++index)
  {
    NetPeer *peer = &network->peers[index];
    if (peer->fd >= 0 && !peer->queued)
      queueFrame(peer, FINISH, NULL, 0);
  }
  drainOutput(transport);
}
