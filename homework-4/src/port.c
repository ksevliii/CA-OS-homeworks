#include "port.h"
#include "utils.h"
#include <stdlib.h>

void portInit(Port *port)
{
  if (port == NULL)
  {
    fprintf(stderr, "SOFT_ASSERT failed: port != NULL\n");
    perror("Port pointer is NULL");
    return;
  }
  *port = (Port){0};
  port->delayMs = 1;
  resInit(&port->res);
  statsInit(&port->stats);
}

void portFree(Port *port)
{
  if (port == NULL)
  {
    fprintf(stderr, "failed: port != NULL\n");
    perror("Port pointer is NULL");
    return;
  }
  resFree(&port->res);
  free(port->ships);
  port->ships = NULL;
  port->shipCount = 0;
}

bool allocShips(Port *port, int shipCount)
{
  SOFT_ASSERT(port != NULL, "Port pointer is NULL", false);
  SOFT_ASSERT(shipCount >= 0, "shipCount <= 0", false);
  if (shipCount == 0)
    return true;
  port->ships = calloc((size_t)shipCount, sizeof(Ship));
  SOFT_ASSERT(port->ships != NULL, "allocShips: calloc failed", false);
  port->shipCount = shipCount;
  for (int ship = 0; ship < shipCount; ++ship)
  {
    port->ships[ship] = (Ship){.id = ship,
                               .tugId = -1,
                               .berthId = -1,
                               .craneId = -1,
                               .state = NOT_ARRIVED};
  }
  return true;
}

void addShip(Port *port, int idx, ShipType type, CargoType cargoType,
             int volume, int arrTime)
{
  if (port == NULL)
  {
    fprintf(stderr, "failed: port != NULL\n");
    perror("Port pointer is NULL");
    return;
  }
  if (idx < 0 || idx >= port->shipCount)
    return;
  shipInit(&port->ships[idx], idx, type, cargoType, volume, arrTime);
}

bool isDone(const Port *port)
{
  SOFT_ASSERT(port != NULL, "Port pointer is NULL", true);
  for (int ship = 0; ship < port->shipCount; ++ship)
  {
    if (port->ships[ship].state != DEPARTED)
      return false;
  }
  return true;
}
