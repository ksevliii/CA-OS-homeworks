#include "process.h"
#include <stdlib.h>
#include <string.h>

int main(int argc, char **argv)
{
#ifdef ACTOR_ROLE
  return processRoleMain(argc, argv, ACTOR_ROLE);
#endif
#ifdef DEFAULT_MODE
  if (argc == 2 && (!strcmp(argv[1], "--help") || !strcmp(argv[1], "-h")))
    return processMain(argc, argv);
  char **args = calloc((size_t)argc + 2, sizeof(*args));
  if (!args)
    return 3;
  args[0] = argv[0];
  args[1] = DEFAULT_MODE;
  for (int argumentIndex = 1; argumentIndex < argc; ++argumentIndex)
    args[argumentIndex + 1] = argv[argumentIndex];
  int result = processMain(argc + 1, args);
  free(args);
  return result;
#else
  return processMain(argc, argv);
#endif
}
