 

 

 

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

#ifndef HAVE_DECL_STRTOUL
"this configure-time declaration test was not run"
#endif
#if !HAVE_DECL_STRTOUL
extern unsigned long strtoul PARAMS((const char *, char **, int));
#endif

#ifndef HAVE_DECL_STRTOULL
"this configure-time declaration test was not run"
#endif
#if !HAVE_DECL_STRTOULL && HAVE_UNSIGNED_LONG_LONG_INT
extern unsigned long long strtoull PARAMS((const char *, char **, int));
#endif

#ifdef strtoumax
#undef strtoumax
#endif

uintmax_t
strtoumax (ptr, endptr, base)
     const char *ptr;
     char **endptr;
     int base;
{
#if HAVE_UNSIGNED_LONG_LONG_INT
  verify (size_is_that_of_unsigned_long_or_unsigned_long_long,
	  (sizeof (uintmax_t) == sizeof (unsigned long) ||
	   sizeof (uintmax_t) == sizeof (unsigned long long)));

  if (sizeof (uintmax_t) != sizeof (unsigned long))
    return (strtoull (ptr, endptr, base));
#else
  verify (size_is_that_of_unsigned_long, sizeof (uintmax_t) == sizeof (unsigned long));
#endif

  return (strtoul (ptr, endptr, base));
}

#ifdef TESTING
# include <stdio.h>
int
main ()
{
  char *p, *endptr;
  uintmax_t x;
#if HAVE_UNSIGNED_LONG_LONG_INT
  unsigned long long y;
#endif
  unsigned long z;

  printf ("sizeof uintmax_t: %d\n", sizeof (uintmax_t));

#if HAVE_UNSIGNED_LONG_LONG_INT
  printf ("sizeof unsigned long long: %d\n", sizeof (unsigned long long));
#endif
  printf ("sizeof unsigned long: %d\n", sizeof (unsigned long));

  x = strtoumax("42", &endptr, 10);
#if HAVE_LONG_LONG_INT
  y = strtoull("42", &endptr, 10);
#else
  y = 0;
#endif
  z = strtoul("42", &endptr, 10);

  printf ("%llu %llu %lu\n", x, y, z);

  exit (0);
}
#endif
