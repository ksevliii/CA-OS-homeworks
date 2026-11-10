#ifndef PROCESS_H
#define PROCESS_H
/** @file process.h
 * @brief Linux process-based port simulation with four POSIX IPC variants.
 */
/** @brief Parse CLI arguments and run a dispatcher or an independent actor.
 * @param argc Number of arguments including the executable name.
 * @param argv Null-terminated argument vector.
 * @return 0 on normal completion, 2 on invalid input, 3 on IPC/runtime failure.
 */
int processMain(int argc, char **argv);
/** @brief Run a standalone participant program restricted to one model role.
 * @param argc Argument count, including the executable name.
 * @param argv Arguments: program, session name, actor index.
 * @param role Expected participant role: ship, tug, berth, crane or warehouse.
 * @return 0 on shutdown, 2 on invalid arguments/role, 3 on IPC failure.
 */
int processRoleMain(int argc, char **argv, int role);

/** @brief Shared data for the process-based simulation.
 * Value protocol, model state and interfaces for process modules.
 * Only the dispatcher writes Port. Participants own local operation state.
 */
#include "config.h"
#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <semaphore.h>
#include <signal.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>
#define ACTORS 82 /**< Maximum protocol slots, including the warehouse. */
/** @brief Participant roles; each instance has its own process. */
enum
{
  SHIP_ACTOR,
  TUG_ACTOR,
  BERTH_ACTOR,
  CRANE_ACTOR,
  WAREHOUSE_ACTOR
};
/** @brief Protocol commands and acknowledgements. */
enum
{
  HELLO,
  ARRIVAL,
  ENTER,
  MOOR,
  CARGO,
  EXIT,
  DRAIN,
  RELEASE,
  STOP
};
/** @brief Fixed-size command or reply copied through shared slots or FIFO. */
typedef struct
{
  int seq, action, ship, volume, duration, delay,
      pid;              /**< Protocol sequence, command, ship, portion, ticks, tick
                           delay and sender PID. */
  int warehouseData[8]; /**< DRAIN snapshot/results; never contains pointers. */
} Message;
/** @brief Require atomic-sized FIFO messages at compile time. */
_Static_assert(sizeof(Message) <= PIPE_BUF, "Message must fit in one atomic FIFO write");

/** @brief Single outstanding request and reply synchronized by two semaphores. */
typedef struct
{
  Message request, reply;     /**< Command and acknowledgement buffers. */
  sem_t requestSem, replySem; /**< Process-shared unnamed semaphores used only in fork-unnamed mode. */
} Slot;
/** @brief POSIX shared-memory mailboxes; contains no process-private pointers.
 */
typedef struct
{
  Slot slots[ACTORS]; /**< Per-participant mailboxes. */
} Shared;
/** @brief Published session identity, dispatcher PID and participant mapping.
 */
typedef struct
{
  int pid, count, mode, role[ACTORS],
      id[ACTORS]; /**< Protocol owner PID, participant count,
                     transport mode and role mapping. */
} Metadata;

/** @brief Process-local IPC handles and explicit cleanup ownership. */
typedef struct
{
  Metadata meta;  /**< Validated session metadata. */
  Shared *shared; /**< Locally mapped shared slots; NULL in FIFO mode. */
  int shmfd, claimfd, reqfd[ACTORS], repfd[ACTORS],
      initialized;                  /**< Memory, claim and FIFO descriptors; number of
                                       initialized unnamed slots. */
  sem_t *req[ACTORS], *rep[ACTORS]; /**< Local request and reply semaphore handles. */
  pid_t children[ACTORS],
      peers[ACTORS]; /**< Forked child PIDs and registered peer PIDs. */
  int childCount,
      owner; /**< Child count and permission to unlink session objects. */
  char session[25], dir[128],
      shmName[80]; /**< Bounded session name and derived IPC paths. */
} Transport;

/** @brief Dispatcher-owned outstanding operation and validated acknowledgement.
 */
typedef struct
{
  int actor, action, ship, due, seq, volume,
      ready;        /**< Slot, command, ship, completion tick, sequence, portion and
                       reply-ready flag. */
  Message response; /**< Completed participant reply, including warehouse
                       results. */
} Job;

