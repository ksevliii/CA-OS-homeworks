#ifndef STATISTICS_H
#define STATISTICS_H

/**
 * @file statistics.h
 * @brief Collection and output of simulation statistics
 */

#include "logger.h"
#include "utils.h"

/** @brief Statistics structure */
typedef struct
{
  int shipsArrived;  /**< Total ships arrived */
  int shipsDeparted; /**< Ships successfully departed */
  int shipsPending;  /**< Ships remaining in port */
  int totalWaitTime; /**< Total waiting time (ticks) */
  int maxQueueLen;   /**< Maximum queue length */
  int unloads;       /**< Ships departed unloaded */
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
 * @brief Add waiting time to the total
 * @param stats Pointer to the Statistics structure
 * @param wait  Time to add
 */
void statsAddWait(Statistics *stats, int wait);

/**
 * @brief Update maximum queue length if the current length is greater
 * @param stats Pointer to the Statistics structure
 * @param len   Current queue length
 */
void statsUpdateMaxQueue(Statistics *stats, int len);

/**
 * @brief Print final statistics to the log
 * @param stats Pointer to the Statistics structure
 * @param log   Pointer to the Logger
 */
void statsPrint(const Statistics *stats, const Logger *log);

#endif