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
 * @note The comment above describes Homework 2. In this process-based version,
 * only the dispatcher reads and modifies Port; no mutex is needed.
 * @param port Pointer to the port
 */
void uiRender(Port *port);

/**
 * @brief Store a recent event without changing the full file log.
 * @param port Dispatcher-owned model state.
 * @param text Null-terminated event text, truncated to fit the dashboard.
 */
void uiAddEvent(Port *port, const char *text);

#endif