/** @brief Dispatcher-owned jobs, resource mapping and operation accounting. */
typedef struct
{
  Port *port;       /**< Model state owned by the dispatcher. */
  Transport *ipc;   /**< Local transport handles. */
  Job jobs[ACTORS]; /**< At most one outstanding command per participant. */
  int actorShip[32], actorTug[16], actorBerth[16], actorCrane[16],
      actorWarehouse; /**< Role-to-slot mappings for ships, tugs, berths, cranes
                         and warehouse. */
  int assigned[32][16], remaining[32],
      portions[32];            /**< Crane sets, outstanding crane replies and cargo
                                  portions per ship. */
  int seq, failed, cargoMoved; /**< Next command sequence, failure flag and
                                  total completed cargo. */
  unsigned seed;               /**< Deterministic process-model generator state. */
} Engine;

/** @brief State owned exclusively by one participant process. */
typedef struct
{
  int phase;                          /**< Ship lifecycle: not arrived, present, departed. */
  int occupant;                       /**< Ship held by this berth, or -1 when free. */
  int lastSeq;                        /**< Most recent accepted sequence number. */
  unsigned long long completedVolume; /**< Cargo acknowledged by this crane. */
} ActorState;

/** @brief Validate command data and the role-specific lifecycle before
 * execution.
 * @param role Participant role.
 * @param state Local state, updated on acceptance.
 * @param message Command supplied by the dispatcher.
 * @return 1 for acceptance, 0 for a protocol error.
 */
int actorAccept(int role, ActorState *state, const Message *message);

/** @brief Commit the local role transition before acknowledging completion.
 * @param role Participant role.
 * @param state State owned by the participant.
 * @param message Successfully completed command.
 */
void actorComplete(int role, ActorState *state, Message *message);

/** @brief Signal flag; handlers only set it, ordinary code performs cleanup. */
extern volatile sig_atomic_t interrupted;

/** @brief Set the interruption flag using only async-signal-safe operations. */
void onSigint(int signalNumber);
/** @brief Sleep for the requested milliseconds; return early after a signal. */
void pauseMs(int milliseconds);
/** @brief Read elapsed-time clock seconds for startup and peer-health limits.
 */
double monotonic(void);
/** @brief Write all bytes, retrying interrupted and short writes. */
int fullWrite(int descriptor, const void *data, size_t size);
/** @brief Parse a complete bounded decimal token. */
int number(const char *text, int maximum, int *value);
/** @brief Write ready event text with a model timestamp; flag output errors. */
void event(Engine *engine, const char *text);
/** @brief Accept a nonempty session name of at most 24 letters, digits or
 * underscores. */
int validSession(const char *session);
/** @brief Zero transport ownership and initialize all descriptors to -1. */
void transportInit(Transport *transport, const char *session, int mode);
/** @brief Create exclusive session objects; partial creation is safe to close.
 */
int createTransport(Transport *transport);
/** @brief Validate metadata, attach IPC and claim exactly one participant slot.
 */
int attachTransport(Transport *transport, int actor);
/** @brief Close acquired handles and unlink only objects created by this owner.
 */
void closeTransport(Transport *transport);
/** @brief Publish a complete value message before posting its semaphore. */
int putMessage(Transport *transport, int actor, int reply, Message message);
/** @brief Block up to 100 ms for a message; return 1, 0 (timeout/signal), or
 * -1.
 * @param transport Attached transport, owned by the caller.
 * @param actor Valid participant slot.
 * @param reply Nonzero selects replies; zero selects requests.
 * @param message Receives the validated value message.
 * @return Message availability or protocol failure.
 */
int waitMessage(Transport *transport, int actor, int reply, Message *message);
/** @brief Try one message without blocking; return 1, 0 (not ready), or -1. */
int getMessage(Transport *transport, int actor, int reply, Message *message);
/** @brief Check process existence and termination of owned children. */
int peersAlive(Transport *transport);
/** @brief Wait for participant registration with bounded startup time. */
int waitReady(Transport *transport);
/** @brief Run a participant, validating commands and acknowledging operations.
 */
int workerLoop(Transport *transport, int actor);
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
/** @brief Signal participants, reap forked children and permit independent
 * cleanup. */
void shutdownActors(Transport *transport);
/** @brief Log termination reason, unfinished ships, totals and invariant
 * status. */
void summary(Engine *engine, const char *reason);
/** @brief Advance model ticks until completion, horizon, interruption or
 * failure. */
int runModel(Engine *engine);
/** @brief Format an event at its call site without a variadic function. */
#define PORT_EVENT(engine, ...)                                            \
  do                                                                       \
  {                                                                        \
    char eventText[512];                                                   \
    int eventLength = snprintf(eventText, sizeof(eventText), __VA_ARGS__); \
    if (eventLength < 0 || (size_t)eventLength >= sizeof(eventText))       \
      (engine)->failed = 1;                                                \
    else                                                                   \
      event((engine), eventText);                                          \
  } while (0)
#endif
