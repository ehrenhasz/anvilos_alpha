 

#include <config.h>

#if !(defined _WIN32 && !defined __CYGWIN__)
 

 
#include <unistd.h>

#ifdef HAVE_UNAME
# include <sys/utsname.h>
#endif

#include <string.h>

 

#include <stddef.h>

int
gethostname (char *name, size_t len)
{
#ifdef HAVE_UNAME
  struct utsname uts;

  if (uname (&uts) == -1)
    return -1;
  if (len > sizeof (uts.nodename))
    {
       
      name[sizeof (uts.nodename)] = '\0';
      len = sizeof (uts.nodename);
    }
  strncpy (name, uts.nodename, len);
#else
  strcpy (name, "");             
#endif
  return 0;
}

#else
 

#define WIN32_LEAN_AND_MEAN
 
#include <unistd.h>

 
#include <limits.h>

 
#include "w32sock.h"

#include "sockets.h"

#undef gethostname

int
rpl_gethostname (char *name, size_t len)
{
  int r;

  if (len > INT_MAX)
    len = INT_MAX;
  gl_sockets_startup (SOCKETS_1_1);
  r = gethostname (name, (int) len);
  if (r < 0)
    set_winsock_errno ();

  return r;
}

#endif
