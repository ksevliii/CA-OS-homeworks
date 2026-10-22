#ifndef UI_H
#define UI_H

/**
 * @file ui.h
 * @brief Console interface of the port
 */

#include "port.h"

/**
 * @brief Render the current port state to the console
 * @param port Pointer to the port
 */
void uiRender(const Port *port);

#endif