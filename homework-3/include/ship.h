#ifndef SHIP_H
#define SHIP_H

/**
 * @file ship.h
 * @brief Ship model (each ship has its own process)
 */

#include "utils.h"

/** @brief Ship structure */
typedef struct
{
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
  int totalWait;       /**< Total waiting time */
  int waitSince;       /**< Time when the current waiting started */
  bool warehouseWait;  /**< Cargo is suspended: warehouse space or stock is
                          unavailable */
  bool loading;        /**< True for loading, false for unloading. */
  int cranesNeeded;    /**< Number of cranes reserved atomically (1..16). */
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

#endif
