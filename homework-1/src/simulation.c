#include "simulation.h"
#include "ui.h"
#include "utils.h"
#include <unistd.h>
#include <stdio.h>

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
    Event eve;
    char msg[256];
    int len = 0;
    while (port->currentTime <= port->maxTime && !port->interrupted)
    {
        port->disp.currentTime = port->currentTime;
        while (queuePop(&port->queue, &eve, port->currentTime))
        {
            if (eve.type == ARRIVE)
            {
                port->ships[eve.shipId].state = QUEUED;
                len = sprintf(msg,
                              "[T=%04d] Ship #%d arrived (type=%d, cargo=%d, vol=%d)\n",
                              port->currentTime, eve.shipId,
                              port->ships[eve.shipId].type,
                              port->ships[eve.shipId].cargoType,
                              port->ships[eve.shipId].cargoVolume);
                loggerWrite(log, msg, len);
            }
            else
            {
                inEvent(&port->disp, port->ships, port->shipCount, &eve);
            }
        }
        int queued = 0;
        for (int idx = 0; idx < port->shipCount; ++idx)
        {
            Ship *ship = &port->ships[idx];
            if (ship->state == DEPARTED || ship->state == NOT_ARRIVED)
                continue;
            bool progressed = false;
            switch (ship->state)
            {
            case QUEUED:
                if (tryEnter(&port->disp, ship))
                {
                    len = sprintf(msg,
                                  "[T=%04d] Ship #%d: tug #%d entering\n",
                                  port->currentTime, ship->id, ship->tugId);
                    loggerWrite(log, msg, len);
                    progressed = true;
                }
                else
                    queued++;
                break;
            case WAITING_BERTH:
                if (tryBerth(&port->disp, ship))
                {
                    len = sprintf(msg,
                                  "[T=%04d] Ship #%d: wait to berth #%d\n",
                                  port->currentTime, ship->id, ship->berthId);
                    loggerWrite(log, msg, len);
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
                else if (res == TO_DEPART)
                {
                    ship->state = WAITING_DEPART;
                    len = sprintf(msg,
                                  "[T=%04d] Ship #%d: warehouse full! "
                                  "Departing unloaded (volume: %d)\n",
                                  port->currentTime, ship->id, ship->cargoVolume);
                    loggerWrite(log, msg, len);
                    statsRecordUnload(&port->stats);
                    progressed = true;
                }
                else
                {
                    len = sprintf(msg,
                                  "[T=%04d] Ship #%d: unloading started "
                                  "(volume: %d, warehouse: %d/%d)\n",
                                  port->currentTime, ship->id, ship->cargoVolume,
                                  port->res.warehouse.used,
                                  port->res.warehouse.capacity);
                    loggerWrite(log, msg, len);
                    progressed = true;
                }
                break;
            }
            case WAITING_DEPART:
                if (tryDepart(&port->disp, ship))
                {
                    len = sprintf(msg,
                                  "[T=%04d] Ship #%d: tug #%d exiting\n",
                                  port->currentTime, ship->id, ship->tugId);
                    loggerWrite(log, msg, len);
                    progressed = true;
                }
                else
                    queued++;
                break;

            default:
                break;
            }
            if (!progressed &&
                (ship->state == QUEUED || ship->state == WAITING_BERTH ||
                 ship->state == WAITING_CRANE_AND_WAREHOUSE ||
                 ship->state == WAITING_DEPART))
            {
                ship->totalWait++;
            }
        }
        statsUpdateMaxQueue(&port->stats, queued);
        int dep = 0;
        for (int idx = 0; idx < port->shipCount; ++idx)
            if (port->ships[idx].state == DEPARTED)
                dep++;
        port->stats.shipsDeparted = dep;
        port->stats.shipsPending = port->shipCount - dep;
        uiRender(port);
        if (delayMs > 0)
            usleep((unsigned int)(delayMs * 1000));
        if (isDone(port))
            break;
        port->currentTime++;
    }
    int totalWait = 0;
    for (int idx = 0; idx < port->shipCount; ++idx)
        totalWait += port->ships[idx].totalWait;
    statsAddWait(&port->stats, totalWait);
    statsPrint(&port->stats, log);
}