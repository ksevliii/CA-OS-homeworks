#ifndef DISPATCHER_H
#define DISPATCHER_H

/**
 * @file dispatcher.h
 * @brief Port dispatcher for resource allocation (works in its own thread)
 * Resources are assigned by stages, so ships never hold one resource
 * while waiting for another one (no deadlock!!!!)
 */

#include "resources.h"
#include "ship.h"
#include "sync.h"
#include "utils.h"

/**
 * @brief Result of an unloading attempt
 */
typedef enum {
  WAIT,           /**< No crane available: ship continues waiting */
  WAIT_WAREHOUSE, /**< Warehouse full: unloading is suspended, ship waits */
  SUCCESS         /**< Unloading started successfully */
} UnloadResult;

/** @brief Dispatcher structure */
typedef struct {
  Resources *res;    /**< Pointer to port resources */
  int enterExitTime; /**< Base time for entering/exiting */
  int unloadTime;    /**< Base time for unloading */
  unsigned int seed; /**< Random seed for reproducibility */
  Sema wake;         /**< Posted by every thread that needs the dispatcher */
} Dispatcher;

/**
 * @brief Initialize the dispatcher
 * @param disp       Pointer to the Dispatcher structure
 * @param res        Pointer to port resources
 * @param enterTime  Base time for entering/exiting
 * @param unloadTime Base time for unloading
 * @param seed       Random seed
 */
void dispInit(Dispatcher *disp, Resources *res, int enterTime, int unloadTime,
              unsigned int seed);

/**
 * @brief Attempt to assign a tug and start the entering process
 * @param disp Pointer to the Dispatcher
 * @param ship Pointer to the Ship
 * @return true if successful, false if no tugs available
 */
bool tryEnter(Dispatcher *disp, Ship *ship);

/**
 * @brief Attempt to assign a compatible berth
 * @param disp Pointer to the Dispatcher
 * @param ship Pointer to the Ship
 * @return true if successful, false if no suitable berths available
 */
bool tryBerth(Dispatcher *disp, Ship *ship);

/**
 * @brief Attempt to assign a crane and warehouse space for unloading
 * One portion is unloaded: as much as the warehouse can take
 * @param disp Pointer to the Dispatcher
 * @param ship Pointer to the Ship
 * @return UnloadResult indicating the outcome
 */
UnloadResult tryUnload(Dispatcher *disp, Ship *ship);

/**
 * @brief Attempt to assign a tug and start the exiting process
 * @param disp Pointer to the Dispatcher
 * @param ship Pointer to the Ship
 * @return true if successful, false if no tugs available
 */
bool tryDepart(Dispatcher *disp, Ship *ship);

/**
 * @brief Thread of the dispatcher: sleeps on a semaphore, wakes up when
 * somebody needs resources and assigns them to waiting ships
 * @param arg Pointer to the Port
 * @return NULL
 */
void *dispatcherThread(void *arg);

#endif
