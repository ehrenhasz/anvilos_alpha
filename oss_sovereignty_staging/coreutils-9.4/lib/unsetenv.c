 
#define _GL_ARG_NONNULL(params)

#include <config.h>

 
#include <stdlib.h>

#include <errno.h>
#if !_LIBC
# define __set_errno(ev) ((errno) = (ev))
#endif

#include <string.h>
#include <unistd.h>

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
# define unsetenv __unsetenv
#endif

#if _LIBC || !HAVE_UNSETENV

int
unsetenv (const char *name)
{
  size_t len;
  char **ep;

  if (name == NULL || *name == '\0' || strchr (name, '=') != NULL)
    {
      __set_errno (EINVAL);
      return -1;
    }

  len = strlen (name);

  LOCK;

  ep = __environ;
  while (*ep != NULL)
    if (!strncmp (*ep, name, len) && (*ep)[len] == '=')
      {
         
        char **dp = ep;

        do
          dp[0] = dp[1];
        while (*dp++);
         
      }
    else
      ++ep;

  UNLOCK;

  return 0;
}

#ifdef _LIBC
# undef unsetenv
weak_alias (__unsetenv, unsetenv)
#endif

#else  

# undef unsetenv
# if !HAVE_DECL_UNSETENV
#  if VOID_UNSETENV
extern void unsetenv (const char *);
#  else
extern int unsetenv (const char *);
#  endif
# endif

 
int
rpl_unsetenv (const char *name)
{
  int result = 0;
  if (!name || !*name || strchr (name, '='))
    {
      errno = EINVAL;
      return -1;
    }
  while (getenv (name))
# if !VOID_UNSETENV
    result =
# endif
      unsetenv (name);
  return result;
}

#endif  
