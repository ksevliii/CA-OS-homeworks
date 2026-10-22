#ifndef UTILS_H
#define UTILS_H

/**
 * @file utils.h
 * @brief Base utils and types for the port simulation
 */

#include <stdbool.h>
#include <stdio.h>

/**
 \brief Macro for soft condition checking
 Checks the condition. If false, prints an error message to stderr, calls
 perror() with the provided message, and returns from the caller
 \param cond Condition to check
 \param msg Error message
 \param ret Value to return if false
 */
#define SOFT_ASSERT(cond, msg, ret)           \
  do                                          \
  {                                           \
    if (!(cond))                              \
    {                                         \
      fprintf(stderr, "failed: %s\n", #cond); \
      perror((msg));                          \
      return (ret);                           \
    }                                         \
  } while (0)

/** @brief Ship types */
typedef enum
{
  SMALL = 0,
  MEDIUM = 1,
  LARGE = 2
} ShipType;

/** @brief Cargo types */
typedef enum
{
  CONTAINER = 0,
  LIQUID = 1
} CargoType;

/** @brief Ship states */
typedef enum
{
  NOT_ARRIVED,                 /**< Has not yet arrived (inactive) */
  QUEUED,                      /**< Waiting for entry tug */
  ENTERING,                    /**< Entering port with tug */
  WAITING_BERTH,               /**< Waiting for a free berth */
  WAITING_CRANE_AND_WAREHOUSE, /**< Waiting for crane and warehouse */
  UNLOADING,                   /**< Unloading process */
  WAITING_DEPART,              /**< Waiting for departure tug */
  EXITING,                     /**< Exiting port with tug */
  DEPARTED                     /**< Ship has left the port or unactive yet */
} ShipState;

/** @brief Event types in the queue */
typedef enum
{
  ARRIVE,      /**< Ship arrived at port */
  ENTER_DONE,  /**< Entry completed, tug released */
  UNLOAD_DONE, /**< Unloading completed, crane released */
  EXIT_DONE    /**< Exit completed, ship departed */
} EventType;

/** @brief Event record in the queue */
typedef struct
{
  EventType type; /**< Event type */
  int time;       /**< Time of occurrence */
  int shipId;     /**< Ship id */
} Event;

#endif