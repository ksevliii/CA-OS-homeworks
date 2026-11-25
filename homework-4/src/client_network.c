#include "network.h"

int clientFrame(Transport *transport, int *kind, const unsigned char **data,
                size_t *size, int milliseconds)
{
  NetPeer *peer = &transport->network->peers[0];
  struct pollfd descriptor = {.fd = peer->fd, .events = POLLIN};
  if (peer->queued)
    descriptor.events |= POLLOUT;
  int result = poll(&descriptor, 1, milliseconds);
  if (result < 0)
    return errno == EINTR ? 0 : -1;
  if ((descriptor.revents & POLLOUT) && flushFrame(peer) < 0)
    return -1;
  if (descriptor.revents & POLLIN)
    return readFrame(peer, kind, data, size);
  if (descriptor.revents & (POLLERR | POLLHUP | POLLNVAL))
    return -1;
  return 0;
}

int connectClient(Transport *transport, int actor, int role)
{
  struct sockaddr_in address;
  if (!endpoint(transport->address, transport->port, &address) || !networkInit(transport))
    return 0;
  NetPeer *peer = &transport->network->peers[0];
  peer->fd = socket(AF_INET, SOCK_STREAM, 0);
  if (peer->fd < 0 || !socketOptions(peer->fd))
    return 0;
  int result = connect(peer->fd, (struct sockaddr *)&address, sizeof(address));
  if (result < 0 && errno != EINPROGRESS)
    return 0;
  struct pollfd descriptor = {.fd = peer->fd, .events = POLLOUT};
  int error = 0;
  socklen_t errorSize = sizeof(error);
  if (poll(&descriptor, 1, 3000) <= 0 ||
      getsockopt(peer->fd, SOL_SOCKET, SO_ERROR, &error, &errorSize) < 0 || error)
    return 0;
  Message registration = {.action = HELLO, .ship = actor, .volume = role};
  unsigned char bytes[MESSAGE_SIZE];
  encodeMessage(&registration, bytes);
  queueFrame(peer, REGISTER, bytes, sizeof(bytes));
  double deadline = monotonic() + 5;
  while (!interrupted && monotonic() < deadline)
  {
    int kind;
    size_t size;
    const unsigned char *data;
    result = clientFrame(transport, &kind, &data, &size, 50);
    if (result < 0)
      return 0;
    if (result == 1)
    {
      Message welcome;
      if (kind != WELCOME || !decodeMessage(&welcome, data, size) ||
          welcome.ship != actor || welcome.duration < 1 || welcome.duration > ACTORS ||
          (actor >= 0 && welcome.volume != role))
        return 0;
      transport->actor = actor;
      transport->role = role;
      transport->meta.count = welcome.duration;
      if (actor >= 0)
        transport->meta.role[actor] = role;
      transport->network->restored = (ActorState){.phase = welcome.warehouseData[0],
                                                  .occupant = welcome.warehouseData[1],
                                                  .lastSeq = welcome.warehouseData[2]};
      return 1;
    }
  }
  return 0;
}

int attachTransport(Transport *transport, int actor)
{
  return connectClient(transport, actor, transport->role);
}
