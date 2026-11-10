#include "config.h"
#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

static int parseInt(const char **cursor)
{ // парсинг целых чисел из файла
  while (isspace((unsigned char)**cursor))
    ++*cursor;
  if (!isdigit((unsigned char)**cursor))
    return -1;
  int value = 0;
  while (isdigit((unsigned char)**cursor))
  {
    int digit = *(*cursor)++ - '0';
    if (value > (1000000 - digit) / 10)
      return -1;
    value = value * 10 + digit;
  }
  if (**cursor && !isspace((unsigned char)**cursor))
    return -1;
  return value;
}

static bool configError(const char *path, const char *begin, const char *cursor,
                        const char *reason)
{
  size_t line = 1;
  for (const char *position = begin; position < cursor; ++position)
    if (*position == '\n')
      ++line;
  fprintf(stderr, "%s:%zu: %s\n", path, line, reason);
  return false;
}

bool configLoad(Port *port, const char *path)
{
  SOFT_ASSERT(port != NULL, "Port pointer is NULL", false);
  SOFT_ASSERT(path != NULL, "Configuration path is NULL", false);
  int descriptor = open(path, O_RDONLY | O_NONBLOCK);
  if (descriptor < 0)
  {
    perror(path);
    return false;
  }
  struct stat info;
  if (fstat(descriptor, &info) < 0 || !S_ISREG(info.st_mode))
  {
    fprintf(stderr, "%s: configuration must be a regular file\n", path);
    close(descriptor);
    return false;
  }
  char buffer[65537];
  size_t size = 0;
  while (size < sizeof(buffer) - 1)
  {
    ssize_t received =
        read(descriptor, buffer + size, sizeof(buffer) - 1 - size);
    if (received < 0 && errno == EINTR)
      continue;
    if (received < 0)
    {
      close(descriptor);
      return false;
    }
    if (!received)
      break;
    size += (size_t)received;
  }
  close(descriptor);
  if (size == sizeof(buffer) - 1 || memchr(buffer, 0, size))
  {
    fprintf(stderr,
            "%s: file is too large (maximum 65535 bytes) or contains NUL\n",
            path);
    return false;
  }
  buffer[size] = 0;
  const char *cursor = buffer;
  int headerValues[9];
  for (int index = 0; index < 9; ++index)
    if ((headerValues[index] = parseInt(&cursor)) < 0)
      return configError(
          path, buffer, cursor,
          "expected nine nonnegative header numbers (0..1000000)");
  int tugCount = headerValues[1], berthCount = headerValues[2],
      craneCount = headerValues[3], shipCount = headerValues[7];
  if (tugCount > 16 || berthCount > 16 || craneCount > 16 || shipCount > 32 ||
      !headerValues[4] || !headerValues[5] || !headerValues[6] ||
      headerValues[5] > 10000 || headerValues[6] > 10000)
    return configError(path, buffer, cursor,
                       "header bounds: tugs/berths/cranes <=16, ships <=32; "
                       "positive capacity and durations <=10000");
  int berthCompatibility[16][3] = {{0}}, craneCompatibility[16][2] = {{0}},
      ships[32][4] = {{0}};
  bool seen[16] = {false};
  for (int index = 0; index < berthCount; ++index)
  {
    int id = parseInt(&cursor);
    if (id < 0 || id >= berthCount || seen[id])
      return configError(path, buffer, cursor,
                         "berth ID is repeated or out of range");
    seen[id] = true;
    for (int column = 0; column < 3; ++column)
      if ((berthCompatibility[id][column] = parseInt(&cursor)) < 0 ||
          berthCompatibility[id][column] > 1)
        return configError(path, buffer, cursor,
                           "berth compatibility must be 0 or 1");
  }
  memset(seen, 0, sizeof(seen));
  for (int index = 0; index < craneCount; ++index)
  {
    int id = parseInt(&cursor);
    if (id < 0 || id >= craneCount || seen[id])
      return configError(path, buffer, cursor,
                         "crane ID is repeated or out of range");
    seen[id] = true;
    for (int column = 0; column < 2; ++column)
      if ((craneCompatibility[id][column] = parseInt(&cursor)) < 0 ||
          craneCompatibility[id][column] > 1)
        return configError(path, buffer, cursor,
                           "crane compatibility must be 0 or 1");
  }
  for (int index = 0; index < shipCount; ++index)
  {
    for (int column = 0; column < 4; ++column)
      if ((ships[index][column] = parseInt(&cursor)) < 0)
        return configError(path, buffer, cursor,
                           "incomplete or malformed ship description");
    if (ships[index][0] > 2 || ships[index][1] > 1)
      return configError(path, buffer, cursor,
                         "ship type must be 0..2 and cargo type 0..1");
  }
  int options[10] = {1, 0, 1, headerValues[6], headerValues[4] / 4 + 1, 3, 0,
                     0, 0, 0};
  int masks[16], rates[16], operations[32] = {0}, needs[32];
  for (int index = 0; index < 16; ++index)
  {
    masks[index] = 7;
    rates[index] = headerValues[4];
  }
  for (int index = 0; index < 32; ++index)
    needs[index] = 1;
  while (isspace((unsigned char)*cursor))
    ++cursor;
  if (*cursor)
  {
    if (strncmp(cursor, "EXT", 3) || !isspace((unsigned char)cursor[3]))
      return configError(path, buffer, cursor, "expected EXT or end of file");
    cursor += 3;
    for (int index = 0; index < 10; ++index)
      if ((options[index] = parseInt(&cursor)) < 0)
        return configError(path, buffer, cursor,
                           "EXT requires ten valid parameters");
    if (options[0] > 2 || options[1] > 1 || !options[2] || !options[3] ||
        options[2] > 10000 || options[3] > 10000 || options[5] < 1 ||
        options[5] > 3 || options[6] + options[7] > headerValues[4])
      return configError(path, buffer, cursor,
                         "invalid EXT strategy, reservation, duration, cargo "
                         "mask or initial stock");
    for (int index = 0; index < tugCount; ++index)
      if ((masks[index] = parseInt(&cursor)) < 0 || masks[index] > 7)
        return configError(path, buffer, cursor,
                           "tug compatibility mask must be 0..7");
    for (int index = 0; index < craneCount; ++index)
      if ((rates[index] = parseInt(&cursor)) < 1)
        return configError(path, buffer, cursor,
                           "crane rate must be 1..1000000");
    for (int index = 0; index < shipCount; ++index)
    {
      operations[index] = parseInt(&cursor);
      needs[index] = parseInt(&cursor);
      if (operations[index] < 0 || operations[index] > 1 || needs[index] < 1 ||
          needs[index] > 16)
        return configError(path, buffer, cursor,
                           "operation must be 0/1 and crane count 1..16");
    }
    while (isspace((unsigned char)*cursor))
      ++cursor;
    if (*cursor)
      return configError(path, buffer, cursor, "unexpected trailing data");
  }
  if (!(options[5] & 1) && (options[6] || options[8]))
    return configError(path, buffer, cursor,
                       "container stock/supply conflicts with warehouse mask");
  if (!(options[5] & 2) && (options[7] || options[9]))
    return configError(path, buffer, cursor,
                       "liquid stock/supply conflicts with warehouse mask");
  if (!resAlloc(&port->res, tugCount, berthCount, craneCount, headerValues[4]))
    return configError(path, buffer, cursor, "resource allocation failed");
  port->maxTime = headerValues[0];
  dispInit(&port->disp, &port->res, headerValues[5], headerValues[6],
           (unsigned)headerValues[8]);
  port->strategy = options[0];
  port->reservation = options[1];
  port->moorTime = options[2];
  port->warehousePeriod = options[3];
  port->warehouseOut = options[4];
  port->res.warehouse.cargoMask = options[5];
  port->res.warehouse.stock[0] = options[6];
  port->res.warehouse.stock[1] = options[7];
  port->res.warehouse.used = options[6] + options[7];
  port->supply[0] = options[8];
  port->supply[1] = options[9];
  for (int index = 0; index < tugCount; ++index)
    port->res.tugs[index].typeMask = masks[index];
  for (int index = 0; index < berthCount; ++index)
    for (int column = 0; column < 3; ++column)
      setBerthCompatibility(&port->res, index, (ShipType)column,
                            berthCompatibility[index][column]);
  for (int index = 0; index < craneCount; ++index)
  {
    port->res.cranes[index].rate = rates[index];
    for (int column = 0; column < 2; ++column)
      setCraneCompatibility(&port->res, index, (CargoType)column,
                            craneCompatibility[index][column]);
  }
  if (!allocShips(port, shipCount))
    return configError(path, buffer, cursor, "ship allocation failed");
  for (int index = 0; index < shipCount; ++index)
  {
    addShip(port, index, (ShipType)ships[index][0], (CargoType)ships[index][1],
            ships[index][2], ships[index][3]);
    port->ships[index].loading = operations[index];
    port->ships[index].cranesNeeded = needs[index];
  }
  return true;
}
