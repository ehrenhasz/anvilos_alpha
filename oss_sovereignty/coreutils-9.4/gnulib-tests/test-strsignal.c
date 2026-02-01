 

#include <config.h>

#include <string.h>

#include "signature.h"
SIGNATURE_CHECK (strsignal, char *, (int));

#include <signal.h>

#include "macros.h"

#if HAVE_DECL_SYS_SIGLIST
# define ASSERT_DESCRIPTION(actual, expected)
#else
 
# define ASSERT_DESCRIPTION(actual, expected) \
   ASSERT (strncmp (actual, expected, strlen (expected)) == 0)
#endif

int
main (void)
{
   
  const char *str;

   

#ifdef SIGHUP
  str = strsignal (SIGHUP);
  ASSERT (str);
  ASSERT (*str);
  ASSERT_DESCRIPTION (str, "Hangup");
#endif

#ifdef SIGINT
  str = strsignal (SIGINT);
  ASSERT (str);
  ASSERT (*str);
  ASSERT_DESCRIPTION (str, "Interrupt");
#endif

   

  str = strsignal (-1);
  ASSERT (str);
  ASSERT (str != (char *) -1);
  ASSERT (strlen (str));

  str = strsignal (9249234);
  ASSERT (str);
  ASSERT (str != (char *) -1);
  ASSERT (strlen (str));

  return 0;
}
