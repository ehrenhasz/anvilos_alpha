 

 

#include <config.h>

#ifdef HAVE_STDLIB_H
#  include <stdlib.h>
#endif

#include "bashansi.h"
#include "shmbutil.h"

extern int locale_mb_cur_max;
extern int locale_utf8locale;

#if defined (HANDLE_MULTIBYTE)

char *
utf8_mbschr (s, c)
     const char *s;
     int c;
{
  return strchr (s, c);		 
}

int
utf8_mbscmp (s1, s2)
     const char *s1, *s2;
{
   
  return strcmp (s1, s2);
}

char *
utf8_mbsmbchar (str)
     const char *str;
{
  register char *s;

  for (s = (char *)str; *s; s++)
    if ((*s & 0xc0) == 0x80)
      return s;
  return (0);
}

int
utf8_mbsnlen(src, srclen, maxlen)
     const char *src;
     size_t srclen;
     int maxlen;
{
  register int sind, count;

  for (sind = count = 0; src[sind] && sind <= maxlen; sind++)
    {
      if ((src[sind] & 0xc0) != 0x80)
	count++;
    }
  return (count);
}

 
int
utf8_mblen (s, n)
     const char *s;
     size_t n;
{
  unsigned char c, c1, c2, c3;

  if (s == 0)
    return (0);	 
  if (n <= 0)
    return (-1);

  c = (unsigned char)*s;
  if (c < 0x80)
    return (c != 0);
  if (c >= 0xc2)
    {
      c1 = (unsigned char)s[1];
      if (c < 0xe0)
	{
	  if (n == 1)
	    return -2;

	   

	  if (n >= 2 && (c1 ^ 0x80) < 0x40)		 
	    return 2;
	}
      else if (c < 0xf0)
	{
	  if (n == 1)
	    return -2;

	   

	  if ((c1 ^ 0x80) < 0x40
		&& (c >= 0xe1 || c1 >= 0xa0)
		&& (c != 0xed || c1 < 0xa0))
	    {
	      if (n == 2)
		return -2;		 

	      c2 = (unsigned char)s[2];
	      if ((c2 ^ 0x80) < 0x40)
		 return 3;
	    }
	}
      else if (c <= 0xf4)
	{
	  if (n == 1)
	    return -2;
	 
	   
	  if (((c1 ^ 0x80) < 0x40) 
		&& (c >= 0xf1 || c1 >= 0x90)
		&& (c < 0xf4 || (c == 0xf4 && c1 < 0x90)))
	    {
	      if (n == 2)
		return -2;		 

	      c2 = (unsigned char)s[2];
	      if ((c2 ^ 0x80) < 0x40)
		{
		  if (n == 3)
		    return -2;

		  c3 = (unsigned char)s[3];
	 	  if ((c3 ^ 0x80) < 0x40)
	  	    return 4;
		}
	    }
	}
    }
   
  return -1;
}

 
size_t
utf8_mbstrlen(s)
     const char *s;
{
  size_t clen, nc;
  int mb_cur_max;

  nc = 0;
  mb_cur_max = MB_CUR_MAX;
  while (*s && (clen = (size_t)utf8_mblen(s, mb_cur_max)) != 0)
    {
      if (MB_INVALIDCH(clen))
	clen = 1;	 

      s += clen;
      nc++;
    }
  return nc;
}

#endif
