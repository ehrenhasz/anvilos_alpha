 

 

#ifdef HAVE_CONFIG_H
# include <config.h>
#endif

#if !defined (HAVE_STRNLEN)

#include <sys/types.h>

#if defined (HAVE_UNISTD_H)
#  include <unistd.h>
#endif

#include <stdc.h>

 
size_t
strnlen (s, maxlen)
     register const char *s;
     size_t maxlen;
{
  register const char *e;
  size_t n;

  for (e = s, n = 0; *e && n < maxlen; e++, n++)
    ;
  return n;
}
#endif
