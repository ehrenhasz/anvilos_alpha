 

#include <config.h>

 

#if USE_POSIX_THREADS || USE_ISOC_AND_POSIX_THREADS

 

# include <errno.h>
# include <pthread.h>
# include <stdlib.h>

# if PTHREAD_IN_USE_DETECTION_HARD

#  if defined __FreeBSD__ || defined __DragonFly__                  

 

int
glthread_in_use (void)
{
  static int tested;
  static int result;  

  if (!tested)
    {
      pthread_key_t key;
      int err = pthread_key_create (&key, NULL);

      if (err == ENOSYS)
        result = 0;
      else
        {
          result = 1;
          if (err == 0)
            pthread_key_delete (key);
        }
      tested = 1;
    }
  return result;
}

#  else                                                      

 

 
static void *
dummy_thread_func (void *arg)
{
  return arg;
}

int
glthread_in_use (void)
{
  static int tested;
  static int result;  

  if (!tested)
    {
      pthread_t thread;

      if (pthread_create (&thread, NULL, dummy_thread_func, NULL) != 0)
         
        result = 0;
      else
        {
           
          void *retval;
          if (pthread_join (thread, &retval) != 0)
            abort ();
          result = 1;
        }
      tested = 1;
    }
  return result;
}

#  endif

# endif

#endif

 

 
typedef int dummy;
