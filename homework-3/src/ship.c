#include "ship.h"

void shipInit(Ship *ship, int id, ShipType type, CargoType cargoType,
              int volume, int arrTime)
{
  if (ship == NULL)
  {
    fprintf(stderr, "failed: ship != NULL\n");
    perror("Ship pointer is NULL");
    return;
  }
  if (id < 0)
  {
    fprintf(stderr, "failed: id >= 0\n");
    perror("Ship id < 0");
    return;
  }
  if (volume < 0)
  {
    fprintf(stderr, "failed: volume > 0\n");
    perror("Ship volume <= 0");
    return;
  }
  *ship = (Ship){.id = id,
                 .type = type,
                 .cargoType = cargoType,
                 .cargoVolume = volume,
                 .arrivalTime = arrTime,
                 .state = NOT_ARRIVED,
                 .tugId = -1,
                 .berthId = -1,
                 .craneId = -1,
                 .totalWait = 0,
                 .startVolume = volume,
                 .waitSince = 0,
                 .warehouseWait = false};
}

const char *shipStateName(ShipState state)
{
  switch (state)
  {
  case NOT_ARRIVED:
    return "not arrived";
  case QUEUED:
    return "queued";
  case ENTERING:
    return "entering";
  case WAITING_BERTH:
    return "waiting berth";
  case MOORED:
    return "moored";
  case WAITING_CRANE_AND_WAREHOUSE:
    return "waiting crane/warehouse";
  case UNLOADING:
    return "unloading";
  case WAITING_DEPART:
    return "waiting depart tug";
  case EXITING:
    return "exiting";
  case DEPARTED:
    return "departed";
  case UNMOORING:
    return "unmooring to waiting area";
  case WAITING_WAREHOUSE:
    return "waiting warehouse in waiting area";
  }
  return "unknown state";
}
