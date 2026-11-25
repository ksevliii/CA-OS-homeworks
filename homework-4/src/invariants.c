#include "process.h"

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
