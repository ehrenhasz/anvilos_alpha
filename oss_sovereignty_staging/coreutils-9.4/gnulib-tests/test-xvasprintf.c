 

 
#if (__GNUC__ == 4 && 3 <= __GNUC_MINOR__) || 4 < __GNUC__
# pragma GCC diagnostic ignored "-Wformat-zero-length"
# pragma GCC diagnostic ignored "-Wformat-nonliteral"
# pragma GCC diagnostic ignored "-Wformat-security"
#endif

#include <config.h>

#include "xvasprintf.h"

#include <stdarg.h>
#include <stdlib.h>
#include <string.h>

#include "macros.h"

static char *
my_xasprintf (const char *format, ...)
{
  va_list args;
  char *ret;

  va_start (args, format);
  ret = xvasprintf (format, args);
  va_end (args);
  return ret;
}

static void
test_xvasprintf (void)
{
  int repeat;
  char *result;

  for (repeat = 0; repeat <= 8; repeat++)
    {
      result = my_xasprintf ("%d", 12345);
      ASSERT (result != NULL);
      ASSERT (strcmp (result, "12345") == 0);
      free (result);
    }

  {
     
    const char *empty = "";
    result = my_xasprintf (empty);
    ASSERT (result != NULL);
    ASSERT (strcmp (result, "") == 0);
    free (result);
  }

  result = my_xasprintf ("%s", "foo");
  ASSERT (result != NULL);
  ASSERT (strcmp (result, "foo") == 0);
  free (result);

  result = my_xasprintf ("%s%s", "foo", "bar");
  ASSERT (result != NULL);
  ASSERT (strcmp (result, "foobar") == 0);
  free (result);

  result = my_xasprintf ("%s%sbaz", "foo", "bar");
  ASSERT (result != NULL);
  ASSERT (strcmp (result, "foobarbaz") == 0);
  free (result);
}

static void
test_xasprintf (void)
{
  int repeat;
  char *result;

  for (repeat = 0; repeat <= 8; repeat++)
    {
      result = xasprintf ("%d", 12345);
      ASSERT (result != NULL);
      ASSERT (strcmp (result, "12345") == 0);
      free (result);
    }

  {
     
    const char *empty = "";
    result = xasprintf (empty, empty);
    ASSERT (result != NULL);
    ASSERT (strcmp (result, "") == 0);
    free (result);
  }

  result = xasprintf ("%s", "foo");
  ASSERT (result != NULL);
  ASSERT (strcmp (result, "foo") == 0);
  free (result);

  result = xasprintf ("%s%s", "foo", "bar");
  ASSERT (result != NULL);
  ASSERT (strcmp (result, "foobar") == 0);
  free (result);

  result = my_xasprintf ("%s%sbaz", "foo", "bar");
  ASSERT (result != NULL);
  ASSERT (strcmp (result, "foobarbaz") == 0);
  free (result);
}

int
main (_GL_UNUSED int argc, char *argv[])
{
  test_xvasprintf ();
  test_xasprintf ();

  return 0;
}
