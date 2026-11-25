#ifndef STATISTICS_H
#define STATISTICS_H

/**
 * @file statistics.h
 * @brief Collection and output of simulation statistics
 * Changed only by the dispatcher process
 */

#include "logger.h"
#include "utils.h"

/** @brief Statistics structure */
typedef struct
{
  int shipsArrived;  /**< Total ships arrived */
  int shipsDeparted; /**< Ships successfully departed */
  int shipsPending;  /**< Ships that have not completed their processing */
  int shipsPartial;  /**< Unfinished ships with some cargo already processed */
  int maxQueueLen;   /**< Maximum queue length */
  int unloads;       /**< Finished cargo portions */
} Statistics;

/**
 * @brief Initialize statistics to zero
 * @param stats Pointer to the Statistics structure
 */
void statsInit(Statistics *stats);

/**
 * @brief Record a ship arrival
 * @param stats Pointer to the Statistics structure
 */
void statsRecordArrival(Statistics *stats);

/**
 * @brief Record a ship departure
 * @param stats Pointer to the Statistics structure
 */
void statsRecordDeparture(Statistics *stats);

/**
 * @brief Record an unload event
 * @param stats Pointer to the Statistics structure
 */
void statsRecordUnload(Statistics *stats);

/**
 * @brief Update maximum queue length if the current length is greater
 * @param stats Pointer to the Statistics structure
 * @param len   Current queue length
 */
void statsUpdateMaxQueue(Statistics *stats, int len);

#endif
