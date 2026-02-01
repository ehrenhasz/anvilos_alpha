 

 

#include "config.h"

#include "bashtypes.h"

#include "posixtime.h"

#if defined (HAVE_UNISTD_H)
#include <unistd.h>
#endif

#include <stdio.h>
#include "chartypes.h"

#include "shell.h"
#include "builtins.h"

#define DECIMAL	'.'		 

#define RETURN(x) \
do { \
  if (ip) *ip = ipart * mult; \
  if (up) *up = upart; \
  if (ep) *ep = p; \
  return (x); \
} while (0)

 
static int multiplier[7] = { 1, 100000, 10000, 1000, 100, 10, 1 };

 
int
uconvert(s, ip, up, ep)
     char *s;
     long *ip, *up;
     char **ep;
{
  int n, mult;
  long ipart, upart;
  char *p;

  ipart = upart = 0;
  mult = 1;

  if (s && (*s == '-' || *s == '+'))
    {
      mult = (*s == '-') ? -1 : 1;
      p = s + 1;
    }
  else
    p = s;

  for ( ; p && *p; p++)
    {
      if (*p == DECIMAL)		 
	break;
      if (DIGIT(*p) == 0)
	RETURN(0);
      ipart = (ipart * 10) + (*p - '0');
    }

  if (p == 0 || *p == 0)	 
    RETURN(1);

  if (*p == DECIMAL)
    p++;

   
  for (n = 0; n < 6 && p[n]; n++)
    {
      if (DIGIT(p[n]) == 0)
	{
	  if (ep)
	    {
	      upart *= multiplier[n];
	      p += n;		 
	    }
	  RETURN(0);
	}
      upart = (upart * 10) + (p[n] - '0');
    }

   
  upart *= multiplier[n];

  if (n == 6 && p[6] >= '5' && p[6] <= '9')
    upart++;			 

  if (ep)
    {
      p += n;
      while (DIGIT(*p))
	p++;
    }

  RETURN(1);
}
