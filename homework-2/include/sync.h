#ifndef SYNC_H
#define SYNC_H

/**
 * @file sync.h
 * @brief Counting semaphore used by the threads of the simulation
 * In port_sim it is a POSIX semaphore (sem_t);
 * In port_sim_cond (built with -DCOND) the same interface is
 * implemented with a mutex and a condition variable
 */

#include <pthread.h>
#include <stdbool.h>
#ifndef COND
#include <semaphore.h>
#endif

#ifdef COND
/** @brief Semaphore replacement: mutex + condition variable + counter */
typedef struct {
  pthread_mutex_t mutex; /**< Protects count */
  pthread_cond_t cond;   /**< Signalled by semaPost */
  int count;             /**< Current value */
} Sema;
#else
/** @brief POSIX semaphore */
typedef struct {
  sem_t sem; /**< Underlying semaphore */
} Sema;
#endif

/**
 * @brief Initialize the semaphore
 * @param sema  Pointer to the semaphore
 * @param value Initial value
 * @return true on success, false on error
 */
bool semaInit(Sema *sema, int value);

/**
 * @brief Destroy the semaphore
 * @param sema Pointer to the semaphore
 */
void semaDestroy(Sema *sema);

/**
 * @brief Increment the semaphore and wake one waiting thread
 * @param sema Pointer to the semaphore
 */
void semaPost(Sema *sema);

/**
 * @brief Decrement the semaphore, wait while its value is zero
 * @param sema Pointer to the semaphore
 * @return true on success, false on error
 */
bool semaWait(Sema *sema);

#endif
