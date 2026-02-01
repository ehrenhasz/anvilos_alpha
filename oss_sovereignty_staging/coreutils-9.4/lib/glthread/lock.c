 

#include <config.h>

#include "glthread/lock.h"

 

#if USE_ISOC_THREADS || USE_ISOC_AND_POSIX_THREADS

 

int
glthread_lock_init (gl_lock_t *lock)
{
  if (mtx_init (&lock->mutex, mtx_plain) != thrd_success)
    return ENOMEM;
  lock->init_needed = 0;
  return 0;
}

int
glthread_lock_lock (gl_lock_t *lock)
{
  if (lock->init_needed)
    call_once (&lock->init_once, lock->init_func);
  if (mtx_lock (&lock->mutex) != thrd_success)
    return EAGAIN;
  return 0;
}

int
glthread_lock_unlock (gl_lock_t *lock)
{
  if (lock->init_needed)
    call_once (&lock->init_once, lock->init_func);
  if (mtx_unlock (&lock->mutex) != thrd_success)
    return EINVAL;
  return 0;
}

int
glthread_lock_destroy (gl_lock_t *lock)
{
  if (lock->init_needed)
    call_once (&lock->init_once, lock->init_func);
  mtx_destroy (&lock->mutex);
  return 0;
}

 

int
glthread_rwlock_init (gl_rwlock_t *lock)
{
  if (mtx_init (&lock->lock, mtx_plain) != thrd_success
      || cnd_init (&lock->waiting_readers) != thrd_success
      || cnd_init (&lock->waiting_writers) != thrd_success)
    return ENOMEM;
  lock->waiting_writers_count = 0;
  lock->runcount = 0;
  lock->init_needed = 0;
  return 0;
}

int
glthread_rwlock_rdlock (gl_rwlock_t *lock)
{
  if (lock->init_needed)
    call_once (&lock->init_once, lock->init_func);
  if (mtx_lock (&lock->lock) != thrd_success)
    return EAGAIN;
   
  while (!(lock->runcount + 1 > 0 && lock->waiting_writers_count == 0))
    {
       
      if (cnd_wait (&lock->waiting_readers, &lock->lock) != thrd_success)
        {
          mtx_unlock (&lock->lock);
          return EINVAL;
        }
    }
  lock->runcount++;
  if (mtx_unlock (&lock->lock) != thrd_success)
    return EINVAL;
  return 0;
}

int
glthread_rwlock_wrlock (gl_rwlock_t *lock)
{
  if (lock->init_needed)
    call_once (&lock->init_once, lock->init_func);
  if (mtx_lock (&lock->lock) != thrd_success)
    return EAGAIN;
   
  while (!(lock->runcount == 0))
    {
       
      lock->waiting_writers_count++;
      if (cnd_wait (&lock->waiting_writers, &lock->lock) != thrd_success)
        {
          lock->waiting_writers_count--;
          mtx_unlock (&lock->lock);
          return EINVAL;
        }
      lock->waiting_writers_count--;
    }
  lock->runcount--;  
  if (mtx_unlock (&lock->lock) != thrd_success)
    return EINVAL;
  return 0;
}

int
glthread_rwlock_unlock (gl_rwlock_t *lock)
{
  if (lock->init_needed)
    call_once (&lock->init_once, lock->init_func);
  if (mtx_lock (&lock->lock) != thrd_success)
    return EAGAIN;
  if (lock->runcount < 0)
    {
       
      if (!(lock->runcount == -1))
        {
          mtx_unlock (&lock->lock);
          return EINVAL;
        }
      lock->runcount = 0;
    }
  else
    {
       
      if (!(lock->runcount > 0))
        {
          mtx_unlock (&lock->lock);
          return EINVAL;
        }
      lock->runcount--;
    }
  if (lock->runcount == 0)
    {
       
      if (lock->waiting_writers_count > 0)
        {
           
          if (cnd_signal (&lock->waiting_writers) != thrd_success)
            {
              mtx_unlock (&lock->lock);
              return EINVAL;
            }
        }
      else
        {
           
          if (cnd_broadcast (&lock->waiting_readers) != thrd_success)
            {
              mtx_unlock (&lock->lock);
              return EINVAL;
            }
        }
    }
  if (mtx_unlock (&lock->lock) != thrd_success)
    return EINVAL;
  return 0;
}

