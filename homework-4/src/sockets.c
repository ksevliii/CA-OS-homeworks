#include "network.h"
#include <netinet/tcp.h>

void transportInit(Transport *transport, const char *address, int port)
{
  memset(transport, 0, sizeof(*transport));
  transport->actor = -1;
  transport->role = -1;
  transport->port = port;
  snprintf(transport->address, sizeof(transport->address), "%s", address);
}

int endpoint(const char *address, int port, struct sockaddr_in *socketAddress)
{
  *socketAddress = (struct sockaddr_in){.sin_family = AF_INET};
  if (port < 1 || port > 65535 || inet_pton(AF_INET, address, &socketAddress->sin_addr) != 1)
    return 0;
  socketAddress->sin_port = htons((uint16_t)port);
  return 1;
}

int socketOptions(int descriptor)
{
  int flags = fcntl(descriptor, F_GETFL, 0), enabled = 1;
  return flags >= 0 && fcntl(descriptor, F_SETFL, flags | O_NONBLOCK) == 0 &&
         fcntl(descriptor, F_SETFD, FD_CLOEXEC) == 0 &&
         setsockopt(descriptor, IPPROTO_TCP, TCP_NODELAY, &enabled, sizeof(enabled)) == 0;
}

int networkInit(Transport *transport)
{
  transport->network = calloc(1, sizeof(Network));
  SOFT_ASSERT(transport->network != NULL, "Cannot allocate TCP buffers", 0);
  Network *network = transport->network;
  network->listener = -1;
  for (int index = 0; index < CONNECTIONS; ++index)
    network->peers[index].fd = network->peers[index].actor = -1;
  for (int index = 0; index < ACTORS; ++index)
  {
    network->actors[index].peer = -1;
    network->actors[index].state.occupant = -1;
  }
  return 1;
}

int createTransport(Transport *transport)
{
  struct sockaddr_in address;
  if (!endpoint(transport->address, transport->port, &address) || !networkInit(transport))
    return 0;
  transport->owner = 1;
  int descriptor = socket(AF_INET, SOCK_STREAM, 0), enabled = 1;
  transport->network->listener = descriptor;
  return descriptor >= 0 && socketOptions(descriptor) &&
         setsockopt(descriptor, SOL_SOCKET, SO_REUSEADDR, &enabled, sizeof(enabled)) == 0 &&
         bind(descriptor, (struct sockaddr *)&address, sizeof(address)) == 0 &&
         listen(descriptor, CONNECTIONS) == 0;
}

void closeTransport(Transport *transport)
{
  if (!transport || !transport->network)
    return;
  Network *network = transport->network;
  if (network->listener >= 0)
    close(network->listener);
  for (int index = 0; index < CONNECTIONS; ++index)
    if (network->peers[index].fd >= 0)
      close(network->peers[index].fd);
  free(network);
  transport->network = NULL;
}

void disconnectPeer(Transport *transport, int index)
{
  NetPeer *peer = &transport->network->peers[index];
  if (peer->fd < 0)
    return;
  close(peer->fd);
  if (peer->actor >= 0)
  {
    NetActor *actor = &transport->network->actors[peer->actor];
    actor->peer = -1;
    actor->sent = 0;
    fprintf(stdout, "Worker %d disconnected; saved work retained\n", peer->actor);
  }
  else if (peer->observer)
    fprintf(stdout, "Observer disconnected\n");
  memset(peer, 0, sizeof(*peer));
  peer->fd = peer->actor = -1;
}
