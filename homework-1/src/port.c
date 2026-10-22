#include "port.h"
#include "utils.h"
#include <stdlib.h>

static const int startQueueCapacity = 60;

void portInit(Port *port)
{
    if (port == NULL)
    {
        fprintf(stderr, "SOFT_ASSERT failed: port != NULL\n");
        perror("Port pointer is NULL");
        return;
    }
    *port = (Port){
        .res = {0},
        .ships = NULL,
        .shipCount = 0,
        .queue = {0},
        .disp = {0},
        .stats = {0},
        .currentTime = 0,
        .maxTime = 0,
        .interrupted = false};
    resInit(&port->res);
    queueInit(&port->queue, startQueueCapacity);
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
    queueFree(&port->queue);
    free(port->ships);
    port->ships = NULL;
}

bool allocShips(Port *port, int shipCount)
{
    SOFT_ASSERT(port != NULL, "Port pointer is NULL", false);
    SOFT_ASSERT(shipCount > 0, "shipCount <= 0", false);

    port->ships = calloc((size_t)shipCount, sizeof(Ship));
    SOFT_ASSERT(port->ships != NULL, "allocShips: calloc failed", false);

    port->shipCount = shipCount;
    for (int ship = 0; ship < shipCount; ++ship)
    {
        port->ships[ship] = (Ship){.id = ship, .tugId = -1, .berthId = -1, .craneId = -1, .state = NOT_ARRIVED};
    }
    return true;
}

void addShip(Port *port, int idx, ShipType type, CargoType cargoType, int volume, int arrTime)
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
    queuePush(&port->queue, ARRIVE, arrTime, idx);
    statsRecordArrival(&port->stats);
}

bool isDone(const Port *port)
{
    SOFT_ASSERT(port != NULL, "Port pointer is NULL", true);
    for (int ship = 0; ship < port->shipCount; ++ship)
    {
        if (port->ships[ship].state != DEPARTED)
            return false;
    }
    return queueEmpty(&port->queue);
}