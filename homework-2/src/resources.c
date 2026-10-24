#include "resources.h"
#include "port.h"
#include "simulation.h"
#include "utils.h"
#include <stdlib.h>

static const int warehouseOutShare = 4; // за раз со склада вывозится 1/4 вместимости

void resInit(Resources *res)
{
    if (res == NULL)
    {
        fprintf(stderr, "failed: res != NULL\n");
        perror("Resources pointer is NULL");
        return;
    }
    *res = (Resources){
        .tugs = NULL,
        .tugCount = 0,
        .berths = NULL,
        .berthCount = 0,
        .cranes = NULL,
        .craneCount = 0,
        .warehouse = {.capacity = 0, .used = 0}};
}

void resFree(Resources *res)
{
    if (res == NULL)
    {
        fprintf(stderr, "failed: res != NULL\n");
        perror("Resources pointer is NULL");
        return;
    }
    for (int tugNum = 0; res->tugs != NULL && tugNum < res->tugCount; ++tugNum)
        semaDestroy(&res->tugs[tugNum].start);
    for (int craneNum = 0; res->cranes != NULL && craneNum < res->craneCount; ++craneNum)
        semaDestroy(&res->cranes[craneNum].start);
    free(res->tugs);
    free(res->berths);
    free(res->cranes);
    res->tugs = NULL;
    res->berths = NULL;
    res->cranes = NULL;
    res->tugCount = res->berthCount = res->craneCount = 0;
}

bool resAlloc(Resources *res, int tugCount, int berthCount, int craneCount,
              int warehouseCapacity)
{
    SOFT_ASSERT(res != NULL, "Resources pointer is NULL", false);
    SOFT_ASSERT(tugCount >= 0, "tugCount < 0", false);
    SOFT_ASSERT(berthCount >= 0, "berthCount < 0", false);
    SOFT_ASSERT(craneCount >= 0, "craneCount < 0", false);
    SOFT_ASSERT(warehouseCapacity > 0, "warehouseCapacity <= 0", false);
    res->tugs = calloc((size_t)tugCount, sizeof(Tug));
    res->berths = calloc((size_t)berthCount, sizeof(Berth));
    res->cranes = calloc((size_t)craneCount, sizeof(Crane));
    if (res->tugs == NULL || res->berths == NULL || res->cranes == NULL)
    {
        perror("resAlloc: calloc failed");
        resFree(res);
        return false;
    }
    res->tugCount = tugCount;
    res->berthCount = berthCount;
    res->craneCount = craneCount;
    res->warehouse = (Warehouse){.capacity = warehouseCapacity, .used = 0};
    for (int tugNum = 0; tugNum < tugCount; ++tugNum)
    {
        res->tugs[tugNum] = (Tug){.id = tugNum, .busy = false, .shipId = -1, .port = NULL};
        semaInit(&res->tugs[tugNum].start, 0);
    }
    for (int berthNum = 0; berthNum < berthCount; ++berthNum)
        res->berths[berthNum] = (Berth){.id = berthNum, .shipId = -1, .compatibility = {true, true, true}};
    for (int craneNum = 0; craneNum < craneCount; ++craneNum)
    {
        res->cranes[craneNum] = (Crane){.id = craneNum,
                                        .busy = false,
                                        .compatibility = {true, true},
                                        .shipId = -1,
                                        .volume = 0,
                                        .port = NULL};
        semaInit(&res->cranes[craneNum].start, 0);
    }
    return true;
}

void setBerthCompatibility(Resources *res, int berthId, ShipType shipType,
                           bool isCompatible)
{
    if (res == NULL)
    {
        fprintf(stderr, "failed: res != NULL\n");
        perror("Resources pointer is NULL");
        return;
    }
    if (berthId >= 0 && berthId < res->berthCount && shipType >= SMALL && shipType <= LARGE)
        res->berths[berthId].compatibility[shipType] = isCompatible;
}

void setCraneCompatibility(Resources *res, int craneId,
                           CargoType cargoType, bool isCompatible)
{
    if (res == NULL)
    {
        fprintf(stderr, "failed: res != NULL\n");
        perror("Resources pointer is NULL");
        return;
    }
    if (craneId >= 0 && craneId < res->craneCount && cargoType >= CONTAINER && cargoType <= LIQUID)
        res->cranes[craneId].compatibility[cargoType] = isCompatible;
}

int takeTug(Resources *res)
{
    SOFT_ASSERT(res != NULL, "Resources pointer is NULL", -1);
    for (int tugNum = 0; tugNum < res->tugCount; ++tugNum)
    {
        if (!res->tugs[tugNum].busy)
        {
            res->tugs[tugNum].busy = true;
            return tugNum;
        }
    }
    return -1;
}

void freeTug(Resources *res, int tugId)
{
    if (res == NULL)
    {
        fprintf(stderr, "failed: res != NULL\n");
        perror("Resources pointer is NULL");
        return;
    }
    if (tugId >= 0 && tugId < res->tugCount)
        res->tugs[tugId].busy = false;
}

int takeBerth(Resources *res, ShipType shipType, int shipId)
{
    SOFT_ASSERT(res != NULL, "Resources pointer is NULL", -1);
    for (int berthNum = 0; berthNum < res->berthCount; ++berthNum)
    {
        if (res->berths[berthNum].shipId == -1 && res->berths[berthNum].compatibility[shipType])
        {
            res->berths[berthNum].shipId = shipId;
            return berthNum;
        }
    }
    return -1;
}

