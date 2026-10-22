#ifndef PORT_H
#define PORT_H

/**
 * @file port.h
 * @brief Aggregator of all port entities and simulation state
 */

#include "dispatcher.h"
#include "queue.h"
#include "resources.h"
#include "ship.h"
#include "statistics.h"
#include "utils.h"

/** @brief Main Port structure containing all simulation state */
typedef struct
{
  Resources res;    /**< Port resources */
  Ship *ships;      /**< Array of ships */
  int shipCount;    /**< Number of ships */
  EventQueue queue; /**< Event queue */
  Dispatcher disp;  /**< Port dispatcher */
  Statistics stats; /**< Simulation statistics */
  int currentTime;  /**< Current simulation time */
  int maxTime;      /**< Maximum simulation time */
  bool interrupted; /**< Flag for user interruption (Ctrl+C) */
} Port;

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
 * @brief Initialize a specific ship and push its ARRIVE event to the queue
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
 * @brief Check if the simulation is finished (all ships departed and queue is empty)
 * @param port Pointer to the Port structure
 * @return true if simulation is done, false otherwise
 */
bool isDone(const Port *port);

#endif