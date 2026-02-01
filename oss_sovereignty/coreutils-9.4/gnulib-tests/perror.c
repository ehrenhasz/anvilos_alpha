 
#include <stdio.h>

#include <errno.h>
#include <stdlib.h>
#include <string.h>

#include "strerror-override.h"

 
#undef fprintf

void
perror (const char *string)
{
  char stackbuf[STACKBUF_LEN];
  int ret;

   
  ret = strerror_r (errno, stackbuf, sizeof stackbuf);
  if (ret == ERANGE)
    abort ();

  if (string != NULL && *string != '\0')
    fprintf (stderr, "%s: %s\n", string, stackbuf);
  else
    fprintf (stderr, "%s\n", stackbuf);
}
