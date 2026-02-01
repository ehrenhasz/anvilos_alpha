 

#include <config.h>

#include "c-strtod.h"

#include <errno.h>
#include <locale.h>
#include <stdlib.h>
#include <string.h>

#if LONG
# define C_STRTOD c_strtold
# define DOUBLE long double
# define STRTOD_L strtold_l
# define HAVE_GOOD_STRTOD_L (HAVE_STRTOLD_L && !GNULIB_defined_strtold_function)
# define STRTOD strtold
#else
# define C_STRTOD c_strtod
# define DOUBLE double
# define STRTOD_L strtod_l
# define HAVE_GOOD_STRTOD_L (HAVE_STRTOD_L && !GNULIB_defined_strtod_function)
# define STRTOD strtod
#endif

#if defined LC_ALL_MASK \
    && ((LONG ? HAVE_GOOD_STRTOLD_L : HAVE_GOOD_STRTOD_L) \
        || HAVE_WORKING_USELOCALE)

 
static volatile locale_t c_locale_cache;

 
static locale_t
c_locale (void)
{
  if (!c_locale_cache)
    c_locale_cache = newlocale (LC_ALL_MASK, "C", (locale_t) 0);
  return c_locale_cache;
}

#endif

DOUBLE
C_STRTOD (char const *nptr, char **endptr)
{
  DOUBLE r;

#if defined LC_ALL_MASK \
    && ((LONG ? HAVE_GOOD_STRTOLD_L : HAVE_GOOD_STRTOD_L) \
        || HAVE_WORKING_USELOCALE)

  locale_t locale = c_locale ();
  if (!locale)
    {
      if (endptr)
        *endptr = (char *) nptr;
      return 0;  
    }

# if (LONG ? HAVE_GOOD_STRTOLD_L : HAVE_GOOD_STRTOD_L)

  r = STRTOD_L (nptr, endptr, locale);

# else  

  locale_t old_locale = uselocale (locale);
  if (old_locale == (locale_t)0)
    {
      if (endptr)
        *endptr = (char *) nptr;
      return 0;  
    }

  r = STRTOD (nptr, endptr);

  int saved_errno = errno;
  if (uselocale (old_locale) == (locale_t)0)
     
    abort ();
  errno = saved_errno;

# endif

#else

  char *saved_locale = setlocale (LC_NUMERIC, NULL);

  if (saved_locale)
    {
      saved_locale = strdup (saved_locale);
      if (saved_locale == NULL)
        {
          if (endptr)
            *endptr = (char *) nptr;
          return 0;  
        }
      setlocale (LC_NUMERIC, "C");
    }

  r = STRTOD (nptr, endptr);

  if (saved_locale)
    {
      int saved_errno = errno;

      setlocale (LC_NUMERIC, saved_locale);
      free (saved_locale);
      errno = saved_errno;
    }

#endif

  return r;
}
