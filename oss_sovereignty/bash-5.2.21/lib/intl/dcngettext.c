 

 

#ifdef HAVE_CONFIG_H
# include <config.h>
#endif

#include "gettextP.h"
#ifdef _LIBC
# include <libintl.h>
#else
# include "libgnuintl.h"
#endif

 

 
#ifdef _LIBC
# define DCNGETTEXT __dcngettext
# define DCIGETTEXT __dcigettext
#else
# define DCNGETTEXT libintl_dcngettext
# define DCIGETTEXT libintl_dcigettext
#endif

 
char *
DCNGETTEXT (domainname, msgid1, msgid2, n, category)
     const char *domainname;
     const char *msgid1;
     const char *msgid2;
     unsigned long int n;
     int category;
{
  return DCIGETTEXT (domainname, msgid1, msgid2, 1, n, category);
}

#ifdef _LIBC
 
weak_alias (__dcngettext, dcngettext);
#endif