void freeBerth(Resources *res, int berthId)
{
    if (res == NULL)
    {
        fprintf(stderr, "failed: res != NULL\n");
        perror("Resources pointer is NULL");
        return;
    }
    if (berthId >= 0 && berthId < res->berthCount)
        res->berths[berthId].shipId = -1;
}

int takeCrane(Resources *res, CargoType cargoType)
{
    SOFT_ASSERT(res != NULL, "Resources pointer is NULL", -1);
    for (int craneNum = 0; craneNum < res->craneCount; ++craneNum)
    {
        if (!res->cranes[craneNum].busy && res->cranes[craneNum].compatibility[cargoType])
        {
            res->cranes[craneNum].busy = true;
            return craneNum;
        }
    }
    return -1;
}

void freeCrane(Resources *res, int craneId)
{
    if (res == NULL)
    {
        fprintf(stderr, "failed: res != NULL\n");
        perror("Resources pointer is NULL");
        return;
    }
    if (craneId >= 0 && craneId < res->craneCount)
        res->cranes[craneId].busy = false;
}

void takeWarehouse(Resources *res, int cargoVolume)
{
    if (res == NULL)
    {
        fprintf(stderr, "failed: res != NULL\n");
        perror("Resources pointer is NULL");
        return;
    }
    if (cargoVolume <= 0)
    {
        fprintf(stderr, "failed: cargoVolume > 0\n");
        perror("cargoVolume <= 0");
        return;
    }
    res->warehouse.used += cargoVolume;
}

void freeWarehouse(Resources *res, int cargoVolume)
{
    if (res == NULL)
    {
        fprintf(stderr, "failed: res != NULL\n");
        perror("Resources pointer is NULL");
        return;
    }
    if (cargoVolume <= 0)
    {
        fprintf(stderr, "failed: cargoVolume > 0\n");
        perror("cargoVolume <= 0");
        return;
    }
    res->warehouse.used -= cargoVolume;
    if (res->warehouse.used < 0)
        res->warehouse.used = 0;
}

void *tugThread(void *arg)
{
    Tug *tug = (Tug *)arg;
    Port *port = tug->port;
    char msg[256];
    while (portWait(port, &tug->start))
    {
        pthread_mutex_lock(&port->mutex);
        Ship *ship = &port->ships[tug->shipId];
        bool entering = (ship->state == ENTERING);
        int until = ship->eventTime;
        pthread_mutex_unlock(&port->mutex);
        if (!simWait(port, until))
            break;
        pthread_mutex_lock(&port->mutex);
        freeTug(&port->res, tug->id);
        tug->shipId = -1;
        ship->tugId = -1;
        sprintf(msg, "Tug #%d released: ship #%d %s", tug->id, ship->id,
                entering ? "entered the port" : "left the port");
        logEvent(port, msg);
        pthread_mutex_unlock(&port->mutex);
        semaPost(&ship->granted);
        semaPost(&port->disp.wake);
    }
    return NULL;
}

void *craneThread(void *arg)
{
    Crane *crane = (Crane *)arg;
    Port *port = crane->port;
    char msg[256];
    while (portWait(port, &crane->start))
    {
        pthread_mutex_lock(&port->mutex);
        Ship *ship = &port->ships[crane->shipId];
        int until = ship->eventTime;
        int middle = (port->currentTime + until) / 2;
        sprintf(msg, "Crane #%d: unloading of ship #%d started (portion %d)",
                crane->id, ship->id, crane->volume);
        logEvent(port, msg);
        pthread_mutex_unlock(&port->mutex);
        if (!simWait(port, middle))
            break;
        pthread_mutex_lock(&port->mutex);
        sprintf(msg, "Crane #%d: unloading of ship #%d in progress", crane->id, ship->id);
        logEvent(port, msg);
        pthread_mutex_unlock(&port->mutex);
        if (!simWait(port, until))
            break;
        pthread_mutex_lock(&port->mutex);
        ship->cargoVolume -= crane->volume;
        crane->volume = 0;
        statsRecordUnload(&port->stats);
        freeCrane(&port->res, crane->id);
        crane->shipId = -1;
        ship->craneId = -1;
        sprintf(msg, "Crane #%d released: ship #%d portion unloaded, %d left on board",
                crane->id, ship->id, ship->cargoVolume);
        logEvent(port, msg);
        pthread_mutex_unlock(&port->mutex);
        semaPost(&ship->granted);
        semaPost(&port->disp.wake);
    }
    return NULL;
}

void *warehouseThread(void *arg)
{
    Port *port = (Port *)arg;
    char msg[256];
    int period = (port->disp.unloadTime > 0) ? port->disp.unloadTime : 1;
    int next = period;
    while (simWait(port, next))
    {
        pthread_mutex_lock(&port->mutex);
        Warehouse *wh = &port->res.warehouse;
        int out = wh->capacity / warehouseOutShare + 1;
        if (out > wh->used)
            out = wh->used;
        if (out > 0)
        {
            freeWarehouse(&port->res, out);
            sprintf(msg, "Warehouse: %d units shipped out (warehouse %d/%d)",
                    out, wh->used, wh->capacity);
            logEvent(port, msg);
        }
        pthread_mutex_unlock(&port->mutex);
        if (out > 0)
            semaPost(&port->disp.wake);
        next += period;
    }
    return NULL;
}
