#include "config.h"
#include "utils.h"
#include <fcntl.h>
#include <unistd.h>
#include <stdio.h>

static int parseInt(const char **ptr) // парсинг целых чисел из файла
{
    SOFT_ASSERT(ptr != NULL, "parseInt: pointer is NULL", 0);
    while (**ptr != '\0' && (**ptr < '0' || **ptr > '9'))
    {
        (*ptr)++;
    }
    if (**ptr == '\0')
        return 0;
    int val = 0;
    while (**ptr >= '0' && **ptr <= '9')
    {
        val = val * 10 + (**ptr - '0');
        (*ptr)++;
    }
    return val;
}

bool configLoad(Port *port, const char *path)
{
    SOFT_ASSERT(port != NULL, "Port pointer is NULL", false);
    SOFT_ASSERT(path != NULL, "Config path is NULL", false);
    int fd = open(path, O_RDONLY);
    SOFT_ASSERT(fd >= 0, "configLoad: open failed", false);
    char buf[4096];
    ssize_t bytes = read(fd, buf, sizeof(buf) - 1);
    close(fd);
    SOFT_ASSERT(bytes > 0, "configLoad: read failed", false);
    buf[bytes] = '\0';
    const char *str = buf;
    port->maxTime = parseInt(&str);
    int tugCount = parseInt(&str);
    int berthCount = parseInt(&str);
    int craneCount = parseInt(&str);
    int warehouseCap = parseInt(&str);
    if (!resAlloc(&port->res, tugCount, berthCount, craneCount, warehouseCap))
    {
        perror("configLoad: resAlloc failed");
        return false;
    }
    int enterTime = parseInt(&str);
    int unloadTime = parseInt(&str);
    int shipCount = parseInt(&str);
    unsigned int seed = (unsigned int)parseInt(&str);
    dispInit(&port->disp, &port->res, &port->queue, enterTime, unloadTime, seed);
    for (int idx = 0; idx < berthCount; ++idx)
    {
        int id = parseInt(&str);
        setBerthCompatibility(&port->res, id, SMALL, parseInt(&str) != 0);
        setBerthCompatibility(&port->res, id, MEDIUM, parseInt(&str) != 0);
        setBerthCompatibility(&port->res, id, LARGE, parseInt(&str) != 0);
    }
    for (int idx = 0; idx < craneCount; ++idx)
    {
        int id = parseInt(&str);
        setCraneCompatibility(&port->res, id, CONTAINER, parseInt(&str) != 0);
        setCraneCompatibility(&port->res, id, LIQUID, parseInt(&str) != 0);
    }
    if (!allocShips(port, shipCount))
    {
        perror("configLoad: allocShips failed");
        resFree(&port->res);
        queueFree(&port->queue);
        return false;
    }
    for (int idx = 0; idx < shipCount; ++idx)
    {
        int type = parseInt(&str);
        int cargo = parseInt(&str);
        int volume = parseInt(&str);
        int arrival = parseInt(&str);
        addShip(port, idx, (ShipType)type, (CargoType)cargo, volume, arrival);
    }
    return true;
}