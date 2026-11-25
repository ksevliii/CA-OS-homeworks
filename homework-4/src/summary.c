#include "process.h"

void summary(Engine *engine, const char *reason)
{
  Port *port = engine->port;
  long long totalWait = 0;
  PORT_EVENT(engine, "Termination: %s", reason);
  for (int index = 0; index < port->shipCount; ++index)
  {
    Ship *ship = &port->ships[index];
    totalWait += ship->totalWait;
    if (ship->state == DEPARTED)
      continue;
    ++port->stats.shipsPending;
    if (ship->cargoVolume > 0 && ship->cargoVolume < ship->startVolume)
      ++port->stats.shipsPartial;
    if (ship->state == QUEUED || ship->state == WAITING_BERTH ||
        ship->state == WAITING_CRANE_AND_WAREHOUSE ||
        ship->state == WAITING_DEPART || ship->state == WAITING_WAREHOUSE)
      totalWait += port->currentTime - ship->waitSince;
    PORT_EVENT(
        engine,
        "Ship #%d not finished: %s operation=%s remaining=%d of %d berth=%d "
        "tug=%d crane=%d",
        ship->id, shipStateName(ship->state), ship->loading ? "load" : "unload",
        ship->cargoVolume, ship->startVolume, ship->berthId, ship->tugId,
        ship->craneId);
  }
  PORT_EVENT(engine,
             "FINAL arrived=%d departed=%d pending=%d partial=%d "
             "cargo_operations=%d moved=%d wait=%lld max_queue=%d",
             port->stats.shipsArrived, port->stats.shipsDeparted,
             port->stats.shipsPending, port->stats.shipsPartial,
             port->stats.unloads, engine->cargoMoved, totalWait,
             port->stats.maxQueueLen);
  PORT_EVENT(engine, "Проверка ограничений ресурсов: %s",
             checkInvariants(engine) ? "пройдена" : "ОШИБКА");
}
