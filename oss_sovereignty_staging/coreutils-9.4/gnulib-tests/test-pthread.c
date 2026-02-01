 

#include <config.h>

#include <pthread.h>

 

pthread_t t1;
pthread_attr_t t2;

pthread_once_t t3 = PTHREAD_ONCE_INIT;

pthread_mutex_t t4 = PTHREAD_MUTEX_INITIALIZER;
pthread_mutexattr_t t5;

pthread_rwlock_t t6 = PTHREAD_RWLOCK_INITIALIZER;
pthread_rwlockattr_t t7;

pthread_cond_t t8 = PTHREAD_COND_INITIALIZER;
pthread_condattr_t t9;

pthread_key_t t10;

pthread_spinlock_t t11;

#ifdef TODO  
pthread_barrier_t t12;
pthread_barrierattr_t t13;
#endif

 

 
int ds[] = { PTHREAD_CREATE_JOINABLE, PTHREAD_CREATE_DETACHED };

 
void *canceled = PTHREAD_CANCELED;

 
int mt[] = {
  PTHREAD_MUTEX_DEFAULT,
  PTHREAD_MUTEX_NORMAL,
  PTHREAD_MUTEX_RECURSIVE,
  PTHREAD_MUTEX_ERRORCHECK
};

#ifdef TODO  

 
int mr[] = { PTHREAD_MUTEX_ROBUST, PTHREAD_MUTEX_STALLED };

 
int bp[] = { PTHREAD_PROCESS_SHARED, PTHREAD_PROCESS_PRIVATE };

 
int bw[] = { PTHREAD_BARRIER_SERIAL_THREAD };

 
int cs[] = { PTHREAD_CANCEL_ENABLE, PTHREAD_CANCEL_DISABLE };

 
int ct[] = { PTHREAD_CANCEL_DEFERRED, PTHREAD_CANCEL_ASYNCHRONOUS };

#endif


int
main (void)
{
  return 0;
}
