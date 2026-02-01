 

#ifndef __strtol
# define __strtol strtol
# define __strtol_t long int
# define __xstrtol xstrtol
# define STRTOL_T_MINIMUM LONG_MIN
# define STRTOL_T_MAXIMUM LONG_MAX
#endif

#include <config.h>

#include "xstrtol.h"

 
#include <stdio.h>

#include <ctype.h>
#include <errno.h>
#include <limits.h>
#include <stdckdint.h>
#include <stdlib.h>
#include <string.h>

#if XSTRTOL_INCLUDE_INTTYPES_H
# include <inttypes.h>
#endif

#include "assure.h"
#include "intprops.h"

static strtol_error
bkm_scale (__strtol_t *x, int scale_factor)
{
  __strtol_t scaled;
  if (ckd_mul (&scaled, *x, scale_factor))
    {
      *x = *x < 0 ? TYPE_MINIMUM (__strtol_t) : TYPE_MAXIMUM (__strtol_t);
      return LONGINT_OVERFLOW;
    }

  *x = scaled;

  return LONGINT_OK;
}

static strtol_error
bkm_scale_by_power (__strtol_t *x, int base, int power)
{
  strtol_error err = LONGINT_OK;
  while (power--)
    err |= bkm_scale (x, base);
  return err;
}

 

strtol_error
__xstrtol (const char *s, char **ptr, int strtol_base,
           __strtol_t *val, const char *valid_suffixes)
{
  char *t_ptr;
  char **p;
  __strtol_t tmp;
  strtol_error err = LONGINT_OK;

  assure (0 <= strtol_base && strtol_base <= 36);

  p = (ptr ? ptr : &t_ptr);

  errno = 0;

  if (! TYPE_SIGNED (__strtol_t))
    {
      const char *q = s;
      unsigned char ch = *q;
      while (isspace (ch))
        ch = *++q;
      if (ch == '-')
        return LONGINT_INVALID;
    }

  tmp = __strtol (s, p, strtol_base);

  if (*p == s)
    {
       
      if (valid_suffixes && **p && strchr (valid_suffixes, **p))
        tmp = 1;
      else
        return LONGINT_INVALID;
    }
  else if (errno != 0)
    {
      if (errno != ERANGE)
        return LONGINT_INVALID;
      err = LONGINT_OVERFLOW;
    }

   
   
  if (!valid_suffixes)
    {
      *val = tmp;
      return err;
    }

  if (**p != '\0')
    {
      int base = 1024;
      int suffixes = 1;
      strtol_error overflow;

      if (!strchr (valid_suffixes, **p))
        {
          *val = tmp;
          return err | LONGINT_INVALID_SUFFIX_CHAR;
        }

      switch (**p)
        {
        case 'E': case 'G': case 'g': case 'k': case 'K': case 'M': case 'm':
        case 'P': case 'Q': case 'R': case 'T': case 't': case 'Y': case 'Z':

           

          if (strchr (valid_suffixes, '0'))
            switch (p[0][1])
              {
              case 'i':
                if (p[0][2] == 'B')
                  suffixes += 2;
                break;

              case 'B':
              case 'D':  
                base = 1000;
                suffixes++;
                break;
              }
        }

      switch (**p)
        {
        case 'b':
          overflow = bkm_scale (&tmp, 512);
          break;

        case 'B':
           
          overflow = bkm_scale (&tmp, 1024);
          break;

        case 'c':
          overflow = LONGINT_OK;
          break;

        case 'E':  
          overflow = bkm_scale_by_power (&tmp, base, 6);
          break;

        case 'G':  
        case 'g':  
          overflow = bkm_scale_by_power (&tmp, base, 3);
          break;

        case 'k':  
        case 'K':  
          overflow = bkm_scale_by_power (&tmp, base, 1);
          break;

        case 'M':  
        case 'm':  
          overflow = bkm_scale_by_power (&tmp, base, 2);
          break;

        case 'P':  
          overflow = bkm_scale_by_power (&tmp, base, 5);
          break;

        case 'Q':  
          overflow = bkm_scale_by_power (&tmp, base, 10);
          break;

        case 'R':  
          overflow = bkm_scale_by_power (&tmp, base, 9);
          break;

        case 'T':  
        case 't':  
          overflow = bkm_scale_by_power (&tmp, base, 4);
          break;

        case 'w':
          overflow = bkm_scale (&tmp, 2);
          break;

        case 'Y':  
          overflow = bkm_scale_by_power (&tmp, base, 8);
          break;

        case 'Z':  
          overflow = bkm_scale_by_power (&tmp, base, 7);
          break;

        default:
          *val = tmp;
          return err | LONGINT_INVALID_SUFFIX_CHAR;
        }

      err |= overflow;
      *p += suffixes;
      if (**p)
        err |= LONGINT_INVALID_SUFFIX_CHAR;
    }

  *val = tmp;
  return err;
}
