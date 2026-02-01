 

 

#ifdef HAVE_CONFIG_H
# include <config.h>
#endif

#include <locale.h>

#include "gettextP.h"
#ifdef _LIBC
# include <libintl.h>
#else
# include "libgnuintl.h"
#endif

 

 
#ifdef _LIBC
# define DGETTEXT __dgettext
# define DCGETTEXT INTUSE(__dcgettext)
#else
# define DGETTEXT libintl_dgettext
# define DCGETTEXT libintl_dcgettext
#endif

 
char *
DGETTEXT (domainname, msgid)
     const char *domainname;
     const char *msgid;
{
  return DCGETTEXT (domainname, msgid, LC_MESSAGES);
}

#ifdef _LIBC
 
weak_alias (__dgettext, dgettext);
#endif
