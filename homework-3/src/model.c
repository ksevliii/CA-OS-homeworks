#include "process.h"
#include "ui.h"

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
      int result = job->due <= engine->port->currentTime
                       ? waitMessage(engine->ipc, index, 1, &message)
                       : getMessage(engine->ipc, index, 1, &message);
      if (result < 0 ||
          (result == 1 &&
           (message.seq != job->seq || message.action != job->action ||
            message.ship != job->ship || message.volume != job->volume ||
            message.pid != engine->ipc->peers[index])))
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

void shutdownActors(Transport *transport)
{
  for (int index = 0; index < transport->meta.count; ++index)
    if (transport->peers[index] > 1)
    {
      kill(transport->peers[index], SIGCONT);
      kill(transport->peers[index], SIGTERM);
    }
  for (int index = 0; index < transport->childCount; ++index)
    if (transport->children[index] > 0)
    {
      kill(transport->children[index], SIGCONT);
      kill(transport->children[index], SIGTERM);
    }
  for (int index = 0; index < transport->childCount; ++index)
    if (transport->children[index] > 0)
    {
      while (waitpid(transport->children[index], NULL, 0) < 0 &&
             errno == EINTR)
      {
      }
    }
  pauseMs(30);
}

void summary(Engine *engine, const char *reason)
{
  Port *port = engine->port;
  long long totalWait = 0;
  PORT_EVENT(engine, "Termination: %s", reason);
  for (int index = 0; index < port->shipCount; ++index)
  {
    Ship *ship = &port->ships[index];
    totalWait += ship->totalWait;
    if (ship->state == DEPARTED)
      continue;
    ++port->stats.shipsPending;
    if (ship->cargoVolume > 0 && ship->cargoVolume < ship->startVolume)
      ++port->stats.shipsPartial;
    if (ship->state == QUEUED || ship->state == WAITING_BERTH ||
        ship->state == WAITING_CRANE_AND_WAREHOUSE ||
        ship->state == WAITING_DEPART || ship->state == WAITING_WAREHOUSE)
      totalWait += port->currentTime - ship->waitSince;
    PORT_EVENT(
        engine,
        "Ship #%d not finished: %s operation=%s remaining=%d of %d berth=%d "
        "tug=%d crane=%d",
        ship->id, shipStateName(ship->state), ship->loading ? "load" : "unload",
        ship->cargoVolume, ship->startVolume, ship->berthId, ship->tugId,
        ship->craneId);
  }
  PORT_EVENT(engine,
             "FINAL arrived=%d departed=%d pending=%d partial=%d "
             "cargo_operations=%d moved=%d wait=%lld max_queue=%d",
             port->stats.shipsArrived, port->stats.shipsDeparted,
             port->stats.shipsPending, port->stats.shipsPartial,
             port->stats.unloads, engine->cargoMoved, totalWait,
             port->stats.maxQueueLen);
  PORT_EVENT(engine, "Проверка ограничений ресурсов: %s",
             checkInvariants(engine) ? "пройдена" : "ОШИБКА");
}

int runModel(Engine *engine)
{
  Port *port = engine->port;
  port->uiActive = isatty(STDOUT_FILENO);
  for (int index = 0; index < port->shipCount; ++index)
    startJob(engine, engine->actorShip[index], ARRIVAL, index, 0,
             port->ships[index].arrivalTime);
  int nextWarehouse = port->warehousePeriod;
  const char *reason = "all ships processed";
  while (!interrupted && !engine->failed)
  {
    for (;;)
    {
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
        PORT_EVENT(engine, "ERROR: actor failure or heartbeat timeout");
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
    if (port->uiActive)
    {
      uiRender(port);
      if (ferror(stdout))
        engine->failed = 1;
    }
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
    pauseMs(port->delayMs ? port->delayMs : 1);
    ++port->currentTime;
  }
  if (interrupted)
    reason = "Interrupted by user signal";
  else if (engine->failed && !strcmp(reason, "all ships processed"))
    reason = "IPC or invariant failure";
  port->uiActive = false;
  summary(engine, reason);
  return engine->failed ? 3 : 0;
}
