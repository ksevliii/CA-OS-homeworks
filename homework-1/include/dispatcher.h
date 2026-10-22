#ifndef DISPATCHER_H
#define DISPATCHER_H

/**
 * @file dispatcher.h
 * @brief Port dispatcher for resource allocation
 */

#include "utils.h"
#include "resources.h"
#include "ship.h"
#include "queue.h"

/**
 * @brief Result of an unloading attempt
 */
typedef enum
{
  WAIT,      /**< No crane available: ship continues waiting */
  TO_DEPART, /**< Warehouse full: ship departs unloaded */
  SUCCESS    /**< Unloading started successfully */
} UnloadResult;

/** @brief Dispatcher structure */
typedef struct
{
  Resources *res;    /**< Pointer to port resources */
  EventQueue *queue; /**< Pointer to the event queue */
  int currentTime;   /**< Current simulation time */
  int enterExitTime; /**< Base time for entering/exiting */
  int unloadTime;    /**< Base time for unloading */
  unsigned int seed; /**< Random seed for reproducibility */
} Dispatcher;

/**
 * @brief Initialize the dispatcher
 * @param disp       Pointer to the Dispatcher structure
 * @param res        Pointer to port resources
 * @param queue      Pointer to the event queue
 * @param enterTime  Base time for entering/exiting
 * @param unloadTime Base time for unloading
 * @param seed       Random seed
 */
void dispInit(Dispatcher *disp, Resources *res, EventQueue *queue,
              int enterTime, int unloadTime, unsigned int seed);

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
 * @brief Process a completed event (ENTER_DONE, UNLOAD_DONE, EXIT_DONE)
 * @param disp      Pointer to the Dispatcher
 * @param ships     Array of ships
 * @param shipCount Total number of ships
 * @param event     Pointer to the event to process
 */
void inEvent(Dispatcher *disp, Ship *ships, int shipCount, const Event *event);

#endif