 

 

#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

#if defined (HAVE_UNISTD_H)
#  include <unistd.h>
#endif

#include <bashansi.h>
#include "shell.h"

char *
inttostr (i, buf, len)
     intmax_t i;
     char *buf;
     size_t len;
{
  return (fmtumax (i, 10, buf, len, 0));
}

 
char *
itos (i)
     intmax_t i;
{
  char *p, lbuf[INT_STRLEN_BOUND(intmax_t) + 1];

  p = fmtumax (i, 10, lbuf, sizeof(lbuf), 0);
  return (savestring (p));
}

 
char *
mitos (i)
     intmax_t i;
{
  char *p, lbuf[INT_STRLEN_BOUND(intmax_t) + 1];

  p = fmtumax (i, 10, lbuf, sizeof(lbuf), 0);
  return (strdup (p));
}

char *
uinttostr (i, buf, len)
     uintmax_t i;
     char *buf;
     size_t len;
{
  return (fmtumax (i, 10, buf, len, FL_UNSIGNED));
}

 
char *
uitos (i)
     uintmax_t i;
{
  char *p, lbuf[INT_STRLEN_BOUND(uintmax_t) + 1];

  p = fmtumax (i, 10, lbuf, sizeof(lbuf), FL_UNSIGNED);
  return (savestring (p));
}
