#include "port.h"
#include "simulation.h"
#include "config.h"
#include <unistd.h>
#include "logger.h"
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>

static Port *globalPort = NULL; // for Ctrl+C interrupt

static void onSigint(int sig)
{
    (void)sig;
    if (globalPort != NULL)
        globalPort->interrupted = true;
}

int main(int argc, char **argv)
{
    const char *cfg = (argc > 1) ? argv[1] : "config.txt";
    int delayMs = (argc > 2) ? atoi(argv[2]) : 1000;
    Port port;
    portInit(&port);
    globalPort = &port;

    if (!configLoad(&port, cfg))
    {
        printf("Error loading config: %s\n", cfg);
        portFree(&port);
        return 1;
    }
    srand(port.disp.seed);
    signal(SIGINT, onSigint);
    printf("=== Arkhangelsk Port Simulation ===\nCtrl+C to interrupt\n\n");
    sleep(2);
    Logger log;
    if (!loggerOpen(&log, "port.log"))
    {
        printf("Can't create log file\n");
        portFree(&port);
        return 1;
    }
    loggerWriteStr(&log, "=== Simulation Start ===\n");
    char msg[256];
    int n = sprintf(msg,
                    "Resources: tugs=%d, berths=%d, cranes=%d, warehouse=%d\nSeed: %u\n",
                    port.res.tugCount, port.res.berthCount,
                    port.res.craneCount, port.res.warehouse.capacity,
                    port.disp.seed);
    loggerWrite(&log, msg, n);
    simulationRun(&port, delayMs, &log);
    if (port.interrupted)
        loggerWriteStr(&log, "! Interrupted by user (Ctrl+C) !\n");
    else
        loggerWriteStr(&log, "=== Simulation Finished ===\n");
    loggerClose(&log);
    portFree(&port);
    globalPort = NULL;
    printf("\nLog saved to port.log\n");
    return 0;
}