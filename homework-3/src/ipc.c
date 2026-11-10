#include "process.h"

void transportInit(Transport *transport, const char *session, int mode)
{
  memset(transport, 0, sizeof(*transport));
  transport->shmfd = -1;
  transport->claimfd = -1;
  transport->meta.mode = mode;
  snprintf(transport->session, sizeof(transport->session), "%s", session);
  snprintf(transport->dir, sizeof(transport->dir), "/tmp/port3_%lu_%s",
           (unsigned long)getuid(), session);
  snprintf(transport->shmName, sizeof(transport->shmName), "/port3_%lu_%s",
           (unsigned long)getuid(), session);
  for (int index = 0; index < ACTORS; ++index)
    transport->reqfd[index] = transport->repfd[index] = -1;
}

static void semName(Transport *transport, int index, char kind, char *out,
                    size_t size)
{
  snprintf(out, size, "/port_%lu_%s_%d%c", (unsigned long)getuid(),
           transport->session, index, kind);
}

static sem_t *openSem(Transport *transport, int index, char kind, int owner,
                      unsigned value)
{
  char name[80];
  semName(transport, index, kind, name, sizeof(name));
  sem_t *semaphore =
      owner ? sem_open(name, O_CREAT | O_EXCL, 0600, value) : sem_open(name, 0);
  return semaphore == SEM_FAILED ? NULL : semaphore;
}

static void pathFor(Transport *transport, int index, char kind, char *out,
                    size_t size)
{
  snprintf(out, size, "%s/%d%c", transport->dir, index, kind);
}

static int setupActor(Transport *transport, int index, int owner)
{
  if (transport->meta.mode == 1)
  {
    if (owner)
    {
      if (sem_init(&transport->shared->slots[index].requestSem, 1, 0) < 0)
        return 0;
      if (sem_init(&transport->shared->slots[index].replySem, 1, 0) < 0)
      {
        sem_destroy(&transport->shared->slots[index].requestSem);
        return 0;
      }
      transport->initialized++;
    }
    transport->req[index] = &transport->shared->slots[index].requestSem;
    transport->rep[index] = &transport->shared->slots[index].replySem;
  }
  else
  {
    transport->req[index] = openSem(transport, index, 'q', owner, 0);
    transport->rep[index] = openSem(transport, index, 'r', owner, 0);
    if (!transport->req[index] || !transport->rep[index])
      return 0;
  }
  if (transport->meta.mode == 3)
  {
    char path[160];
    pathFor(transport, index, 'q', path, sizeof(path));
    if (owner && mkfifo(path, 0600) < 0)
      return 0;
    transport->reqfd[index] = open(path, O_RDWR | O_NONBLOCK);
    pathFor(transport, index, 'r', path, sizeof(path));
    if (owner && mkfifo(path, 0600) < 0)
      return 0;
    transport->repfd[index] = open(path, O_RDWR | O_NONBLOCK);
    if (transport->reqfd[index] < 0 || transport->repfd[index] < 0)
      return 0;
  }
  return 1;
}

int createTransport(Transport *transport)
{
  if (mkdir(transport->dir, 0700) < 0)
  {
    perror("session directory (choose a unique session)");
    return 0;
  }
  transport->owner = 1;
  transport->meta.pid = (int)getpid();
  if (transport->meta.mode != 3)
  {
    transport->shmfd =
        shm_open(transport->shmName, O_CREAT | O_EXCL | O_RDWR, 0600);
    if (transport->shmfd < 0 || ftruncate(transport->shmfd, sizeof(Shared)) < 0)
      return 0;
    struct stat sharedInfo;
    if (fstat(transport->shmfd, &sharedInfo) < 0 ||
        sharedInfo.st_size != sizeof(Shared))
      return 0;
    transport->shared = mmap(NULL, sizeof(Shared), PROT_READ | PROT_WRITE,
                             MAP_SHARED, transport->shmfd, 0);
    if (transport->shared == MAP_FAILED)
    {
      transport->shared = NULL;
      return 0;
    }
    memset(transport->shared, 0, sizeof(Shared));
  }
  for (int index = 0; index < transport->meta.count; ++index)
    if (!setupActor(transport, index, 1))
      return 0;
  char path[160];
  snprintf(path, sizeof(path), "%s/meta", transport->dir);
  int descriptor = open(path, O_CREAT | O_EXCL | O_WRONLY, 0600);
  if (descriptor < 0)
    return 0;
  int ok = fullWrite(descriptor, &transport->meta, sizeof(transport->meta));
  close(descriptor);
  return ok;
}

