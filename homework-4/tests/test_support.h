#ifndef TEST_SUPPORT_H
#define TEST_SUPPORT_H
/** @file test_support.h
 * @brief Small integration-test helpers for real TCP processes.
 */

#include <sys/types.h>

#define TEST_WORKERS 82 /**< Maximum worker slots in a scenario. */

/** @brief Processes and temporary output files belonging to one test. */
typedef struct
{
  pid_t server, workers[TEST_WORKERS], observers[3]; /**< Owned child PIDs. */
  int count, role[TEST_WORKERS], port;               /**< Required workers and TCP endpoint. */
  char directory[128], log[160], serverOutput[160];  /**< Temporary evidence paths. */
} TestRun;

/** @brief Start a server and optionally all workers from a known valid fixture. */
int startRun(TestRun *run, const char *config, int delay, int workers);

/** @brief Launch or relaunch one worker under its original role and index. */
pid_t startWorker(TestRun *run, int actor);

/** @brief Start one independent observer and retain its output. */
pid_t startObserver(TestRun *run, int index);

/** @brief Wait for a substring in a file for at most the given real seconds. */
int waitText(const char *path, const char *text, int seconds);

/** @brief Wait for a child with a deadline and check its exit code. */
int finishProcess(pid_t *process, int expected, int seconds);

/** @brief Wait for all processes and require a successful resource check. */
int finishRun(TestRun *run);

/** @brief Stop and reap all remaining children on a test failure. */
void stopRun(TestRun *run);

/** @brief Disconnect one child and wait for its termination. */
void stopProcess(pid_t *process, int signalNumber);

/** @brief Spawn a command with output redirected to a regular file. */
pid_t launch(char *const arguments[], const char *output);

/** @brief Sleep briefly without a shell or a Python test dependency. */
void testPause(int milliseconds);

/** @brief Check split and concatenated frames without assuming TCP boundaries. */
int testFrames(void);
#endif
