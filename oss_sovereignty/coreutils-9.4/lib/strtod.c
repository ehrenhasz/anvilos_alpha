 
#include <stdlib.h>

#include <ctype.h>       
#include <errno.h>
#include <float.h>       
#include <limits.h>      
#include <locale.h>      
#include <math.h>        
#include <stdio.h>       
#include <string.h>      
#if HAVE_NL_LANGINFO
# include <langinfo.h>
#endif

#include "c-ctype.h"

#undef MIN
#undef MAX
#ifdef USE_LONG_DOUBLE
# define STRTOD strtold
# define LDEXP ldexpl
# if defined __hpux && defined __hppa
    
#  define HAVE_UNDERLYING_STRTOD 0
# elif STRTOLD_HAS_UNDERFLOW_BUG
    
#  define HAVE_UNDERLYING_STRTOD 0
# else
#  define HAVE_UNDERLYING_STRTOD HAVE_STRTOLD
# endif
# define DOUBLE long double
# define MIN LDBL_MIN
# define MAX LDBL_MAX
# define L_(literal) literal##L
#else
# define STRTOD strtod
# define LDEXP ldexp
# define HAVE_UNDERLYING_STRTOD 1
# define DOUBLE double
# define MIN DBL_MIN
# define MAX DBL_MAX
# define L_(literal) literal
#endif

#if (defined USE_LONG_DOUBLE ? HAVE_LDEXPM_IN_LIBC : HAVE_LDEXP_IN_LIBC)
# define USE_LDEXP 1
#else
# define USE_LDEXP 0
#endif

 
static bool
locale_isspace (char c)
{
  unsigned char uc = c;
  return isspace (uc) != 0;
}

 
static char
decimal_point_char (void)
{
  const char *point;
   
#if HAVE_NL_LANGINFO && (__GLIBC__ || defined __UCLIBC__ || (defined __APPLE__ && defined __MACH__))
  point = nl_langinfo (RADIXCHAR);
#elif 1
  char pointbuf[5];
  sprintf (pointbuf, "%#.0f", 1.0);
  point = &pointbuf[1];
#else
  point = localeconv () -> decimal_point;
#endif
   
  return (point[0] != '\0' ? point[0] : '.');
}

#if !USE_LDEXP
 #undef LDEXP
 #define LDEXP dummy_ldexp
  
 static DOUBLE LDEXP (_GL_UNUSED DOUBLE x, _GL_UNUSED int exponent)
 {
   abort ();
   return L_(0.0);
 }
#endif

 
static DOUBLE
scale_radix_exp (DOUBLE x, int radix, long int exponent)
{
   

  long int e = exponent;

  if (USE_LDEXP && radix == 2)
    return LDEXP (x, e < INT_MIN ? INT_MIN : INT_MAX < e ? INT_MAX : e);
  else
    {
      DOUBLE r = x;

      if (r != 0)
        {
          if (e < 0)
            {
              while (e++ != 0)
                {
                  r /= radix;
                  if (r == 0 && x != 0)
                    {
                      errno = ERANGE;
                      break;
                    }
                }
            }
          else
            {
              while (e-- != 0)
                {
                  if (r < -MAX / radix)
                    {
                      errno = ERANGE;
                      return -HUGE_VAL;
                    }
                  else if (MAX / radix < r)
                    {
                      errno = ERANGE;
                      return HUGE_VAL;
                    }
                  else
                    r *= radix;
                }
            }
        }

      return r;
    }
}

 
static DOUBLE
parse_number (const char *nptr,
              int base, int radix, int radix_multiplier, char radixchar,
              char expchar,
              char **endptr)
{
  const char *s = nptr;
  const char *digits_start;
  const char *digits_end;
  const char *radixchar_ptr;
  long int exponent;
  DOUBLE num;

   
  digits_start = s;
  radixchar_ptr = NULL;
  for (;; ++s)
    {
      if (base == 16 ? c_isxdigit (*s) : c_isdigit (*s))
        ;
      else if (radixchar_ptr == NULL && *s == radixchar)
        {
           
          radixchar_ptr = s;
        }
      else
         
        break;
    }
  digits_end = s;
   

  if (false)
    {  
      exponent =
        (radixchar_ptr != NULL
         ? - (long int) (digits_end - radixchar_ptr - 1)
         : 0);
    }
  else
    {  
      while (digits_end > digits_start)
        {
          if (digits_end - 1 == radixchar_ptr || *(digits_end - 1) == '0')
            digits_end--;
          else
            break;
        }
      exponent =
        (radixchar_ptr != NULL
         ? (digits_end > radixchar_ptr
            ? - (long int) (digits_end - radixchar_ptr - 1)
            : (long int) (radixchar_ptr - digits_end))
         : (long int) (s - digits_end));
    }

   
  {
    const char *dp;
    num = 0;
    for (dp = digits_start; dp < digits_end; dp++)
      if (dp != radixchar_ptr)
        {
          int digit;

           
          if (!(num <= MAX / base))
            {
               
              exponent +=
                (digits_end - dp)
                - (radixchar_ptr >= dp && radixchar_ptr < digits_end ? 1 : 0);
              break;
            }

           
          if (c_isdigit (*dp))
            digit = *dp - '0';
          else if (base == 16 && c_isxdigit (*dp))
            digit = c_tolower (*dp) - ('a' - 10);
          else
            abort ();
          num = num * base + digit;
        }
  }

  exponent = exponent * radix_multiplier;

   
  if (c_tolower (*s) == expchar && ! locale_isspace (s[1]))
    {
       
      int saved_errno = errno;
      char *end;
      long int value = strtol (s + 1, &end, 10);
      errno = saved_errno;

      if (s + 1 != end)
        {
           
          s = end;
          exponent =
            (exponent < 0
             ? (value < LONG_MIN - exponent ? LONG_MIN : exponent + value)
             : (LONG_MAX - exponent < value ? LONG_MAX : exponent + value));
        }
    }

  *endptr = (char *) s;
  return scale_radix_exp (num, radix, exponent);
}

 
static DOUBLE
minus_zero (void)
{
#if defined __hpux || defined __sgi || defined __ICC
  return -MIN * MIN;
#else
  return -0.0;
#endif
}

 
DOUBLE
STRTOD (const char *nptr, char **endptr)
#if HAVE_UNDERLYING_STRTOD
# ifdef USE_LONG_DOUBLE
#  undef strtold
# else
#  undef strtod
# endif
#else
# undef STRTOD
# define STRTOD(NPTR,ENDPTR) \
   parse_number (NPTR, 10, 10, 1, radixchar, 'e', ENDPTR)
