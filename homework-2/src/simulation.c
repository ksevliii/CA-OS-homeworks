#include "simulation.h"
#include "ui.h"
#include "utils.h"
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

bool simWait(Port *port, int until)
{
    SOFT_ASSERT(port != NULL, "Port pointer is NULL", false);
    useconds_t step = (useconds_t)(port->delayMs * 250);
    while (true)
    {
        pthread_mutex_lock(&port->mutex);
        int now = port->currentTime;
        bool stopped = (port->stop != 0);
        pthread_mutex_unlock(&port->mutex);
        if (stopped)
            return false;
        if (now >= until)
            return true;
        usleep(step);
    }
}

static bool startThread(pthread_t *threads, int *started, void *(*func)(void *), void *arg)
{
    int rc = pthread_create(&threads[*started], NULL, func, arg);
    if (rc != 0)
    {
        fprintf(stderr, "pthread_create failed: %s\n", strerror(rc));
        return false;
    }
    (*started)++;
    return true;
}

void simulationRun(Port *port, int delayMs, Logger *log)
{
    if (port == NULL)
    {
        fprintf(stderr, "failed: port != NULL\n");
        perror("Port pointer is NULL");
        return;
    }
    if (log == NULL)
    {
        fprintf(stderr, "failed: log != NULL\n");
        perror("Logger pointer is NULL");
        return;
    }
    port->log = log;
    port->delayMs = (delayMs > 0) ? delayMs : 1;
    for (int idx = 0; idx < port->res.tugCount; ++idx)
        port->res.tugs[idx].port = port;
    for (int idx = 0; idx < port->res.craneCount; ++idx)
        port->res.cranes[idx].port = port;

    int total = port->shipCount + port->res.tugCount + port->res.craneCount + 2;
    pthread_t *threads = calloc((size_t)total, sizeof(pthread_t));
    int started = 0;
    bool ok = (threads != NULL);
    ok = ok && startThread(threads, &started, dispatcherThread, port);
    ok = ok && startThread(threads, &started, warehouseThread, port);
    for (int idx = 0; idx < port->res.tugCount; ++idx)
        ok = ok && startThread(threads, &started, tugThread, &port->res.tugs[idx]);
    for (int idx = 0; idx < port->res.craneCount; ++idx)
        ok = ok && startThread(threads, &started, craneThread, &port->res.cranes[idx]);
    for (int idx = 0; idx < port->shipCount; ++idx)
        ok = ok && startThread(threads, &started, shipThread, &port->ships[idx]);

    while (ok && !port->interrupted)
    {
        uiRender(port);
        usleep((useconds_t)(port->delayMs * 1000));
        pthread_mutex_lock(&port->mutex);
        bool done = isDone(port);
        pthread_mutex_unlock(&port->mutex);
        if (done || (port->maxTime > 0 && port->currentTime >= port->maxTime))
            break;
        pthread_mutex_lock(&port->mutex);
        port->currentTime++;
        pthread_mutex_unlock(&port->mutex);
    }
    pthread_mutex_lock(&port->mutex);
    port->stop = 1;
    pthread_mutex_unlock(&port->mutex);
    semaPost(&port->disp.wake);
    for (int idx = 0; idx < port->res.tugCount; ++idx)
        semaPost(&port->res.tugs[idx].start);
    for (int idx = 0; idx < port->res.craneCount; ++idx)
        semaPost(&port->res.cranes[idx].start);
    for (int idx = 0; idx < port->shipCount; ++idx)
        semaPost(&port->ships[idx].granted);
    for (int idx = 0; idx < started; ++idx)
        pthread_join(threads[idx], NULL);
    free(threads);
    char msg[256];
    pthread_mutex_lock(&port->mutex);
    int totalWait = 0;
    for (int idx = 0; idx < port->shipCount; ++idx)
    {
        Ship *ship = &port->ships[idx];
        totalWait += ship->totalWait;
        if (ship->state == DEPARTED)
            continue;
        port->stats.shipsPending++;
        if (ship->cargoVolume < ship->startVolume)
            port->stats.shipsPartial++;
        if (ship->state == QUEUED || ship->state == WAITING_BERTH ||
            ship->state == WAITING_CRANE_AND_WAREHOUSE || ship->state == WAITING_DEPART)
            totalWait += port->currentTime - ship->waitSince;
        sprintf(msg, "Ship #%d not finished: %s, cargo left %d of %d",
                ship->id, shipStateName(ship->state), ship->cargoVolume, ship->startVolume);
        logEvent(port, msg);
    }
    statsAddWait(&port->stats, totalWait);
    pthread_mutex_unlock(&port->mutex);
    uiRender(port);
    statsPrint(&port->stats, log);
}
