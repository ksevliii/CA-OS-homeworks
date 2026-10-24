#ifndef SHIP_H
#define SHIP_H

/**
 * @file ship.h
 * @brief Ship model (every ship works in its own thread)
 */

#include "sync.h"
#include "utils.h"

/** @brief Ship structure */
typedef struct {
  int id;              /**< Unique id */
  ShipType type;       /**< Ship type */
  CargoType cargoType; /**< Cargo type */
  int cargoVolume;     /**< Current cargo volume */
  int startVolume;     /**< Cargo volume at the arrival */
  int arrivalTime;     /**< Arrival time */
  ShipState state;     /**< Current state */
  int tugId;           /**< Assigned tug (-1 if none) */
  int berthId;         /**< Assigned berth (-1 if none) */
  int craneId;         /**< Assigned crane (-1 if none) */
  int eventTime;       /**< Time when the current tug/crane operation ends */
  int totalWait;       /**< Total waiting time */
  int waitSince;       /**< Time when the current waiting started */
  bool warehouseWait;  /**< Unloading is suspended: warehouse is full */
  Port *port;          /**< Pointer to the port (for the thread) */
  Sema granted;        /**< Posted when the requested stage is finished */
} Ship;

/**
 * @brief Initialize a ship
 */
void shipInit(Ship *ship, int id, ShipType type, CargoType cargoType,
              int volume, int arrTime);

/**
 * @brief Get a human-readable state name
 */
const char *shipStateName(ShipState state);

/**
 * @brief Thread of a ship: arrival, entering, berth, unloading, departure
 * @param arg Pointer to the Ship
 * @return NULL
 */
void *shipThread(void *arg);

#endif