#endif
 
{
  char radixchar;
  bool negative = false;

   
  DOUBLE num;

  const char *s = nptr;
  const char *end;
  char *endbuf;
  int saved_errno = errno;

  radixchar = decimal_point_char ();

   
  while (locale_isspace (*s))
    ++s;

   
  negative = *s == '-';
  if (*s == '-' || *s == '+')
    ++s;

  num = STRTOD (s, &endbuf);
  end = endbuf;

  if (c_isdigit (s[*s == radixchar]))
    {
       
      if (*s == '0' && c_tolower (s[1]) == 'x')
        {
          if (! c_isxdigit (s[2 + (s[2] == radixchar)]))
            {
              end = s + 1;

               
              errno = saved_errno;
            }
          else if (end <= s + 2)
            {
              num = parse_number (s + 2, 16, 2, 4, radixchar, 'p', &endbuf);
              end = endbuf;
            }
          else
            {
              const char *p = s + 2;
              while (p < end && c_tolower (*p) != 'p')
                p++;
              if (p < end && ! c_isdigit (p[1 + (p[1] == '-' || p[1] == '+')]))
                {
                  char *dup = strdup (s);
                  errno = saved_errno;
                  if (!dup)
                    {
                       
                      num =
                        parse_number (s + 2, 16, 2, 4, radixchar, 'p', &endbuf);
                    }
                  else
                    {
                      dup[p - s] = '\0';
                      num = STRTOD (dup, &endbuf);
                      saved_errno = errno;
                      free (dup);
                      errno = saved_errno;
                    }
                  end = p;
                }
            }
        }
      else
        {
           
          const char *e = s + 1;
          while (e < end && c_tolower (*e) != 'e')
            e++;
          if (e < end && ! c_isdigit (e[1 + (e[1] == '-' || e[1] == '+')]))
            {
              char *dup = strdup (s);
              errno = saved_errno;
              if (!dup)
                {
                   
                  num = parse_number (s, 10, 10, 1, radixchar, 'e', &endbuf);
                }
              else
                {
                  dup[e - s] = '\0';
                  num = STRTOD (dup, &endbuf);
                  saved_errno = errno;
                  free (dup);
                  errno = saved_errno;
                }
              end = e;
            }
        }

      s = end;
    }

   
  else if (c_tolower (*s) == 'i'
           && c_tolower (s[1]) == 'n'
           && c_tolower (s[2]) == 'f')
    {
      s += 3;
      if (c_tolower (*s) == 'i'
          && c_tolower (s[1]) == 'n'
          && c_tolower (s[2]) == 'i'
          && c_tolower (s[3]) == 't'
          && c_tolower (s[4]) == 'y')
        s += 5;
      num = HUGE_VAL;
      errno = saved_errno;
    }
  else if (c_tolower (*s) == 'n'
           && c_tolower (s[1]) == 'a'
           && c_tolower (s[2]) == 'n')
    {
      s += 3;
      if (*s == '(')
        {
          const char *p = s + 1;
          while (c_isalnum (*p))
            p++;
          if (*p == ')')
            s = p + 1;
        }

       
      if (s != end || num == num)
        num = NAN;
      errno = saved_errno;
    }
  else
    {
       
      errno = EINVAL;
      s = nptr;
    }

  if (endptr != NULL)
    *endptr = (char *) s;
   
  if (!num && negative)
    return minus_zero ();
  return negative ? -num : num;
}
