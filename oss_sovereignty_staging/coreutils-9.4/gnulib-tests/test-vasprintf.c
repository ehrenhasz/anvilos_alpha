 

#include <config.h>

#include <stdio.h>

#include "signature.h"
SIGNATURE_CHECK (asprintf, int, (char **, char const *, ...));
SIGNATURE_CHECK (vasprintf, int, (char **, char const *, va_list));

#include <stdarg.h>
#include <stdlib.h>
#include <string.h>

#include "macros.h"

static int
my_asprintf (char **result, const char *format, ...)
{
  va_list args;
  int ret;

  va_start (args, format);
  ret = vasprintf (result, format, args);
  va_end (args);
  return ret;
}

static void
test_vasprintf ()
{
  int repeat;

  for (repeat = 0; repeat <= 8; repeat++)
    {
      char *result;
      int retval = my_asprintf (&result, "%d", 12345);
      ASSERT (retval == 5);
      ASSERT (result != NULL);
      ASSERT (strcmp (result, "12345") == 0);
      free (result);
    }

  for (repeat = 0; repeat <= 8; repeat++)
    {
      char *result;
      int retval = my_asprintf (&result, "%08lx", 12345UL);
      ASSERT (retval == 8);
      ASSERT (result != NULL);
      ASSERT (strcmp (result, "00003039") == 0);
      free (result);
    }
}

static void
test_asprintf ()
{
  int repeat;

  for (repeat = 0; repeat <= 8; repeat++)
    {
      char *result;
      int retval = asprintf (&result, "%d", 12345);
      ASSERT (retval == 5);
      ASSERT (result != NULL);
      ASSERT (strcmp (result, "12345") == 0);
      free (result);
    }

  for (repeat = 0; repeat <= 8; repeat++)
    {
      char *result;
      int retval = asprintf (&result, "%08lx", 12345UL);
      ASSERT (retval == 8);
      ASSERT (result != NULL);
      ASSERT (strcmp (result, "00003039") == 0);
      free (result);
    }
}

int
main (int argc, char *argv[])
{
  test_vasprintf ();
  test_asprintf ();
  return 0;
}
