#include "network.h"

int workerLoop(Transport *transport, int actor)
{
  SOFT_ASSERT(transport && transport->network, "TCP endpoint is required", 3);
  SOFT_ASSERT(actor >= 0 && actor < transport->meta.count, "Invalid actor", 3);
  ActorState state = transport->network->restored;
  int role = transport->meta.role[actor];
  printf("Worker %d role=%d connected; last committed command=%d\n", actor, role, state.lastSeq);
  while (!interrupted)
  {
    int kind;
    size_t size;
    const unsigned char *data;
    int result = clientFrame(transport, &kind, &data, &size, 50);
    if (result < 0 || (result == 1 && kind == FINISH))
    {
      printf("Worker %d: server stopped or connection closed\n", actor);
      return 0;
    }
    if (!result)
      continue;
    Message message;
    if (kind != COMMAND || !decodeMessage(&message, data, size) ||
        !actorAccept(role, &state, &message))
      return 3;
    printf("Worker %d: command=%d ship=%d volume=%d seq=%d started\n",
           actor, message.action, message.ship, message.volume, message.seq);
    double deadline = monotonic() + (double)message.duration * message.delay / 1000.0;
    while (!interrupted && monotonic() < deadline)
    {
      result = clientFrame(transport, &kind, &data, &size, 20);
      if (result < 0 || (result == 1 && kind == FINISH))
        return 0;
      if (result == 1)
        return 3;
    }
    if (interrupted)
      break;
    actorComplete(role, &state, &message);
    if (!putMessage(transport, actor, 1, message))
      return 3;
    printf("Worker %d: seq=%d completed, reply queued\n", actor, message.seq);
  }
  return 0;
}

int actorAccept(int role, ActorState *state, const Message *message)
{
  SOFT_ASSERT(state && message, "Participant state is required", 0);
  SOFT_ASSERT(message->seq > state->lastSeq, "Repeated or unordered command",
              0);
  int maximumDuration =
      role == SHIP_ACTOR && message->action == ARRIVAL ? 1000000 : 12000;
  SOFT_ASSERT(message->duration >= 0 && message->duration <= maximumDuration,
              "Invalid operation duration", 0);
  SOFT_ASSERT(message->delay >= 0 && message->delay <= 1000,
              "Invalid display delay", 0);
  SOFT_ASSERT(message->volume >= 0 && message->volume <= 1000000,
              "Invalid cargo portion", 0);
  SOFT_ASSERT(message->ship >= -1 && message->ship < 32,
              "Invalid ship identifier", 0);
  int accepted = 0;
  switch (role)
  {
  case SHIP_ACTOR:
    accepted = (message->action == ARRIVAL && state->phase == 0) ||
               (message->action == RELEASE && state->phase == 1);
    break;
  case TUG_ACTOR:
    accepted = (message->action == ENTER || message->action == EXIT) &&
               message->ship >= 0 && message->volume == 0;
    break;
  case BERTH_ACTOR:
    accepted = (message->action == MOOR && state->occupant == -1) ||
               (message->action == RELEASE && state->occupant == message->ship);
    break;
  case CRANE_ACTOR:
    accepted =
        message->action == CARGO && message->ship >= 0 && message->volume > 0;
    break;
  case WAREHOUSE_ACTOR:
    accepted = message->action == DRAIN && message->ship == -1;
    for (int field = 0; field < 7; ++field)
      accepted = accepted && message->warehouseData[field] >= 0 &&
                 message->warehouseData[field] <= 1000000;
    accepted = accepted && message->warehouseData[0] +
                                   message->warehouseData[1] +
                                   message->warehouseData[3] <=
                               message->warehouseData[2];
    break;
  }
  SOFT_ASSERT(accepted, "Command violates the participant lifecycle", 0);
  state->lastSeq = message->seq;
  return 1;
}

static void warehouseOperation(Message *message);

void actorComplete(int role, ActorState *state, Message *message)
{
  if (role == WAREHOUSE_ACTOR)
    warehouseOperation(message);
  if (role == SHIP_ACTOR)
    ++state->phase;
  if (role == BERTH_ACTOR)
    state->occupant = message->action == MOOR ? message->ship : -1;
  if (role == CRANE_ACTOR)
    state->completedVolume += (unsigned long long)message->volume;
}

static void warehouseOperation(Message *message)
{
  int *data = message->warehouseData;
  int capacity = data[2], reserved = data[3], budget = data[4];
  int exported = 0, supplied = 0;
  for (int cargoType = 0; cargoType < 2; ++cargoType)
  {
    int portion = data[cargoType] < budget ? data[cargoType] : budget;
    data[cargoType] -= portion;
    budget -= portion;
    exported += portion;
  }
  for (int cargoType = 0; cargoType < 2; ++cargoType)
  {
    int available = capacity - reserved - data[0] - data[1];
    int portion =
        data[5 + cargoType] < available ? data[5 + cargoType] : available;
    data[cargoType] += portion;
    supplied += portion;
  }
  data[2] = exported;
  data[3] = supplied;
}