int attachTransport(Transport *transport, int actor)
{
  char path[160];
  snprintf(path, sizeof(path), "%s/meta", transport->dir);
  int descriptor = open(path, O_RDONLY);
  if (descriptor < 0)
    return 0;
  ssize_t count = read(descriptor, &transport->meta, sizeof(transport->meta));
  close(descriptor);
  if (count != sizeof(transport->meta) ||
      transport->meta.count < 1 || transport->meta.count > ACTORS ||
      actor >= transport->meta.count || transport->meta.mode < 2 ||
      transport->meta.mode > 3 || transport->meta.pid <= 1 ||
      transport->meta.role[actor] < SHIP_ACTOR ||
      transport->meta.role[actor] > WAREHOUSE_ACTOR)
    return 0;
  if (transport->meta.mode == 2)
  {
    transport->shmfd = shm_open(transport->shmName, O_RDWR, 0600);
    if (transport->shmfd < 0)
      return 0;
    struct stat sharedInfo;
    if (fstat(transport->shmfd, &sharedInfo) < 0 ||
        sharedInfo.st_size != sizeof(Shared))
      return 0;
    transport->shared = mmap(NULL, sizeof(Shared), PROT_READ | PROT_WRITE,
                             MAP_SHARED, transport->shmfd, 0);
    if (transport->shared == MAP_FAILED)
    {
      transport->shared = NULL;
      return 0;
    }
  }
  if (!setupActor(transport, actor, 0))
    return 0;
  transport->claimfd = open(path, O_RDWR);
  if (transport->claimfd < 0)
    return 0;
  struct flock lock = {
      .l_type = F_WRLCK, .l_whence = SEEK_SET, .l_start = actor, .l_len = 1};
  if (fcntl(transport->claimfd, F_SETLK, &lock) < 0)
  {
    fprintf(stderr, "Actor already connected\n");
    return 0;
  }
  return 1;
}

void closeTransport(Transport *transport)
{
  for (int index = 0; index < transport->meta.count && index < ACTORS;
       ++index)
  {
    if (transport->meta.mode == 1)
    {
      if (transport->owner && index < transport->initialized)
      {
        sem_destroy(transport->req[index]);
        sem_destroy(transport->rep[index]);
      }
    }
    else
    {
      sem_t *semaphores[] = {
          transport->req[index],
          transport->rep[index]};
      const char kinds[] = "qr";
      for (int semaphoreIndex = 0; semaphoreIndex < 2; ++semaphoreIndex)
        if (semaphores[semaphoreIndex])
        {
          sem_close(semaphores[semaphoreIndex]);
          if (transport->owner)
          {
            char name[80];
            semName(transport, index, kinds[semaphoreIndex], name,
                    sizeof(name));
            sem_unlink(name);
          }
        }
    }
    if (transport->reqfd[index] >= 0)
      close(transport->reqfd[index]);
    if (transport->repfd[index] >= 0)
      close(transport->repfd[index]);
    if (transport->owner && transport->meta.mode == 3)
    {
      char path[160];
      pathFor(transport, index, 'q', path, sizeof(path));
      unlink(path);
      pathFor(transport, index, 'r', path, sizeof(path));
      unlink(path);
    }
  }
  if (transport->claimfd >= 0)
    close(transport->claimfd);
  if (transport->shared)
    munmap(transport->shared, sizeof(Shared));
  if (transport->shmfd >= 0)
  {
    close(transport->shmfd);
    if (transport->owner)
      shm_unlink(transport->shmName);
  }
  if (transport->owner)
  {
    char path[160];
    snprintf(path, sizeof(path), "%s/meta", transport->dir);
    unlink(path);
    rmdir(transport->dir);
  }
}

