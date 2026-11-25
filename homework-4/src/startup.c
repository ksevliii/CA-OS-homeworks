#include "network.h"

int waitReady(Transport *transport)
{
  double deadline = monotonic() + 120;
  double lastDisplay = 0;
  while (!interrupted && monotonic() < deadline)
  {
    if (!networkPump(transport, 50))
      return 0;
    if (transport->network->engine && monotonic() - lastDisplay >= 0.1)
    {
      publishState(transport->network->engine, "WAITING FOR WORKERS");
      lastDisplay = monotonic();
    }
    if (allConnected(transport))
      return 1;
  }
  return 0;
}
