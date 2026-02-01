 
# define _GL_ARG_NONNULL(params)

# define _GL_USE_STDLIB_ALLOC 1
# include <config.h>
#endif

#include <alloca.h>

 
#include <stdlib.h>

#include <errno.h>
#ifndef __set_errno
# define __set_errno(ev) ((errno) = (ev))
#endif

#include <string.h>
#if _LIBC || HAVE_UNISTD_H
# include <unistd.h>
#endif

#if !_LIBC
# include "malloca.h"
#endif

#if _LIBC || !HAVE_SETENV

#if !_LIBC
# define __environ      environ
#endif

#if _LIBC
 
# include <bits/libc-lock.h>
__libc_lock_define_initialized (static, envlock)
# define LOCK   __libc_lock_lock (envlock)
# define UNLOCK __libc_lock_unlock (envlock)
#else
# define LOCK
# define UNLOCK
#endif

 
#ifdef _LIBC
# define setenv __setenv
# define clearenv __clearenv
# define tfind __tfind
# define tsearch __tsearch
#endif

 
#if defined _LIBC || (defined HAVE_SEARCH_H && defined HAVE_TSEARCH \
                      && (defined __GNUC__ || defined __clang__))
# define USE_TSEARCH    1
# include <search.h>
typedef int (*compar_fn_t) (const void *, const void *);

 
static void *known_values;

# define KNOWN_VALUE(Str) \
  ({                                                                          \
    void *value = tfind (Str, &known_values, (compar_fn_t) strcmp);           \
    value != NULL ? *(char **) value : NULL;                                  \
  })
# define STORE_VALUE(Str) \
  tsearch (Str, &known_values, (compar_fn_t) strcmp)

#else
# undef USE_TSEARCH

# define KNOWN_VALUE(Str) NULL
# define STORE_VALUE(Str) do { } while (0)

#endif


 
static char **last_environ;


 
int
__add_to_environ (const char *name, const char *value, const char *combined,
                  int replace)
{
  char **ep;
  size_t size;
  const size_t namelen = strlen (name);
  const size_t vallen = value != NULL ? strlen (value) + 1 : 0;

  LOCK;

   
  ep = __environ;

  size = 0;
  if (ep != NULL)
    {
      for (; *ep != NULL; ++ep)
        if (!strncmp (*ep, name, namelen) && (*ep)[namelen] == '=')
          break;
        else
          ++size;
    }

  if (ep == NULL || *ep == NULL)
    {
      char **new_environ;
#ifdef USE_TSEARCH
      char *new_value;
#endif

       
      new_environ =
        (char **) (last_environ == NULL
                   ? malloc ((size + 2) * sizeof (char *))
                   : realloc (last_environ, (size + 2) * sizeof (char *)));
      if (new_environ == NULL)
        {
           
          __set_errno (ENOMEM);
          UNLOCK;
          return -1;
        }

       
      if (combined != NULL)
         
        new_environ[size] = (char *) combined;
      else
        {
           
#ifdef USE_TSEARCH
# ifdef _LIBC
          new_value = (char *) alloca (namelen + 1 + vallen);
          __mempcpy (__mempcpy (__mempcpy (new_value, name, namelen), "=", 1),
                     value, vallen);
# else
          new_value = (char *) malloca (namelen + 1 + vallen);
          if (new_value == NULL)
            {
              __set_errno (ENOMEM);
              UNLOCK;
              return -1;
            }
          memcpy (new_value, name, namelen);
          new_value[namelen] = '=';
          memcpy (&new_value[namelen + 1], value, vallen);
# endif

          new_environ[size] = KNOWN_VALUE (new_value);
          if (new_environ[size] == NULL)
#endif
            {
              new_environ[size] = (char *) malloc (namelen + 1 + vallen);
              if (new_environ[size] == NULL)
                {
#if defined USE_TSEARCH && !defined _LIBC
                  freea (new_value);
#endif
                  __set_errno (ENOMEM);
                  UNLOCK;
                  return -1;
                }

#ifdef USE_TSEARCH
              memcpy (new_environ[size], new_value, namelen + 1 + vallen);
#else
              memcpy (new_environ[size], name, namelen);
              new_environ[size][namelen] = '=';
              memcpy (&new_environ[size][namelen + 1], value, vallen);
#endif
               
              STORE_VALUE (new_environ[size]);
            }
#if defined USE_TSEARCH && !defined _LIBC
          freea (new_value);
#endif
        }

      if (__environ != last_environ)
        memcpy ((char *) new_environ, (char *) __environ,
                size * sizeof (char *));

      new_environ[size + 1] = NULL;

      last_environ = __environ = new_environ;
    }
  else if (replace)
    {
      char *np;

       
      if (combined != NULL)
        np = (char *) combined;
      else
        {
#ifdef USE_TSEARCH
          char *new_value;
# ifdef _LIBC
          new_value = alloca (namelen + 1 + vallen);
          __mempcpy (__mempcpy (__mempcpy (new_value, name, namelen), "=", 1),
                     value, vallen);
# else
          new_value = malloca (namelen + 1 + vallen);
          if (new_value == NULL)
            {
              __set_errno (ENOMEM);
              UNLOCK;
              return -1;
            }
          memcpy (new_value, name, namelen);
          new_value[namelen] = '=';
          memcpy (&new_value[namelen + 1], value, vallen);
# endif

          np = KNOWN_VALUE (new_value);
          if (np == NULL)
#endif
            {
              np = (char *) malloc (namelen + 1 + vallen);
              if (np == NULL)
                {
#if defined USE_TSEARCH && !defined _LIBC
                  freea (new_value);
#endif
                  __set_errno (ENOMEM);
                  UNLOCK;
                  return -1;
                }

#ifdef USE_TSEARCH
              memcpy (np, new_value, namelen + 1 + vallen);
#else
              memcpy (np, name, namelen);
              np[namelen] = '=';
              memcpy (&np[namelen + 1], value, vallen);
#endif
               
              STORE_VALUE (np);
            }
#if defined USE_TSEARCH && !defined _LIBC
          freea (new_value);
#endif
        }

      *ep = np;
    }

  UNLOCK;

  return 0;
}

