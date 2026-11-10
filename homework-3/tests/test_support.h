#ifndef TEST_SUPPORT_H
#define TEST_SUPPORT_H
/** @file test_support.h
 * @brief Process-launch and timeout helpers for the C integration tests.
 */
#include <stddef.h>
#include <sys/types.h>
#define TEST_ACTORS 82 /**< Maximum independent participants in one test. */
/** @brief Handles and captured output owned by one integration test. */
typedef struct
{
  pid_t server; /**< Dispatcher process, or zero after waitpid. */
  pid_t
      workers[TEST_ACTORS];                           /**< Participant processes started by this test. */
  int workerCount;                                    /**< Number of valid entries in workers. */
  int serverStatus;                                   /**< Saved waitpid status of the dispatcher. */
  char session[25];                                   /**< Unique IPC session name. */
  char directory[128];                                /**< Temporary output directory. */
  char outputPath[160], errorPath[160], logPath[160]; /**< Capture paths. */
  char text[262144];                                  /**< Captured stdout and events. */
} TestRun;
/** @brief Start a dispatcher and, for shm/fifo, each advertised role.
 * @param run Zeroed by this function; caller later calls finishRun or stopRun.
 * @param mode One of the four dispatcher modes.
 * @param config Configuration path relative to the project root, or absolute.
 * @param delay Real milliseconds per model tick.
 * @param reuse Nonzero reuses a fixed session for cleanup testing.
 * @param wrongRole Nonzero checks rejection of a wrong role before
 * registration.
 * @return 1 on successful startup, 0 on a test infrastructure failure.
 */
int startRun(TestRun *run, const char *mode, const char *config, int delay,
             int reuse, int wrongRole);
/** @brief Wait at most five seconds for an event, refreshing captured output.
 * @param run Running dispatcher and output handles.
 * @param event Required substring.
 * @return 1 when found, 0 on early termination or timeout.
 */
int waitEvent(TestRun *run, const char *event);
/** @brief Reap participants, verify exit code, log equality and IPC cleanup.
 * @param run Active run; resources are released by this function.
 * @param expectedCode Expected dispatcher exit status.
 * @return 1 on success, 0 on any mismatch.
 */
int finishRun(TestRun *run, int expectedCode);
/** @brief Stop all processes owned by a test and remove capture files.
 * @param run Initialized test state, including partially started runs.
 */
void stopRun(TestRun *run);
/** @brief Run a short command without a shell, with a two-second deadline.
 * @param arguments Null-terminated execv argument vector, starting with
 * program.
 * @return Exit code, or 124 on timeout, 125 on an execution failure.
 */
int commandStatus(char *const arguments[]);
/** @brief Pause in real time while allowing interrupted sleeps to resume.
 * @param milliseconds Nonnegative duration.
 */
void testPause(int milliseconds);
#endif
