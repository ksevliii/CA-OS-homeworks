#ifndef PORT_H
#define PORT_H

/**
 * @file port.h
 * @brief Aggregator of all port entities and simulation state
 * All fields are protected by the mutex of the port
 */

#include <pthread.h>
#include "dispatcher.h"
#include "logger.h"
#include "resources.h"
#include "ship.h"
#include "statistics.h"
#include "utils.h"

#define EVENT_LINES 8 /**< Number of last events shown in the console */
#define EVENT_LEN 160 /**< Maximum length of an event line */

/** @brief Main Port structure containing all simulation state */
struct Port
{
  Resources res;                       /**< Port resources */
  Ship *ships;                         /**< Array of ships */
  int shipCount;                       /**< Number of ships */
  Dispatcher disp;                     /**< Port dispatcher */
  Statistics stats;                    /**< Simulation statistics */
  Logger *log;                         /**< Log file for events */
  int currentTime;                     /**< Current simulation time */
  int maxTime;                         /**< Maximum simulation time (0 - unlimited) */
  int delayMs;                         /**< Real duration of one tick in milliseconds */
  bool interrupted;                    /**< Flag for user interruption (Ctrl+C) */
  int stop;                            /**< Flag: all threads must finish */
  pthread_mutex_t mutex;               /**< Mutex protecting the port state */
  char events[EVENT_LINES][EVENT_LEN]; /**< Last events for the console */
  int eventCount;                      /**< Total number of events */
};

/**
 * @brief Initialize the port structure
 * @param port Pointer to the Port structure
 */
void portInit(Port *port);

/**
 * @brief Free allocated memory for the port
 * @param port Pointer to the Port structure
 */
void portFree(Port *port);

/**
 * @brief Allocate memory for the ships array
 * @param port      Pointer to the Port structure
 * @param shipCount Number of ships to allocate
 * @return true on success, false on allocation failure
 */
bool allocShips(Port *port, int shipCount);

/**
 * @brief Initialize a specific ship (its thread starts later, in simulationRun)
 * @param port      Pointer to the Port structure
 * @param idx       Index of the ship in the array
 * @param type      Ship type
 * @param cargoType Cargo type
 * @param volume    Initial cargo volume
 * @param arrTime   Arrival time
 */
void addShip(Port *port, int idx, ShipType type, CargoType cargoType,
             int volume, int arrTime);

/**
 * @brief Check if the simulation is finished (all ships departed)
 * Must be called with the port mutex locked
 * @param port Pointer to the Port structure
 * @return true if simulation is done, false otherwise
 */
bool isDone(const Port *port);

/**
 * @brief Check if the threads were asked to finish
 * @param port Pointer to the Port structure
 * @return true if stop flag is set
 */
bool portStopped(Port *port);

/**
 * @brief Wait on a semaphore and check the stop flag after waking up
 * @param port Pointer to the Port structure
 * @param sema Pointer to the semaphore
 * @return true if the thread may continue, false if it must finish
 */
bool portWait(Port *port, Sema *sema);

/**
 * @brief Write an event to the log file and to the list of last events
 * Time stamp is added automatically. Must be called with the port mutex locked
 * @param port Pointer to the Port structure
 * @param text Ready message (null-terminated string)
 */
void logEvent(Port *port, const char *text);

#endif