int
setenv (const char *name, const char *value, int replace)
{
  if (name == NULL || *name == '\0' || strchr (name, '=') != NULL)
    {
      __set_errno (EINVAL);
      return -1;
    }

  return __add_to_environ (name, value, NULL, replace);
}

 
int
clearenv (void)
{
  LOCK;

  if (__environ == last_environ && __environ != NULL)
    {
       
      free (__environ);
      last_environ = NULL;
    }

   
  __environ = NULL;

  UNLOCK;

  return 0;
}

#ifdef _LIBC
static void
free_mem (void)
{
   
  clearenv ();

   
  __tdestroy (known_values, free);
  known_values = NULL;
}
text_set_element (__libc_subfreeres, free_mem);


# undef setenv
# undef clearenv
weak_alias (__setenv, setenv)
weak_alias (__clearenv, clearenv)
#endif

#endif  

 
#if HAVE_SETENV

# undef setenv
# if !HAVE_DECL_SETENV
extern int setenv (const char *, const char *, int);
# endif
# define STREQ(a, b) (strcmp (a, b) == 0)

int
rpl_setenv (const char *name, const char *value, int replace)
{
  int result;
  if (!name || !*name || strchr (name, '='))
    {
      errno = EINVAL;
      return -1;
    }
   
  result = setenv (name, value, replace);
  if (result == 0 && replace && *value == '=')
    {
      char *tmp = getenv (name);
      if (!STREQ (tmp, value))
        {
          int saved_errno;
          size_t len = strlen (value);
          tmp = malloca (len + 2);
          if (tmp == NULL)
            {
              errno = ENOMEM;
              return -1;
            }
           
          *tmp = '=';
          memcpy (tmp + 1, value, len + 1);
          result = setenv (name, tmp, replace);
          saved_errno = errno;
          freea (tmp);
          errno = saved_errno;
        }
    }
  return result;
}

#endif  
