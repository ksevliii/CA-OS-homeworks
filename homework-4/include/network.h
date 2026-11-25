#ifndef NETWORK_H
#define NETWORK_H

/** @file network.h
 * @brief Bounded TCP framing and nonblocking server connections.
 */

#include "process.h"
#include <arpa/inet.h>
#include <poll.h>
#include <sys/socket.h>

#define OBSERVERS 16                         /**< Maximum simultaneous observers; may reconnect freely. */
#define CONNECTIONS (ACTORS + OBSERVERS + 8) /**< Includes unregistered sockets. */
#define FRAME_SIZE 16384                     /**< Maximum text snapshot or packet buffer size. */
#define MESSAGE_SIZE 56                      /**< Fourteen explicitly encoded 32-bit integers. */
/** @brief TCP frame kinds; no compiler-dependent C structs cross the network. */
enum
{
  REGISTER = 1,
  WELCOME,
  COMMAND,
  RESPONSE,
  SNAPSHOT,
  FINISH,
  REJECT
};
/** @brief A bounded input frame and one partially sent output frame. */
typedef struct
{
  uint32_t snapshotSeen;                                       /**< Last snapshot revision queued for this observer. */
  int fd, actor, observer;                                     /**< Socket and registered role; actor=-1 before login. */
  unsigned char input[FRAME_SIZE + 8], output[FRAME_SIZE + 8]; /**< Frame buffers. */
  size_t received, sent, queued;                               /**< Progress retained across short TCP I/O. */
} NetPeer;
/** @brief One actor's server-owned command and last committed local state. */
typedef struct
{
  int peer, pending, ready, sent; /**< Connection and request/reply availability. */
  Message command, response;      /**< Saved command survives client disconnection. */
  ActorState state;               /**< State restored when the same actor reconnects. */
} NetActor;
/** @brief Process-local server/client state; allocated on the heap. */
struct Network
{
  int listener, failed, closing; /**< Endpoint status flags. */
  Engine *engine;                /**< Dispatcher context used while waiting for registration. */
  uint32_t snapshotRevision;     /**< Revision of the cached integrated view. */
  int snapshotLength;            /**< Cached display for observers joining late. */
  char snapshot[FRAME_SIZE];     /**< Latest integrated model view. */
  NetPeer peers[CONNECTIONS];    /**< Server sockets; clients use slot zero only. */
  NetActor actors[ACTORS];       /**< Pending work and committed participant states. */
  ActorState restored;           /**< State received by a reconnecting worker. */
};
/** @brief Allocate empty TCP buffers and initialize invalid descriptors. */
int networkInit(Transport *transport);
/** @brief Set nonblocking mode, close-on-exec and TCP_NODELAY. */
int socketOptions(int descriptor);
/** @brief Validate a numeric IPv4 address and a nonzero TCP port. */
int endpoint(const char *address, int port, struct sockaddr_in *socketAddress);
/** @brief Encode named Message fields as exactly MESSAGE_SIZE bytes. */
void encodeMessage(const Message *message, unsigned char *bytes);
/** @brief Decode a complete fixed-size wire message, rejecting oversized ints. */
int decodeMessage(Message *message, const unsigned char *bytes, size_t size);
/** @brief Queue one bounded frame; returns zero when output is occupied. */
int queueFrame(NetPeer *peer, int kind, const void *data, size_t size);
/** @brief Progress a partial send without blocking; -1 means broken socket. */
int flushFrame(NetPeer *peer);
/** @brief Read at most one frame; returns 1, 0 (incomplete), or -1 (closed/bad). */
int readFrame(NetPeer *peer, int *kind, const unsigned char **data, size_t *size);
/** @brief Close a peer and release its actor slot while retaining pending work. */
void disconnectPeer(Transport *transport, int index);
/** @brief Poll and service bounded TCP reads/writes without blocking on a peer. */
int networkPump(Transport *transport, int milliseconds);
/** @brief Validate and handle one server-side frame. */
int handleFrame(Transport *transport, int peer, int kind,
                const unsigned char *data, size_t size);
/** @brief Queue an unsent saved command for a connected actor. */
void sendPending(Transport *transport, int actor);
/** @brief Return true only while every required worker is connected. */
int allConnected(Transport *transport);
/** @brief Format and broadcast the latest state; slow observers skip updates. */
void publishState(Engine *engine, const char *status);
/** @brief Connect a role or observer and wait at most five seconds for welcome. */
int connectClient(Transport *transport, int actor, int role);
/** @brief Read one complete frame and expose the payload to a client caller. */
int clientFrame(Transport *transport, int *kind, const unsigned char **data,
                size_t *size, int milliseconds);
/** @brief Run the independent integrated display client. */
int observerMain(int argc, char **argv);
/** @brief Parse endpoint CLI values and install termination signal handlers. */
int clientArguments(int argc, char **argv, Transport *transport, int hasActor);
#endif
