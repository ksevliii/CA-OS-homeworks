#include "resources.h"
#include "utils.h"
#include <stdlib.h>

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
    free(res->tugs);
    free(res->berths);
    free(res->cranes);
    res->tugs = NULL;
    res->berths = NULL;
    res->cranes = NULL;
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
        res->tugs[tugNum] = (Tug){.id = tugNum, .busy = false};
    for (int berthNum = 0; berthNum < berthCount; ++berthNum)
        res->berths[berthNum] = (Berth){.id = berthNum, .shipId = -1, .compatibility = {true, true, true}};
    for (int craneNum = 0; craneNum < craneCount; ++craneNum)
        res->cranes[craneNum] = (Crane){.id = craneNum, .busy = false, .compatibility = {true, true}};
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