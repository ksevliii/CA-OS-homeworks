#include "process.h"

int workerLoop(Transport *transport, int actor)
{
  SOFT_ASSERT(transport != NULL, "Transport is required", 3);
  SOFT_ASSERT(actor >= 0 && actor < transport->meta.count, "Invalid actor", 3);
  ActorState state = {.occupant = -1};

  Message message = {.action = HELLO};
  if (!putMessage(transport, actor, 1, message))
    return 3;
  while (!interrupted)
  {
    if (kill(transport->meta.pid, 0) < 0 && errno == ESRCH)
      return 3;
    int result = waitMessage(transport, actor, 0, &message);
    if (result < 0)
      return 3;
    if (!result)
    {
      continue;
    }
    if (message.action == STOP)
      return 0;
    if (!actorAccept(transport->meta.role[actor], &state, &message))
      return 3;
    int64_t left = (int64_t)message.duration * message.delay;
    while (left > 0 && !interrupted)
    {
      int step = left > 20 ? 20 : (int)left;
      pauseMs(step);
      left -= step;

      if (kill(transport->meta.pid, 0) < 0 && errno == ESRCH)
        return 3;
    }
    if (interrupted)
      break;
    actorComplete(transport->meta.role[actor], &state, &message);
    if (!putMessage(transport, actor, 1, message))
      return 3;
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
