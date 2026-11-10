#ifndef PORT_H
#define PORT_H

/**
 * @file port.h
 * @brief Aggregator of all port entities and simulation state
 * Only the dispatcher process modifies this state.
 */

#include "dispatcher.h"
#include "logger.h"
#include "resources.h"
#include "ship.h"
#include "statistics.h"
#include "utils.h"

/** @brief Number of recent events retained for the console dashboard. */
#define EVENT_LINES 8
/** @brief Maximum stored length of a dashboard event, including the terminator.
 */
#define EVENT_TEXT_LENGTH 600

/** @brief Main Port structure containing all simulation state */
struct Port
{
  bool uiActive;  /**< True while the dispatcher renders the terminal dashboard.
                   */
  int eventCount; /**< Number of stored recent events, bounded by EVENT_LINES.
                   */
  int eventNext;  /**< Circular buffer position for the next event. */
  char events[EVENT_LINES]
             [EVENT_TEXT_LENGTH]; /**< Recent timestamped events. */
  Resources res;                  /**< Port resources */
  Ship *ships;                    /**< Array of ships */
  int shipCount;                  /**< Number of ships */
  Dispatcher disp;                /**< Port dispatcher */
  Statistics stats;               /**< Simulation statistics */
  Logger *log;                    /**< Log file for events */
  int currentTime;                /**< Current simulation time */
  int maxTime;                    /**< Maximum simulation time (0 - unlimited) */
  int delayMs;                    /**< Real duration of one tick in milliseconds */
  int strategy;                   /**< Dispatch order: 0 ID, 1 arrival FCFS, 2 shortest cargo
                                     first. */
  int reservation;                /**< 0 stage allocation, 1 reserve berth with entry tug. */
  int moorTime;                   /**< Duration of mooring in simulation ticks. */
  int warehousePeriod;            /**< Interval between warehouse supply/export actions. */
  int warehouseOut;               /**< Maximum export volume per warehouse action. */
  int supply[2];                  /**< Periodic external cargo supply for loading. */
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
 * @brief Initialize a specific ship; its process is started later
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
 * @param port Pointer to the Port structure
 * @return true if simulation is done, false otherwise
 */
bool isDone(const Port *port);

#endif
