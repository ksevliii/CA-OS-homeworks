#include "ship.h"
#include "port.h"
#include "simulation.h"

void shipInit(Ship *ship, int id, ShipType type, CargoType cargoType, int volume, int arrTime)
{
    if (ship == NULL)
    {
        fprintf(stderr, "failed: ship != NULL\n");
        perror("Ship pointer is NULL");
        return;
    }
    if (id < 0)
    {
        fprintf(stderr, "failed: id >= 0\n");
        perror("Ship id < 0");
        return;
    }
    if (volume <= 0)
    {
        fprintf(stderr, "failed: volume > 0\n");
        perror("Ship volume <= 0");
        return;
    }
    *ship = (Ship){
        .id = id,
        .type = type,
        .cargoType = cargoType,
        .cargoVolume = volume,
        .arrivalTime = arrTime,
        .state = NOT_ARRIVED,
        .tugId = -1,
        .berthId = -1,
        .craneId = -1,
        .eventTime = 0,
        .totalWait = 0,
        .startVolume = volume,
        .waitSince = 0,
        .warehouseWait = false,
        .port = NULL};
    semaInit(&ship->granted, 0);
}

const char *shipStateName(ShipState state)
{
    switch (state)
    {
    case NOT_ARRIVED:
        return "not arrived";
    case QUEUED:
        return "queued";
    case ENTERING:
        return "entering";
    case WAITING_BERTH:
        return "waiting berth";
    case MOORED:
        return "moored";
    case WAITING_CRANE_AND_WAREHOUSE:
        return "waiting crane/warehouse";
    case UNLOADING:
        return "unloading";
    case WAITING_DEPART:
        return "waiting depart tug";
    case EXITING:
        return "exiting";
    case DEPARTED:
        return "departed";
    }
    return "unknown state";
}

static int shipCargoLeft(Ship *ship)
{
    pthread_mutex_lock(&ship->port->mutex);
    int left = ship->cargoVolume;
    pthread_mutex_unlock(&ship->port->mutex);
    return left;
}

static bool shipRequest(Ship *ship, ShipState state, const char *what)
{
    Port *port = ship->port;
    char msg[256];
    pthread_mutex_lock(&port->mutex);
    ship->state = state;
    ship->waitSince = port->currentTime;
    sprintf(msg, "Ship #%d requests %s", ship->id, what);
    logEvent(port, msg);
    pthread_mutex_unlock(&port->mutex);
    semaPost(&port->disp.wake);
    return portWait(port, &ship->granted);
}

void *shipThread(void *arg)
{
    Ship *ship = (Ship *)arg;
    Port *port = ship->port;
    char msg[256];
    if (!simWait(port, ship->arrivalTime))
        return NULL;
    pthread_mutex_lock(&port->mutex);
    statsRecordArrival(&port->stats);
    sprintf(msg, "Ship #%d arrived (type=%d, cargo=%d, vol=%d)",
            ship->id, ship->type, ship->cargoType, ship->cargoVolume);
    logEvent(port, msg);
    pthread_mutex_unlock(&port->mutex);
    if (!shipRequest(ship, QUEUED, "tug for entering"))
        return NULL;
    if (!shipRequest(ship, WAITING_BERTH, "berth"))
        return NULL;
    while (shipCargoLeft(ship) > 0)
    {
        if (!shipRequest(ship, WAITING_CRANE_AND_WAREHOUSE, "crane and warehouse space"))
            return NULL;
    }
    if (!shipRequest(ship, WAITING_DEPART, "tug for departure"))
        return NULL;
    pthread_mutex_lock(&port->mutex);
    ship->state = DEPARTED;
    statsRecordDeparture(&port->stats);
    sprintf(msg, "Ship #%d departed (cargo left on board: %d)", ship->id, ship->cargoVolume);
    logEvent(port, msg);
    pthread_mutex_unlock(&port->mutex);
    semaPost(&port->disp.wake);
    return NULL;
}
