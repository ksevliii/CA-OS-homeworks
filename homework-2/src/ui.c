#include "ui.h"
#include <stdio.h>

void uiRender(Port *port)
{
    pthread_mutex_lock(&port->mutex);
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

    printf("| Warehouse: %d / %d\n",
           port->res.warehouse.used, port->res.warehouse.capacity);

    printf("+--------------------------- SHIPS --------------------------+\n");
    for (int idx = 0; idx < port->shipCount; ++idx)
    {
        const Ship *ship = &port->ships[idx];
        printf("| #%d type=%d cargo=%d volume=%-5d | %-25s |\n",
               ship->id, ship->type, ship->cargoType, ship->cargoVolume,
               shipStateName(ship->state));
    }
    printf("+-------------------------- EVENTS --------------------------+\n");
    int first = (port->eventCount > EVENT_LINES) ? port->eventCount - EVENT_LINES : 0;
    for (int idx = first; idx < port->eventCount; ++idx)
        printf("| %s\n", port->events[idx % EVENT_LINES]);
    printf("+------------------------------------------------------------+\n");
    pthread_mutex_unlock(&port->mutex);
}
