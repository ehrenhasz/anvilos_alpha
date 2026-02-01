 

#include <config.h>

#include "getndelim2.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "macros.h"

int
main (void)
{
  FILE *f;
  char *line = NULL;
  size_t len = 0;
  ssize_t result;

   
  f = fopen ("test-getndelim2.txt", "wb+");
  if (!f || fwrite ("a\nbc\nd\0f", 1, 8, f) != 8)
    {
      fputs ("Failed to create sample file.\n", stderr);
      remove ("test-getndelim2.txt");
      return 1;
    }
  rewind (f);

   

   
  result = getndelim2 (&line, &len, 0, GETNLINE_NO_LIMIT, '\n', '\n', f);
  ASSERT (result == 2);
  ASSERT (strcmp (line, "a\n") == 0);
  ASSERT (2 < len);

   
  free (line);
  line = malloc (1);
  len = 0;
  result = getndelim2 (&line, &len, 0, GETNLINE_NO_LIMIT, EOF, '\n', f);
  ASSERT (result == 3);
  ASSERT (strcmp (line, "bc\n") == 0);
  ASSERT (3 < len);

   
  result = getndelim2 (&line, &len, 0, GETNLINE_NO_LIMIT, '\n', EOF, f);
  ASSERT (result == 3);
  ASSERT (memcmp (line, "d\0f", 4) == 0);
  ASSERT (3 < len);

  result = getndelim2 (&line, &len, 0, GETNLINE_NO_LIMIT, '\n', EOF, f);
  ASSERT (result == -1);

   

   
  free (line);
  rewind (f);
  line = malloc (8);
  memset (line, 'e', 8);
  len = 8;
  result = getndelim2 (&line, &len, 6, 10, 'd', 'd', f);
  ASSERT (result == 3);
  ASSERT (10 == len);
  ASSERT (strcmp (line, "eeeeeea\nb") == 0);

   
  result = getndelim2 (&line, &len, len, 1, EOF, EOF, f);
  ASSERT (result == -1);
  ASSERT (10 == len);
  ASSERT (strcmp (line, "eeeeeea\nb") == 0);

   
  result = getndelim2 (&line, &len, 0, GETNLINE_NO_LIMIT, EOF, EOF, f);
  ASSERT (result == 2);
  ASSERT (10 == len);
  ASSERT (memcmp (line, "\0f\0eeea\nb", 10) == 0);

  result = getndelim2 (&line, &len, 0, GETNLINE_NO_LIMIT, '\n', '\r', f);
  ASSERT (result == -1);

   
  rewind (f);
  {
    int i;
    for (i = 0; i < 16; i++)
      fprintf (f, "%500x%c", i + 0u, i % 2 ? '\n' : '\r');
  }
  rewind (f);
  {
    char buffer[502];

    result = getndelim2 (&line, &len, 0, GETNLINE_NO_LIMIT, '\n', '\r', f);
    ASSERT (result == 501);
    ASSERT (501 < len);
    memset (buffer, ' ', 499);
    buffer[499] = '0';
    buffer[500] = '\r';
    buffer[501] = '\0';
    ASSERT (strcmp (buffer, line) == 0);

    result = getndelim2 (&line, &len, 0, GETNLINE_NO_LIMIT, '\n', '\r', f);
    ASSERT (result == 501);
    ASSERT (501 < len);
    buffer[499] = '1';
    buffer[500] = '\n';
    ASSERT (strcmp (buffer, line) == 0);

    result = getndelim2 (&line, &len, 0, GETNLINE_NO_LIMIT, 'g', 'f', f);
    ASSERT (result == 501 * 14 - 1);
    ASSERT (501 * 14 <= len);
    buffer[499] = 'f';
    buffer[500] = '\0';
    ASSERT (strcmp (buffer, line + 501 * 13) == 0);

    result = getndelim2 (&line, &len, 501 * 14 - 1, GETNLINE_NO_LIMIT,
                         EOF, EOF, f);
    ASSERT (result == 1);
    buffer[500] = '\n';
    ASSERT (strcmp (buffer, line + 501 * 13) == 0);

    result = getndelim2 (&line, &len, 501 * 14 - 1, GETNLINE_NO_LIMIT,
                         EOF, EOF, f);
    buffer[500] = '\0';
    ASSERT (strcmp (buffer, line + 501 * 13) == 0);
    ASSERT (result == -1);
  }

  fclose (f);
  remove ("test-getndelim2.txt");
  return 0;
}
