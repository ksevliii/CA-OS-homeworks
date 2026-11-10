#include "process.h"

void dispInit(Dispatcher *disp, Resources *res, int enterTime, int unloadTime,
              unsigned int seed)
{
  if (disp == NULL)
  {
    fprintf(stderr, "failed: disp != NULL\n");
    perror("Dispatcher pointer is NULL");
    return;
  }
  if (res == NULL)
  {
    fprintf(stderr, "failed: res != NULL\n");
    perror("Resources pointer is NULL");
    return;
  }
  *disp = (Dispatcher){.res = res,
                       .enterExitTime = enterTime,
                       .unloadTime = unloadTime,
                       .seed = seed};
}

int checkInvariants(Engine *engine)
{
  SOFT_ASSERT(engine != NULL && engine->port != NULL,
              "Engine and port are required", 0);
  Port *port = engine->port;
  Warehouse *warehouse = &port->res.warehouse;
  if (warehouse->used < 0 || warehouse->reserved < 0 ||
      warehouse->used > warehouse->capacity - warehouse->reserved ||
      warehouse->stock[0] < 0 || warehouse->stock[1] < 0 ||
      warehouse->used != warehouse->stock[0] + warehouse->stock[1])
    return 0;
  for (int index = 0; index < port->res.berthCount; ++index)
  {
    int ship = port->res.berths[index].shipId;
    if (ship >= 0 &&
        (ship >= port->shipCount || port->ships[ship].berthId != index ||
         !port->res.berths[index].compatibility[port->ships[ship].type]))
      return 0;
  }
  for (int index = 0; index < port->res.tugCount; ++index)
  {
    Tug *tug = &port->res.tugs[index];
    int ship = tug->shipId;
    if (tug->busy != (ship >= 0))
      return 0;
    if (tug->busy && (ship < 0 || ship >= port->shipCount ||
                      port->ships[ship].tugId != index ||
                      !(tug->typeMask & (1 << port->ships[ship].type))))
      return 0;
  }
  for (int index = 0; index < port->res.craneCount; ++index)
  {
    Crane *crane = &port->res.cranes[index];
    int ship = crane->shipId;
    if (crane->busy != (ship >= 0))
      return 0;
    if (crane->busy && (ship < 0 || ship >= port->shipCount ||
                        port->ships[ship].state != UNLOADING ||
                        !crane->compatibility[port->ships[ship].cargoType]))
      return 0;
  }
  int reservedTotal = 0;
  for (int index = 0; index < port->shipCount; ++index)
  {
    Ship *ship = &port->ships[index];
    if (ship->berthId >= 0 && (ship->berthId >= port->res.berthCount ||
                               port->res.berths[ship->berthId].shipId != index))
      return 0;
    if (ship->tugId >= 0 && (ship->tugId >= port->res.tugCount ||
                             port->res.tugs[ship->tugId].shipId != index))
      return 0;
    if (ship->state == UNLOADING)
    {
      if (!ship->loading)
        reservedTotal += engine->portions[index];
      for (int craneIndex = 0; craneIndex < ship->cranesNeeded; ++craneIndex)
      {
        int craneId = engine->assigned[index][craneIndex];
        if (craneId < 0 || craneId >= port->res.craneCount ||
            port->res.cranes[craneId].shipId != index)
          return 0;
        for (int previous = 0; previous < craneIndex; ++previous)
          if (engine->assigned[index][previous] == craneId)
            return 0;
      }
    }
    if (ship->state == WAITING_WAREHOUSE &&
        (ship->berthId != -1 || ship->tugId != -1 || ship->craneId != -1))
      return 0;
    if (ship->state == UNMOORING &&
        (ship->berthId < 0 || ship->tugId != -1 || ship->craneId != -1))
      return 0;
    if (ship->cargoVolume < 0 || ship->cargoVolume > ship->startVolume)
      return 0;
    if (ship->state == DEPARTED && (ship->cargoVolume || ship->berthId != -1 ||
                                    ship->tugId != -1 || ship->craneId != -1))
      return 0;
  }
  SOFT_ASSERT(reservedTotal == warehouse->reserved,
              "Warehouse reservations must equal unfinished unloading portions",
              0);
  return 1;
}

