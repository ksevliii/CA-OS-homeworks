#ifndef CONFIG_H
#define CONFIG_H

/**
 * @file config.h
 * @brief Loading port configuration from a text file
 * Uses system calls open/read/close
 */

#include "port.h"

/**
 * @brief Load configuration from file
 * @param port Pointer to port
 * @param path Path to configuration file
 * @return true on success, false on error
 */
bool configLoad(Port *port, const char *path);

#endif