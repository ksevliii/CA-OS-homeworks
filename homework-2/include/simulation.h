#ifndef SIMULATION_H
#define SIMULATION_H

/**
 * @file simulation.h
 * @brief Port simulation
 */

#include "logger.h"
#include "port.h"

/**
 * @brief Run the simulation: start all threads, count the time, draw the
 * console, stop the threads and collect the results
 * @param port    Pointer to the port
 * @param delayMs Delay between ticks in milliseconds
 * @param log     Pointer to the logger for writing events
 */
void simulationRun(Port *port, int delayMs, Logger *log);

/**
 * @brief Sleep until the given simulation time
 * @param port  Pointer to the port
 * @param until Simulation time to wait for
 * @return true if the time has come, false if the simulation was stopped
 */
bool simWait(Port *port, int until);

#endif
