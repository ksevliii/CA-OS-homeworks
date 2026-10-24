#include "dispatcher.h"
#include "port.h"
#include "utils.h"
#include <stdlib.h>

static int randomDelay(Dispatcher *disp, int base) // случайная задержка от base до base + 1/5*base
{
    if (base <= 1)
        return base;
    int max_variation = base / 5;
    int variation = rand_r(&disp->seed) % (max_variation + 1);
    int result = base + variation;
    return result;
}

void dispInit(Dispatcher *disp, Resources *res, int enterTime, int unloadTime, unsigned int seed)
{
    if (disp == NULL)
    {
        fprintf(stderr, "failed: disp != NULL\n");
        perror("Dispatcher pointer is NULL");
        return;
    }
    if (res == NULL)
    {
        fprintf(stderr, "failed: res != NULL\n");
        perror("Resources pointer is NULL");
        return;
    }
    *disp = (Dispatcher){
        .res = res,
        .enterExitTime = enterTime,
        .unloadTime = unloadTime,
        .seed = seed};
    semaInit(&disp->wake, 0);
}

bool tryEnter(Dispatcher *disp, Ship *ship)
{
    SOFT_ASSERT(disp != NULL, "Dispatcher pointer is NULL", false);
    SOFT_ASSERT(ship != NULL, "Ship pointer is NULL", false);
    int tugId = takeTug(disp->res);
    if (tugId < 0)
        return false;
    int delay = randomDelay(disp, disp->enterExitTime);
    ship->tugId = tugId;
    disp->res->tugs[tugId].shipId = ship->id;
    ship->state = ENTERING;
    ship->eventTime = ship->port->currentTime + delay;
    semaPost(&disp->res->tugs[tugId].start); // будим поток буксира
    return true;
}

bool tryBerth(Dispatcher *disp, Ship *ship)
{
    SOFT_ASSERT(disp != NULL, "Dispatcher pointer is NULL", false);
    SOFT_ASSERT(ship != NULL, "Ship pointer is NULL", false);
    int berthId = takeBerth(disp->res, ship->type, ship->id);
    if (berthId < 0)
        return false;
    ship->berthId = berthId;
    ship->state = MOORED;
    semaPost(&ship->granted); // будим поток судна
    return true;
}

UnloadResult tryUnload(Dispatcher *disp, Ship *ship)
{
    SOFT_ASSERT(disp != NULL, "Dispatcher pointer is NULL", WAIT);
    SOFT_ASSERT(ship != NULL, "Ship pointer is NULL", WAIT);
    int craneId = takeCrane(disp->res, ship->cargoType);
    if (craneId < 0)
        return WAIT;
    int warehouseFree = disp->res->warehouse.capacity - disp->res->warehouse.used;
    if (warehouseFree == 0) // кран и место резервируются вместе, я хейтер дедлока
    {
        freeCrane(disp->res, craneId);
        return WAIT_WAREHOUSE;
    }
    int toUnload = (ship->cargoVolume < warehouseFree) ? ship->cargoVolume : warehouseFree;
    takeWarehouse(disp->res, toUnload);
    int delay = randomDelay(disp, disp->unloadTime);
    ship->craneId = craneId;
    disp->res->cranes[craneId].shipId = ship->id;
    ship->state = UNLOADING;
    disp->res->cranes[craneId].volume = toUnload;
    ship->eventTime = ship->port->currentTime + delay;
    semaPost(&disp->res->cranes[craneId].start); // будим поток крана
    return SUCCESS;
}

bool tryDepart(Dispatcher *disp, Ship *ship)
{
    SOFT_ASSERT(disp != NULL, "Dispatcher pointer is NULL", false);
    SOFT_ASSERT(ship != NULL, "Ship pointer is NULL", false);
    int tugId = takeTug(disp->res);
    if (tugId < 0)
        return false;
    int delay = randomDelay(disp, disp->enterExitTime);
    ship->tugId = tugId;
    disp->res->tugs[tugId].shipId = ship->id;
    freeBerth(disp->res, ship->berthId);
    ship->berthId = -1;
    ship->state = EXITING;
    ship->eventTime = ship->port->currentTime + delay;
    semaPost(&disp->res->tugs[tugId].start);
    return true;
}

static bool dispPass(Port *port)
{
    bool any = false;
    int queued = 0;
    char msg[256];
    for (int idx = 0; idx < port->shipCount; ++idx)
    {
        Ship *ship = &port->ships[idx];
        bool progressed = false;
        int waited = port->currentTime - ship->waitSince;
        switch (ship->state)
        {
        case QUEUED:
            if (tryEnter(&port->disp, ship))
            {
                sprintf(msg, "Ship #%d: tug #%d assigned, entering", ship->id, ship->tugId);
                logEvent(port, msg);
                progressed = true;
            }
            else
                queued++;
            break;
        case WAITING_BERTH:
            if (tryBerth(&port->disp, ship))
            {
                sprintf(msg, "Ship #%d: moored at berth #%d", ship->id, ship->berthId);
                logEvent(port, msg);
                progressed = true;
            }
            else
                queued++;
            break;
        case WAITING_CRANE_AND_WAREHOUSE:
        {
            UnloadResult res = tryUnload(&port->disp, ship);
            if (res == WAIT)
            {
                queued++;
            }
            else if (res == WAIT_WAREHOUSE)
            {
                if (!ship->warehouseWait)
                {
                    ship->warehouseWait = true;
                    sprintf(msg, "Ship #%d: warehouse is full, unloading suspended (cargo left: %d)",
                            ship->id, ship->cargoVolume);
                    logEvent(port, msg);
                }
                queued++;
            }
            else
            {
                if (ship->warehouseWait)
                {
                    ship->warehouseWait = false;
                    sprintf(msg, "Ship #%d: warehouse has free space, unloading resumed", ship->id);
                    logEvent(port, msg);
                }
                sprintf(msg, "Ship #%d: crane #%d assigned, portion %d of %d, warehouse %d/%d",
                        ship->id, ship->craneId, port->res.cranes[ship->craneId].volume,
                        ship->cargoVolume, port->res.warehouse.used, port->res.warehouse.capacity);
                logEvent(port, msg);
                progressed = true;
            }
            break;
        }
        case WAITING_DEPART:
        {
            int berthId = ship->berthId;
            if (tryDepart(&port->disp, ship))
            {
                sprintf(msg, "Ship #%d: tug #%d assigned, exiting, berth #%d released",
                        ship->id, ship->tugId, berthId);
                logEvent(port, msg);
                progressed = true;
            }
            else
                queued++;
            break;
        }
        default:
            break;
        }
        if (progressed)
        {
            ship->totalWait += waited;
            any = true;
        }
    }
    statsUpdateMaxQueue(&port->stats, queued);
    return any;
}

void *dispatcherThread(void *arg)
{
    Port *port = (Port *)arg;
    while (portWait(port, &port->disp.wake)) // ниче от нас не надо -> спим
    {
        pthread_mutex_lock(&port->mutex);
        while (dispPass(port))
            ;
        pthread_mutex_unlock(&port->mutex);
    }
    return NULL;
}
