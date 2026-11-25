#include "process.h"
#include "ui.h"

void event(Engine *engine, const char *text)
{
  char line[600];
  int length = snprintf(line, sizeof(line), "[T=%06d] %s\n",
                        engine->port->currentTime, text);
  if (length < 0 || (size_t)length >= sizeof(line))
  {
    engine->failed = 1;
    return;
  }
  uiAddEvent(engine->port, line);
  if (!fullWrite(STDOUT_FILENO, line, (size_t)length) ||
      (engine->port->log && !loggerWrite(engine->port->log, line, length)))
    engine->failed = 1;
}
