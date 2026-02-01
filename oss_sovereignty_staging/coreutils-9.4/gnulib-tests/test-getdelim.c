 

#include <config.h>

#include <stdio.h>

#include "signature.h"
SIGNATURE_CHECK (getdelim, ssize_t, (char **, size_t *, int, FILE *));

#include <stdlib.h>
#include <string.h>

#include "macros.h"

int
main (void)
{
  FILE *f;
  char *line;
  size_t len;
  ssize_t result;

   
  f = fopen ("test-getdelim.txt", "wb");
  if (!f || fwrite ("anAnbcnd\0f", 1, 10, f) != 10 || fclose (f) != 0)
    {
      fputs ("Failed to create sample file.\n", stderr);
      remove ("test-getdelim.txt");
      return 1;
    }
  f = fopen ("test-getdelim.txt", "rb");
  if (!f)
    {
      fputs ("Failed to reopen sample file.\n", stderr);
      remove ("test-getdelim.txt");
      return 1;
    }

   
  line = NULL;
  len = 0;
  result = getdelim (&line, &len, 'n', f);
  ASSERT (result == 2);
  ASSERT (strcmp (line, "an") == 0);
  ASSERT (2 < len);
  free (line);

   
  line = NULL;
  len = (size_t)(~0) / 4;
  result = getdelim (&line, &len, 'n', f);
  ASSERT (result == 2);
  ASSERT (strcmp (line, "An") == 0);
  ASSERT (2 < len);
  free (line);

   
  line = malloc (1);
  len = 1;
  result = getdelim (&line, &len, 'n', f);
  ASSERT (result == 3);
  ASSERT (strcmp (line, "bcn") == 0);
  ASSERT (3 < len);

   
  result = getdelim (&line, &len, 'n', f);
  ASSERT (result == 3);
  ASSERT (memcmp (line, "d\0f", 4) == 0);
  ASSERT (3 < len);

  result = getdelim (&line, &len, 'n', f);
  ASSERT (result == -1);

  free (line);
  fclose (f);
  remove ("test-getdelim.txt");
  return 0;
}
