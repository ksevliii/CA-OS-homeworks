#include "network.h"

int main(int argc, char **argv)
{
#ifdef ACTOR_ROLE
  return processRoleMain(argc, argv, ACTOR_ROLE);
#elif defined(OBSERVER_CLIENT)
  return observerMain(argc, argv);
#else
  return processMain(argc, argv);
#endif
}
