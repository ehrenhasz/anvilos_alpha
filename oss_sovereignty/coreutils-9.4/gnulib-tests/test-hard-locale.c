 

#include <config.h>

#include "hard-locale.h"

#include <locale.h>
#include <stdio.h>
#include <string.h>

 
static bool all_trivial;

static int
test_one (const char *name, int failure_bitmask)
{
  if (setlocale (LC_ALL, name) != NULL)
    {
      bool expected;

       
#if defined MUSL_LIBC || defined __OpenBSD__ || defined __HAIKU__ || defined __ANDROID__
      expected = true;
#else
      expected = !all_trivial;
#endif
      if (hard_locale (LC_CTYPE) != expected)
        {
          if (expected)
            fprintf (stderr, "Unexpected: The category LC_CTYPE of the locale '%s' is not equivalent to C or POSIX.\n",
                     name);
          else
            fprintf (stderr, "Unexpected: The category LC_CTYPE of the locale '%s' is equivalent to C or POSIX.\n",
                     name);
          return failure_bitmask;
        }

       
#if defined __NetBSD__
      expected = false;
#elif defined MUSL_LIBC
      expected = strcmp (name, "C.UTF-8") != 0;
#elif (defined __OpenBSD__ && HAVE_DUPLOCALE) || defined __HAIKU__ || defined __ANDROID__  
      expected = true;
#else
      expected = !all_trivial;
#endif
      if (hard_locale (LC_COLLATE) != expected)
        {
          if (expected)
            fprintf (stderr, "Unexpected: The category LC_COLLATE of the locale '%s' is not equivalent to C or POSIX.\n",
                     name);
          else
            fprintf (stderr, "Unexpected: The category LC_COLLATE of the locale '%s' is equivalent to C or POSIX.\n",
                     name);
          return failure_bitmask;
        }
    }
  return 0;
}

int
main ()
{
  int fail = 0;

   
#if ! defined __ANDROID__
  if (hard_locale (LC_CTYPE) || hard_locale (LC_COLLATE))
    {
      fprintf (stderr, "The initial locale should not be hard!\n");
      fail |= 1;
    }
#endif

  all_trivial = (setlocale (LC_ALL, "foobar") != NULL);

  fail |= test_one ("de", 2);
  fail |= test_one ("de_DE", 4);
  fail |= test_one ("de_DE.ISO8859-1", 8);
  fail |= test_one ("de_DE.iso88591", 8);
  fail |= test_one ("de_DE.UTF-8", 16);
  fail |= test_one ("de_DE.utf8", 16);
  fail |= test_one ("german", 32);
  fail |= test_one ("C.UTF-8", 64);

  return fail;
}
