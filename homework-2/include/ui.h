#ifndef UI_H
#define UI_H

/**
 * @file ui.h
 * @brief Console interface of the port
 */

#include "port.h"

/**
 * @brief Render the current port state to the console
 * Locks the port mutex while reading the state
 * @param port Pointer to the port
 */
void uiRender(Port *port);

#endif
