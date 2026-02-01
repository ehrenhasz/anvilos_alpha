 
#define USE_VOLATILE 0

#if USE_POSIX_THREADS && HAVE_SEMAPHORE_H
 
# define USE_SEMAPHORE 1
 
# if defined __APPLE__ && defined __MACH__
#  define USE_NAMED_SEMAPHORE 1
# else
#  define USE_UNNAMED_SEMAPHORE 1
# endif
#endif


#if USE_SEMAPHORE
# include <errno.h>
# include <fcntl.h>
# include <semaphore.h>
# include <unistd.h>
#endif


#if USE_VOLATILE
struct atomic_int {
  volatile int value;
};
static void
init_atomic_int (struct atomic_int *ai)
{
}
static int
get_atomic_int_value (struct atomic_int *ai)
{
  return ai->value;
}
static void
set_atomic_int_value (struct atomic_int *ai, int new_value)
{
  ai->value = new_value;
}
#elif USE_SEMAPHORE
 
# if USE_UNNAMED_SEMAPHORE
struct atomic_int {
  sem_t semaphore;
};
#define atomic_int_semaphore(ai) (&(ai)->semaphore)
static void
init_atomic_int (struct atomic_int *ai)
{
  sem_init (&ai->semaphore, 0, 0);
}
# endif
# if USE_NAMED_SEMAPHORE
struct atomic_int {
  sem_t *semaphore;
};
#define atomic_int_semaphore(ai) ((ai)->semaphore)
static void
init_atomic_int (struct atomic_int *ai)
{
  sem_t *s;
  unsigned int count;
  for (count = 0; ; count++)
    {
      char name[80];
       
      sprintf (name, "test-lock-%lu-%p-%u",
               (unsigned long) getpid (), ai, count);
      s = sem_open (name, O_CREAT | O_EXCL, 0600, 0);
      if (s == SEM_FAILED)
        {
          if (errno == EEXIST)
             
            continue;
          else
            {
              perror ("sem_open failed");
              abort ();
            }
        }
      else
        {
           
          sem_unlink (name);
          break;
        }
    }
  ai->semaphore = s;
}
# endif
static int
get_atomic_int_value (struct atomic_int *ai)
{
  if (sem_trywait (atomic_int_semaphore (ai)) == 0)
    {
      if (sem_post (atomic_int_semaphore (ai)))
        abort ();
      return 1;
    }
  else if (errno == EAGAIN)
    return 0;
  else
    abort ();
}
static void
set_atomic_int_value (struct atomic_int *ai, int new_value)
{
  if (new_value == 0)
     
    return;
   
  if (sem_post (atomic_int_semaphore (ai)))
    abort ();
}
#else
struct atomic_int {
  pthread_mutex_t lock;
  int value;
};
static void
init_atomic_int (struct atomic_int *ai)
{
  pthread_mutexattr_t attr;

  ASSERT (pthread_mutexattr_init (&attr) == 0);
  ASSERT (pthread_mutexattr_settype (&attr, PTHREAD_MUTEX_NORMAL) == 0);
  ASSERT (pthread_mutex_init (&ai->lock, &attr) == 0);
  ASSERT (pthread_mutexattr_destroy (&attr) == 0);
}
static int
get_atomic_int_value (struct atomic_int *ai)
{
  ASSERT (pthread_mutex_lock (&ai->lock) == 0);
  int ret = ai->value;
  ASSERT (pthread_mutex_unlock (&ai->lock) == 0);
  return ret;
}
static void
set_atomic_int_value (struct atomic_int *ai, int new_value)
{
  ASSERT (pthread_mutex_lock (&ai->lock) == 0);
  ai->value = new_value;
  ASSERT (pthread_mutex_unlock (&ai->lock) == 0);
}
#endif
