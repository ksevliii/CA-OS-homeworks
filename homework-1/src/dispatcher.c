#include "dispatcher.h"
#include "utils.h"
#include <stdlib.h>

static int randomDelay(int base) // случайная задержка от base до base + 1/5*base
{
    if (base <= 1)
        return base;
    int max_variation = base / 5;
    int variation = rand() % (max_variation + 1);
    int result = base + variation;
    return result;
}

void dispInit(Dispatcher *disp, Resources *res, EventQueue *queue, int enterTime, int unloadTime, unsigned int seed)
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
    if (queue == NULL)
    {
        fprintf(stderr, "failed: queue != NULL\n");
        perror("EventQueue pointer is NULL");
        return;
    }
    *disp = (Dispatcher){
        .res = res,
        .queue = queue,
        .currentTime = 0,
        .enterExitTime = enterTime,
        .unloadTime = unloadTime,
        .seed = seed};
}

bool tryEnter(Dispatcher *disp, Ship *ship)
{
    SOFT_ASSERT(disp != NULL, "Dispatcher pointer is NULL", false);
    SOFT_ASSERT(ship != NULL, "Ship pointer is NULL", false);
    int tugId = takeTug(disp->res);
    if (tugId < 0)
        return false;
    int delay = randomDelay(disp->enterExitTime);
    ship->tugId = tugId;
    ship->state = ENTERING;
    ship->eventTime = disp->currentTime + delay;
    queuePush(disp->queue, ENTER_DONE, ship->eventTime, ship->id);
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
    ship->state = WAITING_CRANE_AND_WAREHOUSE;
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
    if (warehouseFree == 0)
    {
        freeCrane(disp->res, craneId);
        return TO_DEPART;
    }
    int toUnload = (ship->cargoVolume < warehouseFree) ? ship->cargoVolume : warehouseFree;
    takeWarehouse(disp->res, toUnload);
    int delay = randomDelay(disp->unloadTime);
    ship->craneId = craneId;
    ship->state = UNLOADING;
    ship->eventTime = disp->currentTime + delay;
    ship->cargoVolume -= toUnload;
    queuePush(disp->queue, UNLOAD_DONE, ship->eventTime, ship->id);
    return SUCCESS;
}

bool tryDepart(Dispatcher *disp, Ship *ship)
{
    SOFT_ASSERT(disp != NULL, "Dispatcher pointer is NULL", false);
    SOFT_ASSERT(ship != NULL, "Ship pointer is NULL", false);
    int tugId = takeTug(disp->res);
    if (tugId < 0)
        return false;
    int delay = randomDelay(disp->enterExitTime);
    ship->tugId = tugId;
    freeBerth(disp->res, ship->berthId);
    ship->berthId = -1;
    ship->state = EXITING;
    ship->eventTime = disp->currentTime + delay;
    queuePush(disp->queue, EXIT_DONE, ship->eventTime, ship->id);
    return true;
}

void inEvent(Dispatcher *disp, Ship *ships, int shipCount, const Event *event)
{
    if (disp == NULL)
    {
        fprintf(stderr, "failed: disp != NULL\n");
        perror("Dispatcher pointer is NULL");
        return;
    }
    if (ships == NULL)
    {
        fprintf(stderr, "failed: ships != NULL\n");
        perror("Ships array is NULL");
        return;
    }
    if (event == NULL)
    {
        fprintf(stderr, "failed: event != NULL\n");
        perror("Event pointer is NULL");
        return;
    }
    if (event->shipId < 0 || event->shipId >= shipCount)
        return;
    Ship *ship = &ships[event->shipId];
    switch (event->type)
    {
    case ENTER_DONE:
        freeTug(disp->res, ship->tugId);
        ship->tugId = -1;
        ship->state = WAITING_BERTH;
        break;
    case UNLOAD_DONE:
        freeCrane(disp->res, ship->craneId);
        ship->craneId = -1;
        ship->state = WAITING_DEPART;
        break;
    case EXIT_DONE:
        freeTug(disp->res, ship->tugId);
        ship->tugId = -1;
        ship->state = DEPARTED;
        break;
    default:
        break;
    }
}