 

#include <config.h>

 
#include <inttypes.h>

#include <stdlib.h>

#ifdef UNSIGNED
# ifndef HAVE_DECL_STRTOULL
"this configure-time declaration test was not run"
# endif
# if !HAVE_DECL_STRTOULL
unsigned long long int strtoull (char const *, char **, int);
# endif

#else

# ifndef HAVE_DECL_STRTOLL
"this configure-time declaration test was not run"
# endif
# if !HAVE_DECL_STRTOLL
long long int strtoll (char const *, char **, int);
# endif
#endif

#ifdef UNSIGNED
# define Int uintmax_t
# define Strtoimax strtoumax
# define Strtol strtoul
# define Strtoll strtoull
# define Unsigned unsigned
#else
# define Int intmax_t
# define Strtoimax strtoimax
# define Strtol strtol
# define Strtoll strtoll
# define Unsigned
#endif

Int
Strtoimax (char const *ptr, char **endptr, int base)
{
  static_assert (sizeof (Int) == sizeof (Unsigned long int)
                 || sizeof (Int) == sizeof (Unsigned long long int));

  if (sizeof (Int) != sizeof (Unsigned long int))
    return Strtoll (ptr, endptr, base);

  return Strtol (ptr, endptr, base);
}
