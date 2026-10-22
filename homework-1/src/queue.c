#include <stdlib.h>
#include "queue.h"
#include "utils.h"

void queueInit(EventQueue *queue, int capacity)
{
    if (queue == NULL)
    {
        fprintf(stderr, "failed: queue != NULL\n");
        perror("EventQueue pointer is NULL");
        return;
    }
    if (capacity <= 0)
    {
        fprintf(stderr, "failed: capacity > 0\n");
        perror("Initial capacity <= 0");
        return;
    }
    queue->events = calloc((size_t)capacity, sizeof(Event));
    SOFT_ASSERT(queue->events != NULL, "queueInit: calloc failed", (void)0);
    queue->count = 0;
    queue->capacity = capacity;
}

void queueFree(EventQueue *queue)
{
    if (queue == NULL)
    {
        fprintf(stderr, "failed: queue != NULL\n");
        perror("EventQueue pointer is NULL");
        return;
    }
    free(queue->events);
    queue->events = NULL;
    queue->count = 0;
    queue->capacity = 0;
}

bool queuePush(EventQueue *queue, EventType type, int time, int shipId)
{
    SOFT_ASSERT(queue != NULL, "EventQueue pointer is NULL", false);
    if (queue->count >= queue->capacity)
    {
        int newCapacity = queue->capacity * 2;
        Event *newEvents = realloc(queue->events, (size_t)newCapacity * sizeof(Event));
        SOFT_ASSERT(newEvents != NULL, "queuePush: realloc failed", false);
        queue->events = newEvents;
        queue->capacity = newCapacity;
    }
    Event eve = {
        .type = type,
        .time = time,
        .shipId = shipId};
    int idx = queue->count++;
    while (idx > 0 && queue->events[idx - 1].time > eve.time)
    {
        queue->events[idx] = queue->events[idx - 1];
        --idx;
    }
    queue->events[idx] = eve;
    return true;
}

bool queuePop(EventQueue *queue, Event *popEvent, int currentTime)
{
    SOFT_ASSERT(queue != NULL, "EventQueue pointer is NULL", false);
    SOFT_ASSERT(popEvent != NULL, "Output Event pointer is NULL", false);
    if (queue->count == 0 || queue->events[0].time > currentTime)
        return false;
    *popEvent = queue->events[0];
    for (int idx = 0; idx < queue->count - 1; ++idx)
    {
        queue->events[idx] = queue->events[idx + 1];
    }
    queue->count--;
    return true;
}

bool queueEmpty(const EventQueue *queue)
{
    SOFT_ASSERT(queue != NULL, "EventQueue pointer is NULL", true);
    return queue->count == 0;
}