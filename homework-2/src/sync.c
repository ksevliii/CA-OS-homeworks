#include "sync.h"
#include "utils.h"
#include <errno.h>

#ifndef COND

bool semaInit(Sema *sema, int value)
{
    SOFT_ASSERT(sema != NULL, "Sema pointer is NULL", false);
    return sem_init(&sema->sem, 0, (unsigned int)value) == 0;
}

void semaDestroy(Sema *sema)
{
    if (sema != NULL)
        sem_destroy(&sema->sem);
}

void semaPost(Sema *sema)
{
    if (sema != NULL)
        sem_post(&sema->sem);
}

bool semaWait(Sema *sema)
{
    SOFT_ASSERT(sema != NULL, "Sema pointer is NULL", false);
    while (sem_wait(&sema->sem) == -1)
    {
        if (errno != EINTR)
            return false;
    }
    return true;
}

#else

bool semaInit(Sema *sema, int value)
{
    SOFT_ASSERT(sema != NULL, "Sema pointer is NULL", false);
    sema->count = value;
    if (pthread_mutex_init(&sema->mutex, NULL) != 0)
        return false;
    return pthread_cond_init(&sema->cond, NULL) == 0;
}

void semaDestroy(Sema *sema)
{
    if (sema == NULL)
        return;
    pthread_cond_destroy(&sema->cond);
    pthread_mutex_destroy(&sema->mutex);
}

void semaPost(Sema *sema)
{
    if (sema == NULL)
        return;
    pthread_mutex_lock(&sema->mutex);
    sema->count++;
    pthread_cond_signal(&sema->cond);
    pthread_mutex_unlock(&sema->mutex);
}

bool semaWait(Sema *sema)
{
    SOFT_ASSERT(sema != NULL, "Sema pointer is NULL", false);
    pthread_mutex_lock(&sema->mutex);
    while (sema->count == 0)
        pthread_cond_wait(&sema->cond, &sema->mutex);
    sema->count--;
    pthread_mutex_unlock(&sema->mutex);
    return true;
}

#endif
