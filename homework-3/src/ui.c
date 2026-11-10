#include "ui.h"
#include <stdio.h>

void uiRender(Port *port)
{
  if (port == NULL)
  {
    fprintf(stderr, "SOFT_ASSERT failed: port != NULL in uiRender\n");
    return;
  }
  printf("\033[H\033[2J");
  printf("\n+---------------- ARKHANGELSK BAKARITSA PORT ----------------+\n");
  if (port->maxTime > 0)
    printf("| Time: %-4d / %-4d                                          |\n",
           port->currentTime, port->maxTime);
  else
    printf("| Time: %-4d / unlimited                                     |\n",
           port->currentTime);

  printf("+------------------------ RESOURCES -------------------------+\n");
  printf("| Tugs (%d):   ", port->res.tugCount);
  for (int idx = 0; idx < port->res.tugCount; ++idx)
    printf("[ %d%s] ", idx, port->res.tugs[idx].busy ? "*" : " ");
  printf("\n");

  printf("| Berths (%d): ", port->res.berthCount);
  for (int idx = 0; idx < port->res.berthCount; ++idx)
    printf("[ %d%s] ", idx, port->res.berths[idx].shipId != -1 ? "#" : " ");
  printf("\n");

  printf("| Cranes (%d): ", port->res.craneCount);
  for (int idx = 0; idx < port->res.craneCount; ++idx)
    printf("[ %d%s] ", idx, port->res.cranes[idx].busy ? "^" : " ");
  printf("\n");

  printf("| Warehouse: %d / %d, reserved=%d\n", port->res.warehouse.used,
         port->res.warehouse.capacity, port->res.warehouse.reserved);

  printf("+--------------------------- SHIPS --------------------------+\n");
  for (int idx = 0; idx < port->shipCount; ++idx)
  {
    const Ship *ship = &port->ships[idx];
    printf("| #%d type=%d cargo=%d volume=%-5d | %-25s | %s\n", ship->id,
           ship->type, ship->cargoType, ship->cargoVolume,
           shipStateName(ship->state), ship->loading ? "load" : "unload");
  }
  printf("+-------------------------- EVENTS --------------------------+\n");
  int first = (port->eventNext + EVENT_LINES - port->eventCount) % EVENT_LINES;
  for (int idx = 0; idx < port->eventCount; ++idx)
    printf("| %s", port->events[(first + idx) % EVENT_LINES]);
  printf("+------------------------------------------------------------+\n");
  fflush(stdout);
}

void uiAddEvent(Port *port, const char *text)
{
  if (port == NULL || text == NULL)
  {
    fprintf(stderr,
            "SOFT_ASSERT failed: non-null port and event in uiAddEvent\n");
    return;
  }
  snprintf(port->events[port->eventNext], EVENT_TEXT_LENGTH, "%s", text);
  port->eventNext = (port->eventNext + 1) % EVENT_LINES;
  if (port->eventCount < EVENT_LINES)
    ++port->eventCount;
}