int
glthread_rwlock_destroy (gl_rwlock_t *lock)
{
  if (lock->init_needed)
    call_once (&lock->init_once, lock->init_func);
  mtx_destroy (&lock->lock);
  cnd_destroy (&lock->waiting_readers);
  cnd_destroy (&lock->waiting_writers);
  return 0;
}

 

int
glthread_recursive_lock_init (gl_recursive_lock_t *lock)
{
  if (mtx_init (&lock->mutex, mtx_plain | mtx_recursive) != thrd_success)
    return ENOMEM;
  lock->init_needed = 0;
  return 0;
}

int
glthread_recursive_lock_lock (gl_recursive_lock_t *lock)
{
  if (lock->init_needed)
    call_once (&lock->init_once, lock->init_func);
  if (mtx_lock (&lock->mutex) != thrd_success)
    return EAGAIN;
  return 0;
}

int
glthread_recursive_lock_unlock (gl_recursive_lock_t *lock)
{
  if (lock->init_needed)
    call_once (&lock->init_once, lock->init_func);
  if (mtx_unlock (&lock->mutex) != thrd_success)
    return EINVAL;
  return 0;
}

int
glthread_recursive_lock_destroy (gl_recursive_lock_t *lock)
{
  if (lock->init_needed)
    call_once (&lock->init_once, lock->init_func);
  mtx_destroy (&lock->mutex);
  return 0;
}

 

#endif

 

#if USE_POSIX_THREADS

 

 

# if HAVE_PTHREAD_RWLOCK && (HAVE_PTHREAD_RWLOCK_RDLOCK_PREFER_WRITER || (defined PTHREAD_RWLOCK_WRITER_NONRECURSIVE_INITIALIZER_NP && (__GNU_LIBRARY__ > 1)))

#  if defined PTHREAD_RWLOCK_INITIALIZER || defined PTHREAD_RWLOCK_INITIALIZER_NP

#   if !HAVE_PTHREAD_RWLOCK_RDLOCK_PREFER_WRITER
      

int
glthread_rwlock_init_for_glibc (pthread_rwlock_t *lock)
{
  pthread_rwlockattr_t attributes;
  int err;

  err = pthread_rwlockattr_init (&attributes);
  if (err != 0)
    return err;
   
  err = pthread_rwlockattr_setkind_np (&attributes,
                                       PTHREAD_RWLOCK_PREFER_WRITER_NONRECURSIVE_NP);
  if (err == 0)
    err = pthread_rwlock_init(lock, &attributes);
   
  pthread_rwlockattr_destroy (&attributes);
  return err;
}

#   endif
#  else

int
glthread_rwlock_init_multithreaded (gl_rwlock_t *lock)
{
  int err;

  err = pthread_rwlock_init (&lock->rwlock, NULL);
  if (err != 0)
    return err;
  lock->initialized = 1;
  return 0;
}

int
glthread_rwlock_rdlock_multithreaded (gl_rwlock_t *lock)
{
  if (!lock->initialized)
    {
      int err;

      err = pthread_mutex_lock (&lock->guard);
      if (err != 0)
        return err;
      if (!lock->initialized)
        {
          err = glthread_rwlock_init_multithreaded (lock);
          if (err != 0)
            {
              pthread_mutex_unlock (&lock->guard);
              return err;
            }
        }
      err = pthread_mutex_unlock (&lock->guard);
      if (err != 0)
        return err;
    }
  return pthread_rwlock_rdlock (&lock->rwlock);
}

int
glthread_rwlock_wrlock_multithreaded (gl_rwlock_t *lock)
{
  if (!lock->initialized)
    {
      int err;

      err = pthread_mutex_lock (&lock->guard);
      if (err != 0)
        return err;
      if (!lock->initialized)
        {
          err = glthread_rwlock_init_multithreaded (lock);
          if (err != 0)
            {
              pthread_mutex_unlock (&lock->guard);
              return err;
            }
        }
      err = pthread_mutex_unlock (&lock->guard);
      if (err != 0)
        return err;
    }
  return pthread_rwlock_wrlock (&lock->rwlock);
}

int
glthread_rwlock_unlock_multithreaded (gl_rwlock_t *lock)
{
  if (!lock->initialized)
    return EINVAL;
  return pthread_rwlock_unlock (&lock->rwlock);
}

