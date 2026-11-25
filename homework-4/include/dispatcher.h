#ifndef DISPATCHER_H
#define DISPATCHER_H
/** @file dispatcher.h
 * @brief Dispatcher operation parameters for the process-based model.
 */

#include "resources.h"
/** @brief Dispatcher structure */
typedef struct
{
  Resources *res;    /**< Pointer to port resources. */
  int enterExitTime; /**< Base duration of entry or departure. */
  int unloadTime;    /**< Base duration of one cargo operation. */
  unsigned int seed; /**< Initial random generator state. */
} Dispatcher;
/**
 * @brief Initialize the dispatcher
 * @param disp       Pointer to the Dispatcher structure
 * @param res        Pointer to port resources
 * @param enterTime  Base time for entering/exiting
 * @param unloadTime Base time for unloading
 * @param seed       Random seed
 */
void dispInit(Dispatcher *disp, Resources *res, int enterTime, int unloadTime,
              unsigned int seed);
#endif
