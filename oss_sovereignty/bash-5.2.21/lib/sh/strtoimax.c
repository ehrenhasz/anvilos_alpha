 

 

 

#if HAVE_CONFIG_H
#  include <config.h>
#endif

#if HAVE_INTTYPES_H
#  include <inttypes.h>
#endif

#if HAVE_STDINT_H
#  include <stdint.h>
#endif

#if HAVE_STDLIB_H
#  include <stdlib.h>
#endif

#include <stdc.h>

 
#define verify(name, assertion) struct name { char a[(assertion) ? 1 : -1]; }

#ifndef HAVE_DECL_STRTOL
"this configure-time declaration test was not run"
#endif
#if !HAVE_DECL_STRTOL
extern long strtol PARAMS((const char *, char **, int));
#endif

#ifndef HAVE_DECL_STRTOLL
"this configure-time declaration test was not run"
#endif
#if !HAVE_DECL_STRTOLL && HAVE_LONG_LONG_INT
extern long long strtoll PARAMS((const char *, char **, int));
#endif

#ifdef strtoimax
#undef strtoimax
#endif

intmax_t
strtoimax (ptr, endptr, base)
     const char *ptr;
     char **endptr;
     int base;
{
#if HAVE_LONG_LONG_INT
  verify(size_is_that_of_long_or_long_long,
	 (sizeof (intmax_t) == sizeof (long) ||
	  sizeof (intmax_t) == sizeof (long long)));

  if (sizeof (intmax_t) != sizeof (long))
    return (strtoll (ptr, endptr, base));
#else
  verify (size_is_that_of_long, sizeof (intmax_t) == sizeof (long));
#endif

  return (strtol (ptr, endptr, base));
}

#ifdef TESTING
# include <stdio.h>
int
main ()
{
  char *p, *endptr;
  intmax_t x;
#if HAVE_LONG_LONG_INT
  long long y;
#endif
  long z;
  
  printf ("sizeof intmax_t: %d\n", sizeof (intmax_t));

#if HAVE_LONG_LONG_INT
  printf ("sizeof long long: %d\n", sizeof (long long));
#endif
  printf ("sizeof long: %d\n", sizeof (long));

  x = strtoimax("42", &endptr, 10);
#if HAVE_LONG_LONG_INT
  y = strtoll("42", &endptr, 10);
#else
  y = -1;
#endif
  z = strtol("42", &endptr, 10);

  printf ("%lld %lld %ld\n", x, y, z);

  exit (0);
}
#endif
