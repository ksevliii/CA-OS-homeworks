#ifndef QUEUE_H
#define QUEUE_H

/**
 * @file queue.h
 * @brief Eevent queue sorted by simulation time
 *
 * Implemented as a dynamic array with insertion sort on push
 * Events with the same time are processed in FIFO order
 */

#include "utils.h"

/**
 * @brief Event queue structure
 */
typedef struct
{
  Event *events; /**< Dynamic array of events */
  int count;     /**< Current number of events */
  int capacity;  /**< Allocated capacity (for realloc) */
} EventQueue;

/**
 * @brief Initialize the event queue
 * @param queue    Pointer to the queue
 * @param capacity Initial capacity of the events array
 */
void queueInit(EventQueue *queue, int capacity);

/**
 * @brief Free the queue memory
 * @param queue Pointer to the queue
 */
void queueFree(EventQueue *queue);

/**
 * @brief Push an event into the queue with time-based sorting
 * @param queue  Pointer to the queue
 * @param type   Event type
 * @param time   Simulation time of occurrence
 * @param shipId Ship identifier
 * @return true on success, false on realloc failure
 */
bool queuePush(EventQueue *queue, EventType type, int time, int shipId);

/**
 * @brief Pop an event if its time <= currentTime
 * @param queue      Pointer to the queue
 * @param popEvent   Output parameter for the popped event
 * @param currentTime Current simulation time
 * @return true if an event was popped, false otherwise
 */
bool queuePop(EventQueue *queue, Event *popEvent, int currentTime);

/**
 * @brief Check if the queue is empty
 * @param queue Pointer to the queue
 * @return true if the queue is empty
 */
bool queueEmpty(const EventQueue *queue);

#endif