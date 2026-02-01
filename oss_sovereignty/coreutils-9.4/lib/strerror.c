 
#include <string.h>

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "intprops.h"
#include "strerror-override.h"

 
#undef sprintf

char *
strerror (int n)
#undef strerror
{
  static char buf[STACKBUF_LEN];
  size_t len;

   
  const char *msg = strerror_override (n);
  if (msg)
    return (char *) msg;

  msg = strerror (n);

   
  if (!msg || !*msg)
    {
      static char const fmt[] = "Unknown error %d";
      static_assert (sizeof buf >= sizeof (fmt) + INT_STRLEN_BOUND (n));
      sprintf (buf, fmt, n);
      errno = EINVAL;
      return buf;
    }

   
  len = strlen (msg);
  if (sizeof buf <= len)
    abort ();

  memcpy (buf, msg, len + 1);
  return buf;
}
