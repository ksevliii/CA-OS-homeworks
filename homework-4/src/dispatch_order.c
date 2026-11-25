#include "process.h"

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
