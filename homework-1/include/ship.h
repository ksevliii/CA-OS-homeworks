#ifndef SHIP_H
#define SHIP_H

/**
 * @file ship.h
 * @brief Ship entity model
 */

#include "utils.h"

/** @brief Ship structure */
typedef struct
{
  int id;              /**< Unique id */
  ShipType type;       /**< Ship type */
  CargoType cargoType; /**< Cargo type */
  int cargoVolume;     /**< Current cargo volume */
  int arrivalTime;     /**< Arrival time */
  ShipState state;     /**< Current state */
  int tugId;           /**< Assigned tug (-1 if none) */
  int berthId;         /**< Assigned berth (-1 if none) */
  int craneId;         /**< Assigned crane (-1 if none) */
  int eventTime;       /**< Time of current operation */
  int totalWait;       /**< Total waiting time */
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