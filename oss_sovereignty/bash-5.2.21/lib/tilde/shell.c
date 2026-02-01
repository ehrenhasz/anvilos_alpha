 

 

#if defined (HAVE_CONFIG_H)
#  include <config.h>
#endif

#if defined (HAVE_UNISTD_H)
#  ifdef _MINIX
#    include <sys/types.h>
#  endif
#  include <unistd.h>
#endif  

#if defined (HAVE_STDLIB_H)
#  include <stdlib.h>
#else
#  include "ansi_stdlib.h"
#endif  

#if defined (HAVE_STRING_H)
#  include <string.h>
#else
#  include <strings.h>
#endif  

#include <pwd.h>

#if !defined (HAVE_GETPW_DECLS)
extern struct passwd *getpwuid ();
#endif  

char *
get_env_value (char *varname)
{
  return ((char *)getenv (varname));
}

 
char *
get_home_dir (void)
{
  static char *home_dir = (char *)NULL;
  struct passwd *entry;

  if (home_dir)
    return (home_dir);

#if defined (HAVE_GETPWUID)
  entry = getpwuid (getuid ());
  if (entry)
    home_dir = savestring (entry->pw_dir);
#endif

#if defined (HAVE_GETPWENT)
  endpwent ();		 
#endif

  return (home_dir);
}