static int warehouseAvailable(Engine *engine, Ship *ship)
{
  Warehouse *warehouse = &engine->port->res.warehouse;
  if (!(warehouse->cargoMask & (1 << ship->cargoType)))
    return 0;
  return ship->loading
             ? warehouse->stock[ship->cargoType]
             : warehouse->capacity - warehouse->used - warehouse->reserved;
}

static void waitForWarehouse(Engine *engine, Ship *ship)
{
  if (!ship->warehouseWait)
  {
    ship->warehouseWait = true;
    PORT_EVENT(
        engine, "Ship #%d: waiting warehouse, %s suspended, remaining=%d",
        ship->id, ship->loading ? "loading" : "unloading", ship->cargoVolume);
  }
}

static int dispatchShip(Engine *engine, Ship *ship)
{
  Port *port = engine->port;
  switch (ship->state)
  {
  case QUEUED:
  {
    int tug = selectTug(engine, ship);
    if (tug < 0)
      return 0;
    if (port->reservation)
    {
      if (ship->cargoVolume && !warehouseAvailable(engine, ship))
      {
        waitForWarehouse(engine, ship);
        return 0;
      }
      int berthId = takeBerth(&port->res, ship->type, ship->id);
      if (berthId < 0)
        return 0;
      ship->berthId = berthId;
      PORT_EVENT(engine, "Ship #%d reserves berth #%d together with entry tug",
                 ship->id, berthId);
    }
    assignTug(engine, ship, tug, ENTER);
    return 1;
  }
  case WAITING_WAREHOUSE:
    if (!warehouseAvailable(engine, ship))
      return 0;
    ship->totalWait += port->currentTime - ship->waitSince;
    setWaiting(engine, ship, WAITING_BERTH,
               "compatible berth after warehouse became available");
    /* fall through */
  case WAITING_BERTH:
  {
    if (ship->cargoVolume && !warehouseAvailable(engine, ship))
    {
      waitForWarehouse(engine, ship);
      if (ship->berthId >= 0)
      {
        PORT_EVENT(
            engine,
            "Ship #%d: berth #%d reservation cancelled while waiting warehouse",
            ship->id, ship->berthId);
        freeBerth(&port->res, ship->berthId);
        ship->berthId = -1;
      }
      ship->totalWait += port->currentTime - ship->waitSince;
      setWaiting(engine, ship, WAITING_WAREHOUSE,
                 "warehouse without holding a berth");
      return 1;
    }
    int berthId = ship->berthId;
    if (berthId < 0)
      berthId = takeBerth(&port->res, ship->type, ship->id);
    if (berthId < 0)
      return 0;
    ship->berthId = berthId;
    ship->state = MOORED;
    ship->totalWait += port->currentTime - ship->waitSince;
    PORT_EVENT(engine, "Ship #%d: berth #%d assigned, mooring started",
               ship->id, berthId);
    startJob(engine, engine->actorBerth[berthId], MOOR, ship->id, 0,
             port->moorTime);
    return 1;
  }
  case WAITING_CRANE_AND_WAREHOUSE:
  {
    if (!ship->cargoVolume)
    {
      setWaiting(engine, ship, WAITING_DEPART, "departure tug");
      return 1;
    }
    int ids[16], count = 0, rate = 0;
    for (int index = 0;
         index < port->res.craneCount && count < ship->cranesNeeded; ++index)
    {
      Crane *crane = &port->res.cranes[index];
      if (!crane->busy && crane->compatibility[ship->cargoType])
      {
        ids[count++] = index;
        rate += crane->rate;
      }
    }
    Warehouse *warehouse = &port->res.warehouse;
    if (!(warehouse->cargoMask & (1 << ship->cargoType)))
      return 0;
    int available = ship->loading ? warehouse->stock[ship->cargoType]
                                  : warehouse->capacity - warehouse->used -
                                        warehouse->reserved;
    if (!available)
    {
      waitForWarehouse(engine, ship);
      ship->totalWait += port->currentTime - ship->waitSince;
      ship->state = UNMOORING;
      PORT_EVENT(
          engine,
          "Ship #%d: unmooring to the port waiting area to release berth #%d",
          ship->id, ship->berthId);
      startJob(engine, engine->actorBerth[ship->berthId], RELEASE, ship->id, 0,
               port->moorTime);
      return 1;
    }
    if (count < ship->cranesNeeded)
      return 0;
    int volume = ship->cargoVolume;
    if (volume > available)
      volume = available;
    if (volume > rate)
      volume = rate;
    if (ship->warehouseWait)
    {
      ship->warehouseWait = false;
      PORT_EVENT(engine,
                 "Ship #%d: warehouse available, cargo operation resumed",
                 ship->id);
    }
    if (ship->loading)
    {
      warehouse->stock[ship->cargoType] -= volume;
      warehouse->used -= volume;
    }
    else
      warehouse->reserved += volume;
    ship->state = UNLOADING;
    ship->totalWait += port->currentTime - ship->waitSince;
    ship->craneId = ids[0];
    engine->remaining[ship->id] = count;
    engine->portions[ship->id] = volume;
    int duration = randomDelay(engine, port->disp.unloadTime);
    PORT_EVENT(
        engine,
        "Ship #%d: %s started, portion=%d cranes=%d warehouse=%d reserved=%d "
        "capacity=%d",
        ship->id, ship->loading ? "loading" : "unloading", volume, count,
        warehouse->used, warehouse->reserved, warehouse->capacity);
    for (int craneIndex = 0; craneIndex < count; ++craneIndex)
    {
      int id = ids[craneIndex];
      engine->assigned[ship->id][craneIndex] = id;
      port->res.cranes[id].busy = true;
      port->res.cranes[id].shipId = ship->id;
      PORT_EVENT(engine,
                 "Ship #%d: compatible crane #%d assigned, cargo operation in "
                 "progress",
                 ship->id, id);
      startJob(engine, engine->actorCrane[id], CARGO, ship->id, volume,
               duration);
    }
    return 1;
  }
  case WAITING_DEPART:
  {
    int tug = selectTug(engine, ship);
    if (tug < 0 || ship->cargoVolume || ship->craneId != -1)
      return 0;
    assignTug(engine, ship, tug, EXIT);
    return 1;
  }
  default:
    return 0;
  }
}

