#include "network.h"

int testFrames(void)
{
  int sockets[2];
  if (socketpair(AF_UNIX, SOCK_STREAM, 0, sockets) < 0)
    return 0;
  int flags = fcntl(sockets[1], F_GETFL, 0);
  fcntl(sockets[1], F_SETFL, flags | O_NONBLOCK);
  NetPeer sender = {.fd = sockets[0]}, receiver = {.fd = sockets[1]};
  Message original = {.seq = 7, .action = CARGO, .ship = 0, .volume = 8, .duration = 3};
  unsigned char bytes[MESSAGE_SIZE];
  encodeMessage(&original, bytes);
  int success = queueFrame(&sender, COMMAND, bytes, sizeof(bytes));
  int kind = 0, result = 0;
  const unsigned char *data = NULL;
  size_t size = 0;
  for (size_t index = 0; index < sender.queued && success; ++index)
  {
    success = write(sockets[0], sender.output + index, 1) == 1;
    result = readFrame(&receiver, &kind, &data, &size);
    success = success && result == (index + 1 == sender.queued ? 1 : 0);
  }
  Message decoded;
  success = success && kind == COMMAND && decodeMessage(&decoded, data, size) &&
            decoded.seq == 7 && decoded.volume == 8;
  unsigned char combined[2 * (MESSAGE_SIZE + 8)];
  memcpy(combined, sender.output, sender.queued);
  memcpy(combined + sender.queued, sender.output, sender.queued);
  success = success && write(sockets[0], combined, 2 * sender.queued) == (ssize_t)(2 * sender.queued);
  for (int frame = 0; frame < 2 && success; ++frame)
    success = readFrame(&receiver, &kind, &data, &size) == 1 && kind == COMMAND;
  close(sockets[0]);
  close(sockets[1]);
  return success;
}
