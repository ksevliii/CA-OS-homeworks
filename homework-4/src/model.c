#include "network.h"
#include "ui.h"

static int waitForConnections(Engine *engine)
{
  double lastDisplay = 0;
  while (!interrupted && !allConnected(engine->ipc))
  {
    if (!networkPump(engine->ipc, 50))
    {
      engine->failed = 1;
      return 0;
    }
    if (monotonic() - lastDisplay >= 0.1)
    {
      publishState(engine, "WAITING FOR RECONNECTION; MODEL TIME PAUSED");
      lastDisplay = monotonic();
    }
  }
  return !interrupted && !engine->failed;
}

static void waitTick(Transport *transport, int milliseconds)
{
  double deadline = monotonic() + milliseconds / 1000.0;
  while (!interrupted && monotonic() < deadline)
    if (!networkPump(transport, 10) || !allConnected(transport))
      break;
}

static int receiveJobs(Engine *engine)
{
  int changed = 0;
  for (int index = 0; index < engine->ipc->meta.count; ++index)
  {
    Job *job = &engine->jobs[index];
    if (!job->seq)
      continue;
    if (!job->ready)
    {
      Message message;
      int result = getMessage(engine->ipc, index, 1, &message);
      if (result < 0 ||
          (result == 1 &&
           (message.seq != job->seq || message.action != job->action ||
            message.ship != job->ship || message.volume != job->volume)))
      {
        engine->failed = 1;
        return 0;
      }
      if (result == 1)
      {
        job->response = message;
        job->ready = 1;
      }
    }
    if (job->ready && job->due <= engine->port->currentTime)
    {
      Job done = *job;
      memset(job, 0, sizeof(*job));
      finishJob(engine, done);
      changed = 1;
    }
  }
  return changed;
}

static int anyDue(Engine *engine)
{
  for (int index = 0; index < engine->ipc->meta.count; ++index)
    if (engine->jobs[index].seq &&
        engine->jobs[index].due <= engine->port->currentTime)
      return 1;
  return 0;
}

static int registerActor(Engine *engine, int role, int id)
{
  int actor = engine->ipc->meta.count++;
  engine->ipc->meta.role[actor] = role;
  engine->ipc->meta.id[actor] = id;
  return actor;
}

void buildActors(Engine *engine)
{
  Port *port = engine->port;
  for (int index = 0; index < port->shipCount; ++index)
    engine->actorShip[index] = registerActor(engine, SHIP_ACTOR, index);
  for (int index = 0; index < port->res.tugCount; ++index)
    engine->actorTug[index] = registerActor(engine, TUG_ACTOR, index);
  for (int index = 0; index < port->res.berthCount; ++index)
    engine->actorBerth[index] = registerActor(engine, BERTH_ACTOR, index);
  for (int index = 0; index < port->res.craneCount; ++index)
    engine->actorCrane[index] = registerActor(engine, CRANE_ACTOR, index);
  engine->actorWarehouse = registerActor(engine, WAREHOUSE_ACTOR, 0);
}

int runModel(Engine *engine)
{
  Port *port = engine->port;
  for (int index = 0; index < port->shipCount; ++index)
    startJob(engine, engine->actorShip[index], ARRIVAL, index, 0,
             port->ships[index].arrivalTime);
  int nextWarehouse = port->warehousePeriod;
  const char *reason = "all ships processed";
  while (!interrupted && !engine->failed)
  {
    for (;;)
    {
      networkPump(engine->ipc, 10);
      if (!allConnected(engine->ipc))
      {
        if (!waitForConnections(engine))
          break;
      }
      receiveJobs(engine);
      dispatch(engine);
      if (engine->failed || interrupted)
        break;
      if (!checkInvariants(engine))
      {
        PORT_EVENT(engine, "ERROR: invariant violation");
        engine->failed = 1;
        break;
      }
      if (!anyDue(engine))
      {
        if (port->currentTime >= nextWarehouse)
        {
          startJob(engine, engine->actorWarehouse, DRAIN, -1, 0, 0);
          nextWarehouse = port->currentTime + port->warehousePeriod;
        }
        else
        {
          break;
        }
      }
      if (!peersAlive(engine->ipc))
      {
        PORT_EVENT(engine, "ERROR: participant terminated unexpectedly");
        engine->failed = 1;
        break;
      }
      pauseMs(1);
    }
    int queue = 0;
    for (int index = 0; index < port->shipCount; ++index)
    {
      ShipState state = port->ships[index].state;
      if (state == QUEUED || state == WAITING_BERTH ||
          state == WAITING_CRANE_AND_WAREHOUSE || state == WAITING_DEPART ||
          state == WAITING_WAREHOUSE)
        ++queue;
    }
    statsUpdateMaxQueue(&port->stats, queue);
    publishState(engine, "RUNNING");
    if (engine->failed || interrupted || isDone(port))
      break;
    if (port->maxTime && port->currentTime >= port->maxTime)
    {
      reason = "simulation time limit";
      break;
    }
    if (port->currentTime >= INT_MAX - 20001 || engine->seq >= INT_MAX - 100)
    {
      reason = "numeric representation limit";
      engine->failed = 1;
      break;
    }
    if (!peersAlive(engine->ipc))
    {
      reason = "participant terminated unexpectedly";
      engine->failed = 1;
      break;
    }
    waitTick(engine->ipc, port->delayMs ? port->delayMs : 1);
    ++port->currentTime;
  }
  if (interrupted)
    reason = "Interrupted by user signal";
  else if (engine->failed && !strcmp(reason, "all ships processed"))
    reason = "TCP or invariant failure";
  summary(engine, reason);
  return engine->failed ? 3 : 0;
}
