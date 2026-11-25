#include "ui.h"
#include "process.h"
#include <stdio.h>

#define UI_APPEND(...)                                                  \
  do                                                                    \
  {                                                                     \
    int length = snprintf(buffer + used, capacity - used, __VA_ARGS__); \
    if (length < 0 || (size_t)length >= capacity - used)                \
      return 0;                                                         \
    used += (size_t)length;                                             \
  } while (0)

int uiFormat(Port *port, char *buffer, size_t capacity)
{
  if (port == NULL || buffer == NULL || capacity == 0)
  {
    fprintf(stderr, "SOFT_ASSERT failed: port != NULL in uiRender\n");
    return 0;
  }
  size_t used = 0;
  UI_APPEND("\n+---------------- ARKHANGELSK BAKARITSA PORT ----------------+\n");
  if (port->maxTime > 0)
    UI_APPEND("| Time: %-4d / %-4d                                          |\n",
              port->currentTime, port->maxTime);
  else
    UI_APPEND("| Time: %-4d / unlimited                                     |\n",
              port->currentTime);

  UI_APPEND("+------------------------ RESOURCES -------------------------+\n");
  UI_APPEND("| Tugs (%d):   ", port->res.tugCount);
  for (int idx = 0; idx < port->res.tugCount; ++idx)
    UI_APPEND("[ %d%s] ", idx, port->res.tugs[idx].busy ? "*" : " ");
  UI_APPEND("\n");

  UI_APPEND("| Berths (%d): ", port->res.berthCount);
  for (int idx = 0; idx < port->res.berthCount; ++idx)
    UI_APPEND("[ %d%s] ", idx, port->res.berths[idx].shipId != -1 ? "#" : " ");
  UI_APPEND("\n");

  UI_APPEND("| Cranes (%d): ", port->res.craneCount);
  for (int idx = 0; idx < port->res.craneCount; ++idx)
    UI_APPEND("[ %d%s] ", idx, port->res.cranes[idx].busy ? "^" : " ");
  UI_APPEND("\n");

  UI_APPEND("| Warehouse: %d / %d, reserved=%d\n", port->res.warehouse.used,
            port->res.warehouse.capacity, port->res.warehouse.reserved);

  UI_APPEND("+--------------------------- SHIPS --------------------------+\n");
  for (int idx = 0; idx < port->shipCount; ++idx)
  {
    const Ship *ship = &port->ships[idx];
    UI_APPEND("| #%d type=%d cargo=%d volume=%-5d | %-25s | %s\n", ship->id,
              ship->type, ship->cargoType, ship->cargoVolume,
              shipStateName(ship->state), ship->loading ? "load" : "unload");
  }
  UI_APPEND("+-------------------------- EVENTS --------------------------+\n");
  int first = (port->eventNext + EVENT_LINES - port->eventCount) % EVENT_LINES;
  for (int idx = 0; idx < port->eventCount; ++idx)
    UI_APPEND("| %s", port->events[(first + idx) % EVENT_LINES]);
  UI_APPEND("+------------------------------------------------------------+\n");
  return (int)used;
}

int uiRender(const char *snapshot)
{
  if (snapshot == NULL)
  {
    fprintf(stderr, "SOFT_ASSERT failed: snapshot is NULL\n");
    return 0;
  }
  if (isatty(STDOUT_FILENO) && !fullWrite(STDOUT_FILENO, "\033[H\033[2J", 7))
    return 0;
  return fullWrite(STDOUT_FILENO, snapshot, strlen(snapshot));
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
