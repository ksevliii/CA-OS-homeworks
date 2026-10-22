#ifndef RESOURCES_H
#define RESOURCES_H

/**
 * @file resources.h
 * @brief Port resources: tugs, berths, cranes, warehouse
 * Each resource has a compatibility (compatibility[]),
 * configured from the config file.
 */

#include "utils.h"

/** @brief Tug (resource with a busy flag) */
typedef struct
{
  int id;    /**< Tug identifier */
  bool busy; /**< Busy flag */
} Tug;

/**
 * @brief Berth (resource with ship-type compatibility)
 * compatibility[SMALL], compatibility[MEDIUM], compatibility[LARGE] - bool variables
 */
typedef struct
{
  int id;                /**< Berth identifier */
  int shipId;            /**< ID of occupying ship (-1 if free) */
  bool compatibility[3]; /**< Compatibility with ship types */
} Berth;

/**
 * @brief Crane (resource with cargo-type compatibility)
 * compatibility[CONTAINER], compatibility[LIQUID] - bool variables
 */
typedef struct
{
  int id;                /**< Crane identifier */
  bool busy;             /**< Busy flag */
  bool compatibility[2]; /**< Compatibility with cargo types */
} Crane;

/** @brief Warehouse (resource with limited capacity) */
typedef struct
{
  int capacity; /**< Maximum capacity */
  int used;     /**< Currently used capacity */
} Warehouse;

/** @brief Aggregator of all port resources */
typedef struct
{
  Tug *tugs;           /**< Array of tugs */
  int tugCount;        /**< Number of tugs */
  Berth *berths;       /**< Array of berths */
  int berthCount;      /**< Number of berths */
  Crane *cranes;       /**< Array of cranes */
  int craneCount;      /**< Number of cranes */
  Warehouse warehouse; /**< Warehouse resource */
} Resources;

/**
 * @brief Initialize resources to empty state
 * @param res Pointer to the Resources structure
 */
void resInit(Resources *res);

/**
 * @brief Free allocated memory for resources
 * @param res Pointer to the Resources structure
 */
void resFree(Resources *res);

/**
 * @brief Allocate and initialize resources based on config
 * @param res              Pointer to the Resources structure
 * @param tugCount         Number of tugs
 * @param berthCount       Number of berths
 * @param craneCount       Number of cranes
 * @param warehouseCapacity Maximum warehouse capacity
 * @return true on success, false on allocation failure
 */
bool resAlloc(Resources *res, int tugCount, int berthCount,
              int craneCount, int warehouseCapacity);

/**
 * @brief Set berth compatibility for a specific ship type
 * @param res          Pointer to the Resources structure
 * @param berthId      Berth identifier
 * @param shipType     Ship type to set compatibility
 * @param isCompatible true if compatible, false otherwise
 */
void setBerthCompatibility(Resources *res, int berthId,
                           ShipType shipType, bool isCompatible);

/**
 * @brief Set crane compatibility for a specific cargo type
 * @param res         Pointer to the Resources structure
 * @param craneId     Crane identifier
 * @param cargoType   Cargo type to set compatibility for
 * @param isCompatible true if compatible, false otherwise
 */
void setCraneCompatibility(Resources *res, int craneId,
                           CargoType cargoType, bool isCompatible);

/**
 * @brief Allocate a free tug
 * @param res Pointer to the Resources structure
 * @return Tug ID on success, -1 if all tugs are busy
 */
int takeTug(Resources *res);

/**
 * @brief Release a tug
 * @param res    Pointer to the Resources structure
 * @param tugId  ID of the tug to release
 */
void freeTug(Resources *res, int tugId);

/**
 * @brief Allocate a compatible free berth for a ship
 * @param res      Pointer to the Resources structure
 * @param shipType Type of the ship
 * @param shipId   ID of the ship
 * @return Berth ID on success, -1 if no suitable berth is available
 */
int takeBerth(Resources *res, ShipType shipType, int shipId);

/**
 * @brief Release a berth
 * @param res      Pointer to the Resources structure
 * @param berthId  ID of the berth to release
 */
void freeBerth(Resources *res, int berthId);

/**
 * @brief Allocate a compatible free crane for a cargo type
 * @param res        Pointer to the Resources structure
 * @param cargoType  Type of the cargo
 * @return Crane ID on success, -1 if no suitable crane is available
 */
int takeCrane(Resources *res, CargoType cargoType);

/**
 * @brief Release a crane
 * @param res      Pointer to the Resources structure
 * @param craneId  ID of the crane to release
 */
void freeCrane(Resources *res, int craneId);

/**
 * @brief Add cargo volume to the warehouse
 * @param res          Pointer to the Resources structure
 * @param cargoVolume  Volume to add
 */
void takeWarehouse(Resources *res, int cargoVolume);

#endif