 

 

#include <config.h>

#if !HAVE_STRTOL

#include <chartypes.h>
#include <errno.h>

#ifndef errno
extern int errno;
#endif

#ifndef __set_errno
#  define __set_errno(Val) errno = (Val)
#endif

#ifdef HAVE_LIMITS_H
#  include <limits.h>
#endif

#include <typemax.h>

#include <stdc.h>
#include <bashansi.h>

#ifndef NULL
#  define NULL 0
#endif

 
#ifndef UNSIGNED
#  define UNSIGNED 0
#  define INT LONG int
#else
#  define INT unsigned LONG int
#endif

#if UNSIGNED
#  ifdef QUAD
#    define strtol strtoull
#  else
#    define strtol strtoul
#  endif
#else
#  ifdef QUAD
#    define strtol strtoll
#  endif
#endif

 

#ifdef QUAD
#  define LONG long long
#  define STRTOL_LONG_MIN LLONG_MIN
#  define STRTOL_LONG_MAX LLONG_MAX
#  define STRTOL_ULONG_MAX ULLONG_MAX
#else	 
#  define LONG long
#  define STRTOL_LONG_MIN LONG_MIN
#  define STRTOL_LONG_MAX LONG_MAX
#  define STRTOL_ULONG_MAX ULONG_MAX
#endif

 

INT
strtol (nptr, endptr, base)
     const char *nptr;
     char **endptr;
     int base;
{
  int negative;
  register unsigned LONG int cutoff;
  register unsigned int cutlim;
  register unsigned LONG int i;
  register const char *s;
  register unsigned char c;
  const char *save, *end;
  int overflow;

  if (base < 0 || base == 1 || base > 36)
    {
      __set_errno (EINVAL);
      return 0;
    }

  save = s = nptr;

   
  while (ISSPACE ((unsigned char)*s))
    ++s;
  if (*s == '\0')
    goto noconv;

   
  if (*s == '-' || *s == '+')
    {
      negative = (*s == '-');
      ++s;
    }
  else
    negative = 0;

   
  if (*s == '0')
    {
      if ((base == 0 || base == 16) && TOUPPER ((unsigned char) s[1]) == 'X')
	{
	  s += 2;
	  base = 16;
	}
      else if (base == 0)
	base = 8;
    }
  else if (base == 0)
    base = 10;

   
  save = s;

  end = NULL;

  cutoff = STRTOL_ULONG_MAX / (unsigned LONG int) base;
  cutlim = STRTOL_ULONG_MAX % (unsigned LONG int) base;

  overflow = 0;
  i = 0;
  c = *s;
  if (sizeof (long int) != sizeof (LONG int))
    {
      unsigned long int j = 0;
      unsigned long int jmax = ULONG_MAX / base;

      for (;c != '\0'; c = *++s)
	{
	  if (s == end)
	    break;
	  if (DIGIT (c))
	    c -= '0';
	  else if (ISALPHA (c))
	    c = TOUPPER (c) - 'A' + 10;
	  else
	    break;

	  if ((int) c >= base)
	    break;
	   
	  else if (j >= jmax)
	    {
	       
	      i = (unsigned LONG int) j;
	      goto use_long;
	    }
	  else
	    j = j * (unsigned long int) base + c;
	}

      i = (unsigned LONG int) j;
    }
  else
    for (;c != '\0'; c = *++s)
      {
	if (s == end)
	  break;
	if (DIGIT (c))
	  c -= '0';
	else if (ISALPHA (c))
	  c = TOUPPER (c) - 'A' + 10;
	else
	  break;
	if ((int) c >= base)
	  break;
	 
	if (i > cutoff || (i == cutoff && c > cutlim))
	  overflow = 1;
	else
	  {
	  use_long:
	    i *= (unsigned LONG int) base;
	    i += c;
	  }
      }

   
  if (s == save)
    goto noconv;

   
  if (endptr != NULL)
    *endptr = (char *) s;

#if !UNSIGNED
   
  if (overflow == 0
      && i > (negative
	      ? -((unsigned LONG int) (STRTOL_LONG_MIN + 1)) + 1
	      : (unsigned LONG int) STRTOL_LONG_MAX))
    overflow = 1;
#endif

  if (overflow)
    {
      __set_errno (ERANGE);
#if UNSIGNED
      return STRTOL_ULONG_MAX;
#else
      return negative ? STRTOL_LONG_MIN : STRTOL_LONG_MAX;
#endif
    }

   
  return negative ? -i : i;

noconv:
   
  if (endptr != NULL)
    {
      if (save - nptr >= 2 && TOUPPER ((unsigned char) save[-1]) == 'X' && save[-2] == '0')
	*endptr = (char *) &save[-1];
      else
	 
	*endptr = (char *) nptr;
    }

  return 0L;
}

#endif  
