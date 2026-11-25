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
 * @note The mutex comment describes Homework 2. TCP observers now receive
 * an immutable formatted snapshot; no mutex or shared Port is required.
 * @param snapshot Null-terminated integrated view received over TCP.
 * @return 1 if output succeeded, otherwise 0.
 */
int uiRender(const char *snapshot);

/** @brief Format the dispatcher-owned state without writing to its console.
 * @param port Model state.
 * @param buffer Destination text buffer.
 * @param capacity Buffer size including the terminator.
 * @return Text length, or zero if arguments or capacity are invalid.
 */
int uiFormat(Port *port, char *buffer, size_t capacity);

/**
 * @brief Store a recent event without changing the full file log.
 * @param port Dispatcher-owned model state.
 * @param text Null-terminated event text, truncated to fit the dashboard.
 */
void uiAddEvent(Port *port, const char *text);

#endif
