#include "statistics.h"
#include "utils.h"
#include <stdio.h>
#include <unistd.h>

void statsInit(Statistics *stats)
{
  if (stats == NULL)
  {
    fprintf(stderr, "failed: stats != NULL\n");
    perror("Statistics pointer is NULL");
    return;
  }
  stats->shipsArrived = stats->shipsDeparted = stats->shipsPending = 0;
  stats->shipsPartial = 0;
  stats->maxQueueLen = stats->unloads = 0;
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
