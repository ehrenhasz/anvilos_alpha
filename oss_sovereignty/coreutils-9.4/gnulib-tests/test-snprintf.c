 

#include <config.h>

#include <stdio.h>

#include "signature.h"
SIGNATURE_CHECK (snprintf, int, (char *, size_t, char const *, ...));

#include <string.h>

#include "macros.h"

int
main (int argc, char *argv[])
{
  char buf[8];
  int size;
  int retval;

  retval = snprintf (NULL, 0, "%d", 12345);
  ASSERT (retval == 5);

  for (size = 0; size <= 8; size++)
    {
      memcpy (buf, "DEADBEEF", 8);
      retval = snprintf (buf, size, "%d", 12345);
      ASSERT (retval == 5);
      if (size < 6)
        {
          if (size > 0)
            {
              ASSERT (memcmp (buf, "12345", size - 1) == 0);
              ASSERT (buf[size - 1] == '\0' || buf[size - 1] == '0' + size);
            }
#if !CHECK_SNPRINTF_POSIX
          if (size > 0)
#endif
            ASSERT (memcmp (buf + size, &"DEADBEEF"[size], 8 - size) == 0);
        }
      else
        {
          ASSERT (memcmp (buf, "12345\0EF", 8) == 0);
        }
    }

   
  {
    char result[100];
    retval = snprintf (result, sizeof (result), "%2$d %1$d", 33, 55);
    ASSERT (strcmp (result, "55 33") == 0);
    ASSERT (retval == strlen (result));
  }

  return 0;
}
