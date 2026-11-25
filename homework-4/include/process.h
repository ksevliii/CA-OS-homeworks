#ifndef PROCESS_H
#define PROCESS_H
/** @file process.h
 * @brief TCP port simulation; dispatcher and independent participants.
 */
/** @brief Parse CLI arguments and run a dispatcher or an independent actor.
 * @param argc Number of arguments including the executable name.
 * @param argv Null-terminated argument vector.
 * @return 0 on normal completion, 2 on invalid input, 3 on TCP/runtime failure.
 */
int processMain(int argc, char **argv);
/** @brief Run a standalone participant program restricted to one model role.
 * @param argc Argument count, including the executable name.
 * @param argv Arguments: program, IPv4 address, port and actor index.
 * @param role Expected participant role: ship, tug, berth, crane or warehouse.
 * @return 0 on shutdown, 2 on invalid arguments/role, 3 on TCP failure.
 */
int processRoleMain(int argc, char **argv, int role);

/** @brief Shared data for the process-based simulation.
 * Value messages, model state and interfaces for TCP process modules.
 * Only the dispatcher writes Port. Participants own local operation state.
 */
#include "config.h"
#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <signal.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
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
  RELEASE
};
/** @brief Command or reply encoded as network-order integers over TCP. */
typedef struct
{
  int seq, action, ship, volume, duration, delay; /**< Sequence, command, ship, portion, ticks and tick delay. */
  int warehouseData[8];                           /**< DRAIN snapshot/results; never contains pointers. */
} Message;
/** @brief Server-owned participant count, roles and resource mapping.
 */
typedef struct
{
  int count, role[ACTORS],
      id[ACTORS]; /**< Participant count and role mapping. */
} Metadata;

/** @brief Private TCP connection state, defined in network.h. */
typedef struct Network Network;
/** @brief Local TCP endpoint and model participant mapping. */
typedef struct
{
  Metadata meta;                /**< Server-side role and resource mapping. */
  Network *network;             /**< Owned connection buffers; never sent as a pointer. */
  int owner, actor, role, port; /**< Server flag, client slot, role and TCP port. */
  char address[64];             /**< Numeric IPv4 address from the command line. */
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
/** @brief Read elapsed-time clock seconds for connection and shutdown limits.
 */
double monotonic(void);
/** @brief Write all bytes, retrying interrupted and short writes. */
int fullWrite(int descriptor, const void *data, size_t size);
/** @brief Parse a complete bounded decimal token. */
int number(const char *text, int maximum, int *value);
/** @brief Write ready event text with a model timestamp; flag output errors. */
void event(Engine *engine, const char *text);
/** @brief Initialize a local numeric IPv4 endpoint. */
void transportInit(Transport *transport, const char *address, int port);
/** @brief Bind and listen on the server endpoint. */
int createTransport(Transport *transport);
/** @brief Connect and register one participant at the TCP server. */
int attachTransport(Transport *transport, int actor);
/** @brief Close sockets and free locally owned connection buffers. */
void closeTransport(Transport *transport);
/** @brief Save a server command or queue a client reply for TCP transmission. */
int putMessage(Transport *transport, int actor, int reply, Message message);
/** @brief Try one message without blocking; return 1, 0 (not ready), or -1. */
int getMessage(Transport *transport, int actor, int reply, Message *message);
/** @brief Check that the local TCP event loop has not failed. */
int peersAlive(Transport *transport);
/** @brief Wait for participant registration with bounded startup time. */
int waitReady(Transport *transport);
/** @brief Run a participant, validating commands and acknowledging operations.
 */
int workerLoop(Transport *transport, int actor);
#include "model.h"
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
