#ifndef SIMULATION_H
#define SIMULATION_H

/**
 * @file simulation.h
 * @brief Port simulation
 */

#include "logger.h"
#include "port.h"

/**
 * @brief Run the simulation
 * @param port    Pointer to the port
 * @param delayMs Delay between ticks in milliseconds
 * @param log     Pointer to the logger for writing events
 */
void simulationRun(Port *port, int delayMs, Logger *log);

#endif