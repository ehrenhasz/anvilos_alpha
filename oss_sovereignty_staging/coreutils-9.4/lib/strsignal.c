 
#include <string.h>

#include <signal.h>
#include <stdio.h>
#include <stdlib.h>

#ifdef _LIBC
# include <libintl.h>
#else  
# include "gettext.h"
# define _(msgid) gettext (msgid)
# define N_(msgid) gettext_noop (msgid)
#endif  

#ifdef _LIBC
# include <bits/libc-lock.h>
#else  
# include "glthread/lock.h"
# include "glthread/tls.h"
# define __libc_once_define(CLASS, NAME) gl_once_define (CLASS, NAME)
# define __libc_once(NAME, INIT) gl_once ((NAME), (INIT))
# define __libc_key_t gl_tls_key_t
# define __libc_getspecific(NAME) gl_tls_get ((NAME))
# define __libc_setspecific(NAME, POINTER) gl_tls_set ((NAME), (POINTER))
# define __snprintf snprintf
#endif  

#ifdef _LIBC

 
extern const char *const _sys_siglist[];
extern const char *const _sys_siglist_internal[] attribute_hidden;

#else  

 
# if HAVE_UNISTD_H
#  include <unistd.h>
# endif

# define INTUSE(x) (x)

# if HAVE_DECL_SYS_SIGLIST
#  undef _sys_siglist
#  define _sys_siglist sys_siglist
# else  
#  ifndef NSIG
#   define NSIG 32
#  endif  
#  if !HAVE_DECL__SYS_SIGLIST
static const char *_sys_siglist[NSIG];
#  endif
# endif  

#endif  

static __libc_key_t key;

 
#define BUFFERSIZ       100
static char local_buf[BUFFERSIZ];
static char *static_buf;

 
static void init (void);
static void free_key_mem (void *mem);
static char *getbuffer (void);


 
char *
strsignal (int signum)
{
  const char *desc;
  __libc_once_define (static, once);

   
  __libc_once (once, init);

  if (
#ifdef SIGRTMIN
      (signum >= SIGRTMIN && signum <= SIGRTMAX) ||
#endif
      signum < 0 || signum >= NSIG
      || (desc = INTUSE(_sys_siglist)[signum]) == NULL)
    {
      char *buffer = getbuffer ();
      int len;
#ifdef SIGRTMIN
      if (signum >= SIGRTMIN && signum <= SIGRTMAX)
        len = __snprintf (buffer, BUFFERSIZ - 1, _("Real-time signal %d"),
                          signum - (int) SIGRTMIN);
      else
#endif
        len = __snprintf (buffer, BUFFERSIZ - 1, _("Unknown signal %d"),
                          signum);
      if (len >= BUFFERSIZ)
        buffer = NULL;
      else
        buffer[len] = '\0';

      return buffer;
    }

  return (char *) _(desc);
}


 
static void
init (void)
{
#ifdef _LIBC
  if (__libc_key_create (&key, free_key_mem))
     
    static_buf = local_buf;
#else  
  gl_tls_key_init (key, free_key_mem);

# if !HAVE_DECL_SYS_SIGLIST
  memset (_sys_siglist, 0, NSIG * sizeof *_sys_siglist);

   
#  define init_sig(sig, abbrev, desc) \
  if (sig >= 0 && sig < NSIG) \
    _sys_siglist[sig] = desc;

#  include "siglist.h"

#  undef init_sig

# endif  
#endif  
}


 
static void
free_key_mem (void *mem)
{
  free (mem);
  __libc_setspecific (key, NULL);
}


 
static char *
getbuffer (void)
{
  char *result;

  if (static_buf != NULL)
    result = static_buf;
  else
    {
       
      result = __libc_getspecific (key);
      if (result == NULL)
        {
           
          result = malloc (BUFFERSIZ);
          if (result == NULL)
             
            result = local_buf;
          else
            __libc_setspecific (key, result);
        }
    }

  return result;
}
