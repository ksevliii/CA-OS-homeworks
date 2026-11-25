#include "network.h"

static void acceptPeers(Transport *transport)
{
  Network *network = transport->network;
  for (int attempt = 0; attempt < 8; ++attempt)
  {
    int descriptor = accept(network->listener, NULL, NULL);
    if (descriptor < 0)
    {
      if (errno != EAGAIN && errno != EWOULDBLOCK && errno != EINTR)
        network->failed = 1;
      return;
    }
    int slot = 0;
    while (slot < CONNECTIONS && network->peers[slot].fd >= 0)
      ++slot;
    if (slot == CONNECTIONS || !socketOptions(descriptor))
      close(descriptor);
    else
    {
      network->peers[slot].fd = descriptor;
      network->peers[slot].actor = -1;
    }
  }
}

static void servicePeer(Transport *transport, int index, short events)
{
  NetPeer *peer = &transport->network->peers[index];
  if (events & POLLIN)
  {
    int kind;
    size_t size;
    const unsigned char *data;
    int result = readFrame(peer, &kind, &data, &size);
    if (result < 0 || (result == 1 && !handleFrame(transport, index, kind, data, size)))
    {
      disconnectPeer(transport, index);
      return;
    }
  }
  if ((events & POLLOUT) && flushFrame(peer) < 0)
  {
    disconnectPeer(transport, index);
    return;
  }
  if (events & (POLLERR | POLLHUP | POLLNVAL))
    disconnectPeer(transport, index);
}

int networkPump(Transport *transport, int milliseconds)
{
  Network *network = transport->network;
  struct pollfd descriptors[CONNECTIONS + 1];
  descriptors[0] = (struct pollfd){.fd = network->closing ? -1 : network->listener,
                                   .events = POLLIN};
  for (int index = 0; index < transport->meta.count; ++index)
    sendPending(transport, index);
  for (int index = 0; index < CONNECTIONS; ++index)
  {
    NetPeer *peer = &network->peers[index];
    descriptors[index + 1] = (struct pollfd){.fd = peer->fd, .events = POLLIN};
    if (peer->queued)
      descriptors[index + 1].events |= POLLOUT;
  }
  int result = poll(descriptors, CONNECTIONS + 1, milliseconds);
  if (result < 0)
  {
    if (errno != EINTR)
      network->failed = 1;
    return !network->failed;
  }
  if (descriptors[0].revents & POLLIN)
    acceptPeers(transport);
  if (descriptors[0].revents & (POLLERR | POLLHUP | POLLNVAL))
    network->failed = 1;
  for (int index = 0; index < CONNECTIONS; ++index)
    if (descriptors[index + 1].fd >= 0 && descriptors[index + 1].revents)
      servicePeer(transport, index, descriptors[index + 1].revents);
  if (!network->closing)
    for (int index = 0; index < CONNECTIONS; ++index)
    {
      NetPeer *peer = &network->peers[index];
      if (peer->observer && !peer->queued && network->snapshotLength &&
          peer->snapshotSeen != network->snapshotRevision)
      {
        queueFrame(peer, SNAPSHOT, network->snapshot, (size_t)network->snapshotLength);
        peer->snapshotSeen = network->snapshotRevision;
      }
    }
  return !network->failed;
}

int putMessage(Transport *transport, int actor, int reply, Message message)
{
  SOFT_ASSERT(transport && transport->network, "TCP endpoint is required", 0);
  SOFT_ASSERT(actor >= 0 && actor < transport->meta.count, "Invalid actor", 0);
  if (!transport->owner)
  {
    unsigned char bytes[MESSAGE_SIZE];
    encodeMessage(&message, bytes);
    return reply && queueFrame(&transport->network->peers[0], RESPONSE, bytes, sizeof(bytes));
  }
  NetActor *slot = &transport->network->actors[actor];
  if (reply || slot->pending)
    return 0;
  slot->command = message;
  slot->pending = 1;
  slot->sent = 0;
  sendPending(transport, actor);
  return 1;
}

int getMessage(Transport *transport, int actor, int reply, Message *message)
{
  if (!transport->owner || !reply || actor < 0 || actor >= transport->meta.count)
    return -1;
  if (!networkPump(transport, 0))
    return -1;
  NetActor *slot = &transport->network->actors[actor];
  if (!slot->ready)
    return 0;
  *message = slot->response;
  slot->ready = slot->pending = slot->sent = 0;
  return 1;
}
