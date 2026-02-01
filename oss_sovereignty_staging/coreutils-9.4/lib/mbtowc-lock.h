 

 

static inline int
mbtowc_unlocked (wchar_t *pwc, const char *p, size_t m)
{
   
#undef gl_get_mbtowc_lock

#if GNULIB_MBRTOWC_SINGLE_THREAD

 

static int
mbtowc_with_lock (wchar_t *pwc, const char *p, size_t m)
{
  return mbtowc_unlocked (pwc, p, m);
}

#elif defined _WIN32 && !defined __CYGWIN__

extern __declspec(dllimport) CRITICAL_SECTION *gl_get_mbtowc_lock (void);

static int
mbtowc_with_lock (wchar_t *pwc, const char *p, size_t m)
{
  CRITICAL_SECTION *lock = gl_get_mbtowc_lock ();
  int ret;

  EnterCriticalSection (lock);
  ret = mbtowc_unlocked (pwc, p, m);
  LeaveCriticalSection (lock);

  return ret;
}

#elif HAVE_PTHREAD_API  

extern
# if defined _WIN32 || defined __CYGWIN__
  __declspec(dllimport)
# endif
  pthread_mutex_t *gl_get_mbtowc_lock (void);

# if HAVE_WEAK_SYMBOLS  

    
#  pragma weak pthread_mutex_lock
#  pragma weak pthread_mutex_unlock

    
#  pragma weak pthread_mutexattr_gettype
    
#  define pthread_in_use() \
     (pthread_mutexattr_gettype != NULL || c11_threads_in_use ())

# else
#  define pthread_in_use() 1
# endif

static int
mbtowc_with_lock (wchar_t *pwc, const char *p, size_t m)
{
  if (pthread_in_use())
    {
      pthread_mutex_t *lock = gl_get_mbtowc_lock ();
      int ret;

      if (pthread_mutex_lock (lock))
        abort ();
      ret = mbtowc_unlocked (pwc, p, m);
      if (pthread_mutex_unlock (lock))
        abort ();

      return ret;
    }
  else
    return mbtowc_unlocked (pwc, p, m);
}

#elif HAVE_THREADS_H

extern mtx_t *gl_get_mbtowc_lock (void);

static int
mbtowc_with_lock (wchar_t *pwc, const char *p, size_t m)
{
  mtx_t *lock = gl_get_mbtowc_lock ();
  int ret;

  if (mtx_lock (lock) != thrd_success)
    abort ();
  ret = mbtowc_unlocked (pwc, p, m);
  if (mtx_unlock (lock) != thrd_success)
    abort ();

  return ret;
}

#endif
