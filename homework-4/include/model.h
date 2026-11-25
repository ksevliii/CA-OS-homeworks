#ifndef MODEL_H
#define MODEL_H
/** @file model.h
 * @brief Dispatcher model operations.
 */

#include "process.h"

/** @brief Assign one command and record its sequence number and model
 * completion tick. */
int startJob(Engine *engine, int actor, int action, int ship, int volume,
             int duration);

/** @brief Advance the unsigned LCG and return base plus at most base/5 ticks.
 */
int randomDelay(Engine *engine, int base);

/** @brief Find a free tug compatible with the ship without reserving it. */
int selectTug(Engine *engine, Ship *ship);

/** @brief Record the waiting state and tick, then log the resource request. */
void setWaiting(Engine *engine, Ship *ship, ShipState state, const char *what);

/** @brief Claim the selected tug and submit the entry or exit operation. */
void assignTug(Engine *engine, Ship *ship, int tug, int action);

/** @brief Commit one acknowledged operation and release or request its
 * resources. */
void finishJob(Engine *engine, Job job);

/** @brief Validate resource ownership, compatibility, cargo and capacity. */
int checkInvariants(Engine *engine);

/** @brief Try eligible ships in configured order without partially claiming a
 * crane set. */
int dispatch(Engine *engine);

/** @brief Create the role-to-slot mapping for every model participant. */
void buildActors(Engine *engine);

/** @brief Send a TCP finish notification and close connections after bounded output draining. */
void shutdownActors(Transport *transport);

/** @brief Log termination reason, unfinished ships, totals and invariant
 * status. */
void summary(Engine *engine, const char *reason);

/** @brief Advance model ticks until completion, horizon, interruption or
 * failure. */
int runModel(Engine *engine);

/** @brief Advance one eligible ship through its resource stages. */
int dispatchShip(Engine *engine, Ship *ship);
#endif
