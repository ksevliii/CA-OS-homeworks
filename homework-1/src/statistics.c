#include "statistics.h"
#include "utils.h"
#include <stdio.h>

void statsInit(Statistics *stats)
{
    if (stats == NULL)
    {
        fprintf(stderr, "failed: stats != NULL\n");
        perror("Statistics pointer is NULL");
        return;
    }
    stats->shipsArrived = stats->shipsDeparted = stats->shipsPending = 0;
    stats->totalWaitTime = stats->maxQueueLen = stats->unloads = 0;
}

void statsRecordArrival(Statistics *stats)
{
    if (stats == NULL)
    {
        fprintf(stderr, "failed: stats != NULL\n");
        perror("Statistics pointer is NULL");
        return;
    }
    stats->shipsArrived++;
}

void statsRecordDeparture(Statistics *stats)
{
    if (stats == NULL)
    {
        fprintf(stderr, "failed: stats != NULL\n");
        perror("Statistics pointer is NULL");
        return;
    }
    stats->shipsDeparted++;
}

void statsRecordUnload(Statistics *stats)
{
    if (stats == NULL)
    {
        fprintf(stderr, "failed: stats != NULL\n");
        perror("Statistics pointer is NULL");
        return;
    }
    stats->unloads++;
}

void statsAddWait(Statistics *stats, int wait)
{
    if (stats == NULL)
    {
        fprintf(stderr, "failed: stats != NULL\n");
        perror("Statistics pointer is NULL");
        return;
    }
    stats->totalWaitTime += wait;
}

void statsUpdateMaxQueue(Statistics *stats, int len)
{
    if (stats == NULL)
    {
        fprintf(stderr, "failed: stats != NULL\n");
        perror("Statistics pointer is NULL");
        return;
    }
    if (len > stats->maxQueueLen)
        stats->maxQueueLen = len;
}

void statsPrint(const Statistics *stats, const Logger *log)
{
    if (stats == NULL)
    {
        fprintf(stderr, "failed: stats != NULL\n");
        perror("Statistics pointer is NULL");
        return;
    }
    if (log == NULL)
    {
        fprintf(stderr, "failed: log != NULL\n");
        perror("Logger pointer is NULL");
        return;
    }
    char buf[512];
    int len = sprintf(buf,
                      "\n========== FINAL STATISTICS ==========\n"
                      "Ships arrived:              %d\n"
                      "Ships departed:             %d\n"
                      "Ships pending:              %d\n"
                      "Unloaded:                   %d\n"
                      "Total wait time:            %d ticks\n"
                      "Max queue length:           %d\n"
                      "========================================\n",
                      stats->shipsArrived, stats->shipsDeparted, stats->shipsPending,
                      stats->unloads, stats->totalWaitTime, stats->maxQueueLen);
    loggerWrite(log, buf, len);
}