static int before(Engine *engine, int first, int second)
{
  Ship *firstShip = &engine->port->ships[first],
       *secondShip = &engine->port->ships[second];
  if (engine->port->strategy == 1 &&
      firstShip->arrivalTime != secondShip->arrivalTime)
    return firstShip->arrivalTime < secondShip->arrivalTime;
  if (engine->port->strategy == 2 &&
      firstShip->cargoVolume != secondShip->cargoVolume)
    return firstShip->cargoVolume < secondShip->cargoVolume;
  return first < second;
}

int dispatch(Engine *engine)
{
  if (engine->jobs[engine->actorWarehouse].seq)
    return 0;
  int order[32], changed = 0;
  for (int index = 0; index < engine->port->shipCount; ++index)
  {
    int position = index;
    order[index] = index;
    while (position > 0 &&
           before(engine, order[position], order[position - 1]))
    {
      int previous = order[position];
      order[position] = order[position - 1];
      order[position - 1] = previous;
      --position;
    }
  }
  for (int index = 0; index < engine->port->shipCount; ++index)
    changed |= dispatchShip(engine, &engine->port->ships[order[index]]);
  return changed;
}

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

void finishJob(Engine *engine, Job job)
{
  Port *port = engine->port;
  Ship *ship = job.ship >= 0 ? &port->ships[job.ship] : NULL;
  switch (job.action)
  {
  case ARRIVAL:
    statsRecordArrival(&port->stats);
    PORT_EVENT(
        engine,
        "Ship #%d arrived: type=%d cargo=%d volume=%d operation=%s cranes=%d",
        ship->id, ship->type, ship->cargoType, ship->cargoVolume,
        ship->loading ? "load" : "unload", ship->cranesNeeded);
    setWaiting(engine, ship, QUEUED, "compatible entry tug");
    break;
  case ENTER:
    freeTug(&port->res, ship->tugId);
    port->res.tugs[ship->tugId].shipId = -1;
    PORT_EVENT(engine, "Tug #%d released: ship #%d entered", ship->tugId,
               ship->id);
    ship->tugId = -1;
    setWaiting(engine, ship, WAITING_BERTH, "compatible berth and mooring");
    break;
  case MOOR:
    PORT_EVENT(engine, "Ship #%d moored at berth #%d", ship->id, ship->berthId);
    setWaiting(engine, ship, WAITING_CRANE_AND_WAREHOUSE,
               "compatible crane set and warehouse");
    break;
  case CARGO:
    if (--engine->remaining[ship->id])
      break;
    if (!ship->loading)
    {
      port->res.warehouse.reserved -= engine->portions[ship->id];
      port->res.warehouse.used += engine->portions[ship->id];
      port->res.warehouse.stock[ship->cargoType] += engine->portions[ship->id];
    }
    ship->cargoVolume -= engine->portions[ship->id];
    engine->cargoMoved += engine->portions[ship->id];
    for (int craneIndex = 0; craneIndex < ship->cranesNeeded; ++craneIndex)
    {
      int id = engine->assigned[ship->id][craneIndex];
      freeCrane(&port->res, id);
      port->res.cranes[id].shipId = -1;
      PORT_EVENT(engine, "Crane #%d released by ship #%d", id, ship->id);
    }
    ship->craneId = -1;
    statsRecordUnload(&port->stats);
    PORT_EVENT(engine,
               "Ship #%d cargo operation completed: portion=%d remaining=%d "
               "warehouse=%d reserved=%d capacity=%d",
               ship->id, engine->portions[ship->id], ship->cargoVolume,
               port->res.warehouse.used, port->res.warehouse.reserved,
               port->res.warehouse.capacity);
    setWaiting(engine, ship,
               ship->cargoVolume ? WAITING_CRANE_AND_WAREHOUSE : WAITING_DEPART,
               ship->cargoVolume ? "crane set and warehouse" : "departure tug");
    break;
  case EXIT:
    freeTug(&port->res, ship->tugId);
    port->res.tugs[ship->tugId].shipId = -1;
    PORT_EVENT(engine, "Tug #%d released by ship #%d", ship->tugId, ship->id);
    ship->tugId = -1;
    startJob(engine, engine->actorBerth[ship->berthId], RELEASE, ship->id, 0,
             0);
    break;
  case RELEASE:
    if (engine->ipc->meta.role[job.actor] == BERTH_ACTOR)
    {
      PORT_EVENT(engine, "Berth #%d released by ship #%d", ship->berthId,
                 ship->id);
      freeBerth(&port->res, ship->berthId);
      ship->berthId = -1;
      if (ship->state == UNMOORING)
      {
        setWaiting(engine, ship, WAITING_WAREHOUSE,
                   "warehouse in the port waiting area");
      }
      else
      {
        startJob(engine, engine->actorShip[ship->id], RELEASE, ship->id, 0, 0);
      }
    }
    else
    {
      ship->state = DEPARTED;
      statsRecordDeparture(&port->stats);
      PORT_EVENT(engine, "Ship #%d departed; all equipment released", ship->id);
    }
    break;
  case DRAIN:
  {
    Warehouse *warehouse = &port->res.warehouse;
    int *result = job.response.warehouseData;
    if (result[0] < 0 || result[1] < 0 || result[0] > warehouse->capacity ||
        result[1] > warehouse->capacity - result[0] ||
        result[0] + result[1] > warehouse->capacity - warehouse->reserved ||
        result[2] < 0 || result[3] < 0)
    {
      engine->failed = 1;
      break;
    }
    int out = result[2], supplied = result[3];
    warehouse->stock[0] = result[0];
    warehouse->stock[1] = result[1];
    warehouse->used = result[0] + result[1];

    PORT_EVENT(
        engine,
        "Warehouse: exported=%d supplied=%d used=%d reserved=%d capacity=%d",
        out, supplied, warehouse->used, warehouse->reserved,
        warehouse->capacity);
    break;
  }
  default:
    engine->failed = 1;
  }
}
