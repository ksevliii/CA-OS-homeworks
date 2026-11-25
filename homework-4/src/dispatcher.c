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

int dispatchShip(Engine *engine, Ship *ship)
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
