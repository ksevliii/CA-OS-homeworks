#ifndef LOGGER_H
#define LOGGER_H

/**
 * @file logger.h
 * @brief Logger based on system calls (open/write/close)
 */

#include <stdbool.h>

/** @brief Logger descriptor */
typedef struct {
  int fd;      /**< File descriptor (-1 if not opened) */
  bool opened; /**< Flag: was the file successfully opened */
} Logger;

/**
 * @brief Open a log file for writing
 * @param log  Pointer to Logger
 * @param path Path to the log file
 * @return true on success, false on error
 */
bool loggerOpen(Logger *log, const char *path);

/**
 * @brief Write raw bytes to the log
 * @param log Pointer to Logger
 * @param buf Buffer with data
 * @param len Number of bytes to write
 * @return true if exactly len bytes were written, false otherwise
 */
bool loggerWrite(const Logger *log, const char *buf, int len);

/**
 * @brief Write a null-terminated string to the log
 * @param log Pointer to Logger
 * @param str Null-terminated string
 * @return true on success, false on error
 */
bool loggerWriteStr(const Logger *log, const char *str);

/**
 * @brief Close the log file
 * @param log Pointer to Logger
 */
void loggerClose(Logger *log);

#endif