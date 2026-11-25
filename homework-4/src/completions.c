#include "process.h"

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
