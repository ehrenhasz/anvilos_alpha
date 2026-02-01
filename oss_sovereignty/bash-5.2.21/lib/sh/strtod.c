 

 

#if HAVE_CONFIG_H
# include <config.h>
#endif

#ifndef HAVE_STRTOD

#include <errno.h>
#ifndef errno
extern int errno;
#endif

#include <chartypes.h>
#include <math.h>

#if HAVE_FLOAT_H
# include <float.h>
#else
# define DBL_MAX 1.7976931348623159e+308
# define DBL_MIN 2.2250738585072010e-308
#endif

#include <bashansi.h>

#ifndef NULL
#  define NULL 0
#endif

#ifndef HUGE_VAL
#  define HUGE_VAL HUGE
#endif

#ifndef locale_decpoint
extern int locale_decpoint PARAMS((void));
#endif

 
double
strtod (nptr, endptr)
     const char *nptr;
     char **endptr;
{
  register const char *s;
  short sign;

   
  double num;

  int radixchar;
  int got_dot;			 
  int got_digit;		 

   
  long int exponent;

  if (nptr == NULL)
    {
      errno = EINVAL;
      goto noconv;
    }

  s = nptr;

   
  while (ISSPACE ((unsigned char)*s))
    ++s;

   
  sign = *s == '-' ? -1 : 1;
  if (*s == '-' || *s == '+')
    ++s;

  radixchar = locale_decpoint ();
  num = 0.0;
  got_dot = 0;
  got_digit = 0;
  exponent = 0;
  for (;; ++s)
    {
      if (DIGIT (*s))
	{
	  got_digit = 1;

	   
	  if (num > DBL_MAX * 0.1)
	     
	    ++exponent;
	  else
	    num = (num * 10.0) + (*s - '0');

	   
	  if (got_dot)
	    --exponent;
	}
      else if (!got_dot && *s == radixchar)
	 
	got_dot = 1;
      else
	 
	break;
    }

  if (!got_digit)
    goto noconv;

  if (TOLOWER ((unsigned char)*s) == 'e')
    {
       
      int save = errno;
      char *end;
      long int exp;

      errno = 0;
      ++s;
      exp = strtol (s, &end, 10);
      if (errno == ERANGE)
	{
	   
	  if (endptr != NULL)
	    *endptr = end;
	  if (exp < 0)
	    goto underflow;
	  else
	    goto overflow;
	}
      else if (end == s)
	 
	end = (char *) s - 1;
      errno = save;
      s = end;
      exponent += exp;
    }

  if (endptr != NULL)
    *endptr = (char *) s;

  if (num == 0.0)
    return 0.0;

   

  if (exponent < 0)
    {
      if (num < DBL_MIN * pow (10.0, (double) -exponent))
	goto underflow;
    }
  else if (exponent > 0)
    {
      if (num > DBL_MAX * pow (10.0, (double) -exponent))
	goto overflow;
    }

  num *= pow (10.0, (double) exponent);

  return num * sign;

overflow:
   
  errno = ERANGE;
  return HUGE_VAL * sign;

underflow:
   
  if (endptr != NULL)
    *endptr = (char *) nptr;
  errno = ERANGE;
  return 0.0;

noconv:
   
  if (endptr != NULL)
    *endptr = (char *) nptr;
  return 0.0;
}

#endif  
