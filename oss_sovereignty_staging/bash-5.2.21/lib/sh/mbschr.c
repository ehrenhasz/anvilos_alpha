 

 

#include <config.h>

#ifdef HAVE_STDLIB_H
#  include <stdlib.h>
#endif

#include "bashansi.h"
#include "shmbutil.h"

extern int locale_mb_cur_max;
extern int locale_utf8locale;

#undef mbschr

extern char *utf8_mbschr (const char *, int);	 

 

char *
#if defined (PROTOTYPES)
mbschr (const char *s, int c)
#else
mbschr (s, c)
     const char *s;
     int c;
#endif
{
#if HANDLE_MULTIBYTE
  char *pos;
  mbstate_t state;
  size_t strlength, mblength;

  if (locale_utf8locale && c < 0x80)
    return (utf8_mbschr (s, c));		 

   
  if ((unsigned char)c >= '0' && locale_mb_cur_max > 1)
    {
      pos = (char *)s;
      memset (&state, '\0', sizeof(mbstate_t));
      strlength = strlen (s);

      while (strlength > 0)
	{
	  if (is_basic (*pos))
	    mblength = 1;
	  else
	    {
	      mblength = mbrlen (pos, strlength, &state);
	      if (mblength == (size_t)-2 || mblength == (size_t)-1 || mblength == (size_t)0)
	        mblength = 1;
	    }

	  if (mblength == 1 && c == (unsigned char)*pos)
	    return pos;

	  strlength -= mblength;
	  pos += mblength;
	}

      return ((char *)NULL);
    }
  else
#endif
  return (strchr (s, c));
}