int putMessage(Transport *transport, int actor, int reply, Message message)
{
  SOFT_ASSERT(transport != NULL, "Transport is required", 0);
  SOFT_ASSERT(actor >= 0 && actor < transport->meta.count,
              "Actor index out of range", 0);
  message.pid = (int)getpid();
  if (transport->meta.mode == 3)
  {
    int descriptor = reply ? transport->repfd[actor] : transport->reqfd[actor];
    ssize_t count;
    do
    {
      count = write(descriptor, &message, sizeof(message));
    } while (count < 0 && errno == EINTR && !interrupted);
    if (count != sizeof(message))
      return 0;
  }
  else
  {
    if (reply)
      transport->shared->slots[actor].reply = message;
    else
      transport->shared->slots[actor].request = message;
  }
  return sem_post(reply ? transport->rep[actor] : transport->req[actor]) == 0;
}

static int readMessage(Transport *transport, int actor, int reply,
                       Message *message)
{
  if (transport->meta.mode == 3)
  {
    ssize_t received;
    do
    {
      received = read(reply ? transport->repfd[actor] : transport->reqfd[actor],
                      message, sizeof(*message));
    } while (received < 0 && errno == EINTR && !interrupted);
    if (received != sizeof(*message))
      return -1;
  }
  else
  {
    *message = reply ? transport->shared->slots[actor].reply
                     : transport->shared->slots[actor].request;
  }
  return 1;
}

int getMessage(Transport *transport, int actor, int reply, Message *message)
{
  SOFT_ASSERT(transport && message, "Message pointers are required", -1);
  SOFT_ASSERT(actor >= 0 && actor < transport->meta.count,
              "Actor index out of range", -1);
  if (sem_trywait(reply ? transport->rep[actor] : transport->req[actor]) < 0)
    return errno == EAGAIN || errno == EINTR ? 0 : -1;
  return readMessage(transport, actor, reply, message);
}

int waitMessage(Transport *transport, int actor, int reply, Message *message)
{
  SOFT_ASSERT(transport && message, "Message pointers are required", -1);
  SOFT_ASSERT(actor >= 0 && actor < transport->meta.count,
              "Actor index out of range", -1);
  struct timespec deadline;
  if (clock_gettime(CLOCK_REALTIME, &deadline) < 0)
    return -1;
  deadline.tv_nsec += 100000000L;
  if (deadline.tv_nsec >= 1000000000L)
  {
    ++deadline.tv_sec;
    deadline.tv_nsec -= 1000000000L;
  }
  if (sem_timedwait(reply ? transport->rep[actor] : transport->req[actor],
                    &deadline) < 0)
    return errno == ETIMEDOUT || errno == EINTR ? 0 : -1;
  return readMessage(transport, actor, reply, message);
}

int peersAlive(Transport *transport)
{
  for (int index = 0; index < transport->meta.count; ++index)
  {
    if (transport->peers[index] > 0 && kill(transport->peers[index], 0) < 0 &&
        errno == ESRCH)
      return 0;
  }
  for (int index = 0; index < transport->childCount; ++index)
  {
    int status;
    pid_t pid = waitpid(transport->children[index], &status, WNOHANG);
    if (pid == transport->children[index])
    {
      transport->children[index] = 0;
      return 0;
    }
  }
  return 1;
}

int waitReady(Transport *transport)
{
  double deadline = monotonic() + 120;
  int connected = 0;
  while (connected < transport->meta.count && !interrupted &&
         monotonic() < deadline)
  {
    for (int index = 0; index < transport->meta.count; ++index)
      if (!transport->peers[index])
      {
        Message message;
        int result = waitMessage(transport, index, 1, &message);
        if (result < 0 ||
            (result == 1 && (message.action != HELLO || message.pid <= 1)))
          return 0;
        if (result == 1)
        {
          transport->peers[index] = message.pid;
          ++connected;
        }
      }
    if (!peersAlive(transport))
      return 0;
    pauseMs(1);
  }
  return connected == transport->meta.count;
}
