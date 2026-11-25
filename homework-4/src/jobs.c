#include "process.h"

int startJob(Engine *engine, int actor, int action, int ship, int volume,
             int duration)
{
  if (actor < 0 || actor >= engine->ipc->meta.count ||
      engine->jobs[actor].seq)
  {
    engine->failed = 1;
    return 0;
  }
  Message message = {.seq = ++engine->seq,
                     .action = action,
                     .ship = ship,
                     .volume = volume,
                     .duration = duration,
                     .delay = engine->port->delayMs};
  if (action == DRAIN)
  {
    Warehouse *warehouse = &engine->port->res.warehouse;
    message.warehouseData[0] = warehouse->stock[0];
    message.warehouseData[1] = warehouse->stock[1];
    message.warehouseData[2] = warehouse->capacity;
    message.warehouseData[3] = warehouse->reserved;
    message.warehouseData[4] = engine->port->warehouseOut;
    message.warehouseData[5] = engine->port->supply[0];
    message.warehouseData[6] = engine->port->supply[1];
  }
  engine->jobs[actor] = (Job){.actor = actor,
                              .action = action,
                              .ship = ship,
                              .due = engine->port->currentTime + duration,
                              .seq = message.seq,
                              .volume = volume};
  if (!putMessage(engine->ipc, actor, 0, message))
  {
    engine->failed = 1;
    return 0;
  }
  return 1;
}

int randomDelay(Engine *engine, int base)
{
  engine->seed = engine->seed * 1664525u + 1013904223u;
  return base + (int)(engine->seed % (unsigned)(base / 5 + 1));
}

int selectTug(Engine *engine, Ship *ship)
{
  for (int index = 0; index < engine->port->res.tugCount; ++index)
    if (!engine->port->res.tugs[index].busy &&
        (engine->port->res.tugs[index].typeMask & (1 << ship->type)))
      return index;
  return -1;
}

void setWaiting(Engine *engine, Ship *ship, ShipState state, const char *what)
{
  ship->state = state;
  ship->waitSince = engine->port->currentTime;
  PORT_EVENT(engine, "Ship #%d requests %s", ship->id, what);
}

void assignTug(Engine *engine, Ship *ship, int tug, int action)
{
  Tug *assignedTug = &engine->port->res.tugs[tug];
  assignedTug->busy = true;
  assignedTug->shipId = ship->id;
  ship->tugId = tug;
  ship->totalWait += engine->port->currentTime - ship->waitSince;
  ship->state = action == ENTER ? ENTERING : EXITING;
  PORT_EVENT(engine, "Ship #%d: tug #%d assigned, %s", ship->id, tug,
             action == ENTER ? "entering" : "exiting");
  startJob(engine, engine->actorTug[tug], action, ship->id, 0,
           randomDelay(engine, engine->port->disp.enterExitTime));
}
