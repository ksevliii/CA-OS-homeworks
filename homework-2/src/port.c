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
    *port = (Port){
        .res = {0},
        .ships = NULL,
        .shipCount = 0,
        .disp = {0},
        .log = NULL,
        .stats = {0},
        .currentTime = 0,
        .maxTime = 0,
        .delayMs = 1,
        .interrupted = false,
        .stop = 0,
        .eventCount = 0};
    pthread_mutex_init(&port->mutex, NULL);
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
    for (int ship = 0; ship < port->shipCount; ++ship)
        semaDestroy(&port->ships[ship].granted);
    free(port->ships);
    port->ships = NULL;
    port->shipCount = 0;
    semaDestroy(&port->disp.wake);
    pthread_mutex_destroy(&port->mutex);
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
        port->ships[ship] = (Ship){.id = ship,
                                   .tugId = -1,
                                   .berthId = -1,
                                   .craneId = -1,
                                   .state = NOT_ARRIVED};
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
    port->ships[idx].port = port;
}

bool portStopped(Port *port)
{
    pthread_mutex_lock(&port->mutex);
    bool stopped = (port->stop != 0);
    pthread_mutex_unlock(&port->mutex);
    return stopped;
}

bool portWait(Port *port, Sema *sema)
{
    SOFT_ASSERT(port != NULL, "Port pointer is NULL", false);
    SOFT_ASSERT(sema != NULL, "Sema pointer is NULL", false);
    if (!semaWait(sema))
        return false;
    return !portStopped(port);
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

void logEvent(Port *port, const char *text)
{
    if (port == NULL)
    {
        fprintf(stderr, "failed: port != NULL\n");
        perror("Port pointer is NULL");
        return;
    }
    if (text == NULL)
    {
        fprintf(stderr, "failed: text != NULL\n");
        perror("Event text is NULL");
        return;
    }
    char line[EVENT_LEN + 16];
    int len = snprintf(line, sizeof(line), "[T=%04d] %s\n", port->currentTime, text);
    if (len >= (int)sizeof(line))
        len = (int)sizeof(line) - 1;
    if (port->log != NULL)
        loggerWrite(port->log, line, len);
    snprintf(port->events[port->eventCount % EVENT_LINES], EVENT_LEN, "[T=%04d] %s",
             port->currentTime, text);
    port->eventCount++;
}