int
glthread_rwlock_destroy_multithreaded (gl_rwlock_t *lock)
{
  int err;

  if (!lock->initialized)
    return EINVAL;
  err = pthread_rwlock_destroy (&lock->rwlock);
  if (err != 0)
    return err;
  lock->initialized = 0;
  return 0;
}

#  endif

# else

int
glthread_rwlock_init_multithreaded (gl_rwlock_t *lock)
{
  int err;

  err = pthread_mutex_init (&lock->lock, NULL);
  if (err != 0)
    return err;
  err = pthread_cond_init (&lock->waiting_readers, NULL);
  if (err != 0)
    return err;
  err = pthread_cond_init (&lock->waiting_writers, NULL);
  if (err != 0)
    return err;
  lock->waiting_writers_count = 0;
  lock->runcount = 0;
  return 0;
}

int
glthread_rwlock_rdlock_multithreaded (gl_rwlock_t *lock)
{
  int err;

  err = pthread_mutex_lock (&lock->lock);
  if (err != 0)
    return err;
   
  while (!(lock->runcount + 1 > 0 && lock->waiting_writers_count == 0))
    {
       
      err = pthread_cond_wait (&lock->waiting_readers, &lock->lock);
      if (err != 0)
        {
          pthread_mutex_unlock (&lock->lock);
          return err;
        }
    }
  lock->runcount++;
  return pthread_mutex_unlock (&lock->lock);
}

int
glthread_rwlock_wrlock_multithreaded (gl_rwlock_t *lock)
{
  int err;

  err = pthread_mutex_lock (&lock->lock);
  if (err != 0)
    return err;
   
  while (!(lock->runcount == 0))
    {
       
      lock->waiting_writers_count++;
      err = pthread_cond_wait (&lock->waiting_writers, &lock->lock);
      if (err != 0)
        {
          lock->waiting_writers_count--;
          pthread_mutex_unlock (&lock->lock);
          return err;
        }
      lock->waiting_writers_count--;
    }
  lock->runcount--;  
  return pthread_mutex_unlock (&lock->lock);
}

int
glthread_rwlock_unlock_multithreaded (gl_rwlock_t *lock)
{
  int err;

  err = pthread_mutex_lock (&lock->lock);
  if (err != 0)
    return err;
  if (lock->runcount < 0)
    {
       
      if (!(lock->runcount == -1))
        {
          pthread_mutex_unlock (&lock->lock);
          return EINVAL;
        }
      lock->runcount = 0;
    }
  else
    {
       
      if (!(lock->runcount > 0))
        {
          pthread_mutex_unlock (&lock->lock);
          return EINVAL;
        }
      lock->runcount--;
    }
  if (lock->runcount == 0)
    {
       
      if (lock->waiting_writers_count > 0)
        {
           
          err = pthread_cond_signal (&lock->waiting_writers);
          if (err != 0)
            {
              pthread_mutex_unlock (&lock->lock);
              return err;
            }
        }
      else
        {
           
          err = pthread_cond_broadcast (&lock->waiting_readers);
          if (err != 0)
            {
              pthread_mutex_unlock (&lock->lock);
              return err;
            }
        }
    }
  return pthread_mutex_unlock (&lock->lock);
}

int
glthread_rwlock_destroy_multithreaded (gl_rwlock_t *lock)
{
  int err;

  err = pthread_mutex_destroy (&lock->lock);
  if (err != 0)
    return err;
  err = pthread_cond_destroy (&lock->waiting_readers);
  if (err != 0)
    return err;
  err = pthread_cond_destroy (&lock->waiting_writers);
  if (err != 0)
    return err;
  return 0;
}

# endif

 

# if HAVE_PTHREAD_MUTEX_RECURSIVE

#  if defined PTHREAD_RECURSIVE_MUTEX_INITIALIZER || defined PTHREAD_RECURSIVE_MUTEX_INITIALIZER_NP

int
glthread_recursive_lock_init_multithreaded (gl_recursive_lock_t *lock)
{
  pthread_mutexattr_t attributes;
  int err;

  err = pthread_mutexattr_init (&attributes);
  if (err != 0)
    return err;
  err = pthread_mutexattr_settype (&attributes, PTHREAD_MUTEX_RECURSIVE);
  if (err != 0)
    {
      pthread_mutexattr_destroy (&attributes);
      return err;
    }
  err = pthread_mutex_init (lock, &attributes);
  if (err != 0)
    {
      pthread_mutexattr_destroy (&attributes);
      return err;
    }
  err = pthread_mutexattr_destroy (&attributes);
  if (err != 0)
    return err;
  return 0;
}

