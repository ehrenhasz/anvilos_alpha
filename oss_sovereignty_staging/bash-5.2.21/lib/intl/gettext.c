 

 

#ifdef HAVE_CONFIG_H
# include <config.h>
#endif

#ifdef _LIBC
# define __need_NULL
# include <stddef.h>
#else
# include <stdlib.h>		 
#endif

#include "gettextP.h"
#ifdef _LIBC
# include <libintl.h>
#else
# include "libgnuintl.h"
#endif

 

 
#ifdef _LIBC
# define GETTEXT __gettext
# define DCGETTEXT INTUSE(__dcgettext)
#else
# define GETTEXT libintl_gettext
# define DCGETTEXT libintl_dcgettext
#endif

 
char *
GETTEXT (msgid)
     const char *msgid;
{
  return DCGETTEXT (NULL, msgid, LC_MESSAGES);
}

#ifdef _LIBC
 
weak_alias (__gettext, gettext);
#endif
