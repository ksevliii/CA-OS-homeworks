#include "network.h"

void encodeMessage(const Message *message, unsigned char *bytes)
{
  int values[14] = {message->seq, message->action, message->ship,
                    message->volume, message->duration, message->delay};
  for (int index = 0; index < 8; ++index)
    values[6 + index] = message->warehouseData[index];
  for (int index = 0; index < 14; ++index)
  {
    uint32_t value = htonl((uint32_t)values[index]);
    memcpy(bytes + index * 4, &value, 4);
  }
}

int decodeMessage(Message *message, const unsigned char *bytes, size_t size)
{
  if (size != MESSAGE_SIZE)
    return 0;
  int values[14];
  for (int index = 0; index < 14; ++index)
  {
    uint32_t value;
    memcpy(&value, bytes + index * 4, 4);
    value = ntohl(value);
    if (value > INT_MAX && value != UINT32_MAX)
      return 0;
    values[index] = value == UINT32_MAX ? -1 : (int)value;
  }
  *message = (Message){.seq = values[0], .action = values[1], .ship = values[2], .volume = values[3], .duration = values[4], .delay = values[5]};
  for (int index = 0; index < 8; ++index)
    message->warehouseData[index] = values[6 + index];
  return 1;
}

int queueFrame(NetPeer *peer, int kind, const void *data, size_t size)
{
  if (peer->fd < 0 || peer->queued || size > FRAME_SIZE)
    return 0;
  uint32_t header[2] = {htonl((uint32_t)kind), htonl((uint32_t)size)};
  memcpy(peer->output, header, 8);
  if (size)
    memcpy(peer->output + 8, data, size);
  peer->sent = 0;
  peer->queued = size + 8;
  return 1;
}

int flushFrame(NetPeer *peer)
{
  while (peer->sent < peer->queued)
  {
    ssize_t count = send(peer->fd, peer->output + peer->sent,
                         peer->queued - peer->sent, MSG_NOSIGNAL);
    if (count < 0 && errno == EINTR)
      continue;
    if (count < 0 && (errno == EAGAIN || errno == EWOULDBLOCK))
      return 0;
    if (count <= 0)
      return -1;
    peer->sent += (size_t)count;
  }
  peer->sent = peer->queued = 0;
  return 1;
}

int readFrame(NetPeer *peer, int *kind, const unsigned char **data, size_t *size)
{
  size_t target = 8;
  for (;;)
  {
    if (peer->received >= 8)
    {
      uint32_t header[2];
      memcpy(header, peer->input, 8);
      *kind = (int)ntohl(header[0]);
      *size = ntohl(header[1]);
      if (*kind < REGISTER || *kind > REJECT || *size > FRAME_SIZE)
        return -1;
      target = *size + 8;
      if (peer->received == target)
      {
        *data = peer->input + 8;
        peer->received = 0;
        return 1;
      }
    }
    ssize_t count = recv(peer->fd, peer->input + peer->received,
                         target - peer->received, 0);
    if (count < 0 && errno == EINTR)
      continue;
    if (count < 0 && (errno == EAGAIN || errno == EWOULDBLOCK))
      return 0;
    if (count <= 0)
      return -1;
    peer->received += (size_t)count;
  }
}