#  else

int
glthread_recursive_lock_init_multithreaded (gl_recursive_lock_t *lock)
{
  pthread_mutexattr_t attributes;
  int err;

  err = pthread_mutexattr_init (&attributes);
  if (err != 0)
    return err;
  err = pthread_mutexattr_settype (&attributes, PTHREAD_MUTEX_RECURSIVE);
  if (err != 0)
    {
      pthread_mutexattr_destroy (&attributes);
      return err;
    }
  err = pthread_mutex_init (&lock->recmutex, &attributes);
  if (err != 0)
    {
      pthread_mutexattr_destroy (&attributes);
      return err;
    }
  err = pthread_mutexattr_destroy (&attributes);
  if (err != 0)
    return err;
  lock->initialized = 1;
  return 0;
}

int
glthread_recursive_lock_lock_multithreaded (gl_recursive_lock_t *lock)
{
  if (!lock->initialized)
    {
      int err;

      err = pthread_mutex_lock (&lock->guard);
      if (err != 0)
        return err;
      if (!lock->initialized)
        {
          err = glthread_recursive_lock_init_multithreaded (lock);
          if (err != 0)
            {
              pthread_mutex_unlock (&lock->guard);
              return err;
            }
        }
      err = pthread_mutex_unlock (&lock->guard);
      if (err != 0)
        return err;
    }
  return pthread_mutex_lock (&lock->recmutex);
}

int
glthread_recursive_lock_unlock_multithreaded (gl_recursive_lock_t *lock)
{
  if (!lock->initialized)
    return EINVAL;
  return pthread_mutex_unlock (&lock->recmutex);
}

int
glthread_recursive_lock_destroy_multithreaded (gl_recursive_lock_t *lock)
{
  int err;

  if (!lock->initialized)
    return EINVAL;
  err = pthread_mutex_destroy (&lock->recmutex);
  if (err != 0)
    return err;
  lock->initialized = 0;
  return 0;
}

#  endif

# else

int
glthread_recursive_lock_init_multithreaded (gl_recursive_lock_t *lock)
{
  int err;

  err = pthread_mutex_init (&lock->mutex, NULL);
  if (err != 0)
    return err;
  lock->owner = (pthread_t) 0;
  lock->depth = 0;
  return 0;
}

int
glthread_recursive_lock_lock_multithreaded (gl_recursive_lock_t *lock)
{
  pthread_t self = pthread_self ();
  if (lock->owner != self)
    {
      int err;

      err = pthread_mutex_lock (&lock->mutex);
      if (err != 0)
        return err;
      lock->owner = self;
    }
  if (++(lock->depth) == 0)  
    {
      lock->depth--;
      return EAGAIN;
    }
  return 0;
}

int
glthread_recursive_lock_unlock_multithreaded (gl_recursive_lock_t *lock)
{
  if (lock->owner != pthread_self ())
    return EPERM;
  if (lock->depth == 0)
    return EINVAL;
  if (--(lock->depth) == 0)
    {
      lock->owner = (pthread_t) 0;
      return pthread_mutex_unlock (&lock->mutex);
    }
  else
    return 0;
}

int
glthread_recursive_lock_destroy_multithreaded (gl_recursive_lock_t *lock)
{
  if (lock->owner != (pthread_t) 0)
    return EBUSY;
  return pthread_mutex_destroy (&lock->mutex);
}

# endif

 

static const pthread_once_t fresh_once = PTHREAD_ONCE_INIT;

int
glthread_once_singlethreaded (pthread_once_t *once_control)
{
   
  char *firstbyte = (char *)once_control;
  if (*firstbyte == *(const char *)&fresh_once)
    {
       
      *firstbyte = ~ *(const char *)&fresh_once;
      return 1;
    }
  else
    return 0;
}

# if !(PTHREAD_IN_USE_DETECTION_HARD || USE_POSIX_THREADS_WEAK)

int
glthread_once_multithreaded (pthread_once_t *once_control,
                             void (*init_function) (void))
{
  int err = pthread_once (once_control, init_function);
  if (err == ENOSYS)
    {
       
      if (glthread_once_singlethreaded (once_control))
        init_function ();
      return 0;
    }
  return err;
}

# endif

#endif

 

#if USE_WINDOWS_THREADS

#endif

 
