#include "network.h"

static int registerPeer(Transport *transport, int index, const Message *message)
{
  Network *network = transport->network;
  NetPeer *peer = &network->peers[index];
  if (peer->actor >= 0 || peer->observer || network->closing)
    return 0;
  int actorIndex = message->ship;
  Message welcome = {.action = HELLO, .ship = actorIndex, .duration = transport->meta.count};
  if (actorIndex == -1 && message->volume == -1)
  {
    int observers = 0;
    for (int slot = 0; slot < CONNECTIONS; ++slot)
      observers += network->peers[slot].observer;
    if (observers >= OBSERVERS)
      return 0;
    peer->observer = 1;
    fprintf(stdout, "Observer connected\n");
  }
  else
  {
    if (actorIndex < 0 || actorIndex >= transport->meta.count ||
        message->volume != transport->meta.role[actorIndex] ||
        network->actors[actorIndex].peer >= 0)
    {
      fprintf(stdout, "Registration rejected: wrong role, index or duplicate\n");
      return 0;
    }
    NetActor *actor = &network->actors[actorIndex];
    peer->actor = actorIndex;
    actor->peer = index;
    welcome.volume = transport->meta.role[actorIndex];
    welcome.warehouseData[0] = actor->state.phase;
    welcome.warehouseData[1] = actor->state.occupant;
    welcome.warehouseData[2] = actor->state.lastSeq;
    fprintf(stdout, "Worker %d connected role=%d; state restored\n", actorIndex, welcome.volume);
  }
  unsigned char bytes[MESSAGE_SIZE];
  encodeMessage(&welcome, bytes);
  return queueFrame(peer, WELCOME, bytes, sizeof(bytes));
}

static int acceptResponse(Transport *transport, NetPeer *peer, const Message *message)
{
  if (peer->actor < 0)
    return 0;
  NetActor *actor = &transport->network->actors[peer->actor];
  Message *command = &actor->command;
  if (!actor->pending || actor->ready || message->seq != command->seq ||
      message->action != command->action || message->ship != command->ship ||
      message->volume != command->volume || message->duration != command->duration ||
      message->delay != command->delay)
    return 0;
  int role = transport->meta.role[peer->actor];
  Message expected = *command;
  ActorState expectedState = actor->state;
  actorComplete(role, &expectedState, &expected);
  if (memcmp(expected.warehouseData, message->warehouseData,
             sizeof(expected.warehouseData)) != 0)
    return 0;
  actor->response = *message;
  actor->ready = 1;
  actor->state.lastSeq = message->seq;
  if (role == SHIP_ACTOR)
    ++actor->state.phase;
  if (role == BERTH_ACTOR)
    actor->state.occupant = message->action == MOOR ? message->ship : -1;
  return 1;
}

int handleFrame(Transport *transport, int index, int kind,
                const unsigned char *data, size_t size)
{
  Message message;
  if (!decodeMessage(&message, data, size))
    return 0;
  if (kind == REGISTER)
    return registerPeer(transport, index, &message);
  if (kind == RESPONSE)
    return acceptResponse(transport, &transport->network->peers[index], &message);
  return 0;
}

void sendPending(Transport *transport, int index)
{
  NetActor *actor = &transport->network->actors[index];
  if (!actor->pending || actor->ready || actor->sent || actor->peer < 0)
    return;
  unsigned char bytes[MESSAGE_SIZE];
  encodeMessage(&actor->command, bytes);
  if (queueFrame(&transport->network->peers[actor->peer], COMMAND, bytes, sizeof(bytes)))
    actor->sent = 1;
}

int allConnected(Transport *transport)
{
  for (int index = 0; index < transport->meta.count; ++index)
    if (transport->network->actors[index].peer < 0)
      return 0;
  return 1;
}

int peersAlive(Transport *transport)
{
  return transport->network && !transport->network->failed;
}
