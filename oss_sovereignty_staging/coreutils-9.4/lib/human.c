 

#include <config.h>

#include "human.h"

#include <locale.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <argmatch.h>
#include <error.h>
#include <intprops.h>

 
#define HUMAN_READABLE_SUFFIX_LENGTH_MAX 3

static const char power_letter[] =
{
  0,     
  'K',   
  'M',   
  'G',   
  'T',   
  'P',   
  'E',   
  'Z',   
  'Y',   
  'R',   
  'Q'    
};


 

static long double
adjust_value (int inexact_style, long double value)
{
   
  if (inexact_style != human_round_to_nearest && value < UINTMAX_MAX)
    {
      uintmax_t u = value;
      value = u + (inexact_style == human_ceiling && u != value);
    }

  return value;
}

 

static char *
group_number (char *number, size_t numberlen,
              char const *grouping, char const *thousands_sep)
{
  register char *d;
  size_t grouplen = SIZE_MAX;
  size_t thousands_seplen = strlen (thousands_sep);
  size_t i = numberlen;

   
  char buf[2 * INT_STRLEN_BOUND (uintmax_t) + 1];

  memcpy (buf, number, numberlen);
  d = number + numberlen;

  for (;;)
    {
      unsigned char g = *grouping;

      if (g)
        {
          grouplen = g < CHAR_MAX ? g : i;
          grouping++;
        }

      if (i < grouplen)
        grouplen = i;

      d -= grouplen;
      i -= grouplen;
      memcpy (d, buf + i, grouplen);
      if (i == 0)
        return d;

      d -= thousands_seplen;
      memcpy (d, thousands_sep, thousands_seplen);
    }
}

 

char *
human_readable (uintmax_t n, char *buf, int opts,
                uintmax_t from_block_size, uintmax_t to_block_size)
{
  int inexact_style =
    opts & (human_round_to_nearest | human_floor | human_ceiling);
  unsigned int base = opts & human_base_1024 ? 1024 : 1000;
  uintmax_t amt;
  int tenths;
  int exponent = -1;
  int exponent_max = sizeof power_letter - 1;
  char *p;
  char *psuffix;
  char const *integerlim;

   
  int rounding;

  char const *decimal_point = ".";
  size_t decimal_pointlen = 1;
  char const *grouping = "";
  char const *thousands_sep = "";
  struct lconv const *l = localeconv ();
  size_t pointlen = strlen (l->decimal_point);
  if (0 < pointlen && pointlen <= MB_LEN_MAX)
    {
      decimal_point = l->decimal_point;
      decimal_pointlen = pointlen;
    }
  grouping = l->grouping;
  if (strlen (l->thousands_sep) <= MB_LEN_MAX)
    thousands_sep = l->thousands_sep;

   
  psuffix = buf + LONGEST_HUMAN_READABLE - 1 - HUMAN_READABLE_SUFFIX_LENGTH_MAX;
  p = psuffix;

   
  if (to_block_size <= from_block_size)
    {
      if (from_block_size % to_block_size == 0)
        {
          uintmax_t multiplier = from_block_size / to_block_size;
          amt = n * multiplier;
          if (amt / multiplier == n)
            {
              tenths = 0;
              rounding = 0;
              goto use_integer_arithmetic;
            }
        }
    }
  else if (from_block_size != 0 && to_block_size % from_block_size == 0)
    {
      uintmax_t divisor = to_block_size / from_block_size;
      uintmax_t r10 = (n % divisor) * 10;
      uintmax_t r2 = (r10 % divisor) * 2;
      amt = n / divisor;
      tenths = r10 / divisor;
      rounding = r2 < divisor ? 0 < r2 : 2 + (divisor < r2);
      goto use_integer_arithmetic;
    }

  {
     

    long double dto_block_size = to_block_size;
    long double damt = n * (from_block_size / dto_block_size);
    size_t buflen;
    size_t nonintegerlen;

    if (! (opts & human_autoscale))
      {
        sprintf (buf, "%.0Lf", adjust_value (inexact_style, damt));
        buflen = strlen (buf);
        nonintegerlen = 0;
      }
    else
      {
        long double e = 1;
        exponent = 0;

        do
          {
            e *= base;
            exponent++;
          }
        while (e * base <= damt && exponent < exponent_max);

        damt /= e;

        sprintf (buf, "%.1Lf", adjust_value (inexact_style, damt));
        buflen = strlen (buf);
        nonintegerlen = decimal_pointlen + 1;

        if (1 + nonintegerlen + ! (opts & human_base_1024) < buflen
            || ((opts & human_suppress_point_zero)
                && buf[buflen - 1] == '0'))
          {
            sprintf (buf, "%.0Lf",
                     adjust_value (inexact_style, damt * 10) / 10);
            buflen = strlen (buf);
            nonintegerlen = 0;
          }
      }

    p = psuffix - buflen;
    memmove (p, buf, buflen);
    integerlim = p + buflen - nonintegerlen;
  }
  goto do_grouping;

 use_integer_arithmetic:
  {
     

    if (opts & human_autoscale)
      {
        exponent = 0;

        if (base <= amt)
          {
            do
              {
                unsigned int r10 = (amt % base) * 10 + tenths;
                unsigned int r2 = (r10 % base) * 2 + (rounding >> 1);
                amt /= base;
                tenths = r10 / base;
                rounding = (r2 < base
                            ? (r2 + rounding) != 0
                            : 2 + (base < r2 + rounding));
                exponent++;
              }
            while (base <= amt && exponent < exponent_max);

            if (amt < 10)
              {
                if (inexact_style == human_round_to_nearest
                    ? 2 < rounding + (tenths & 1)
                    : inexact_style == human_ceiling && 0 < rounding)
                  {
                    tenths++;
                    rounding = 0;

                    if (tenths == 10)
                      {
                        amt++;
                        tenths = 0;
                      }
                  }

                if (amt < 10
                    && (tenths || ! (opts & human_suppress_point_zero)))
                  {
                    *--p = '0' + tenths;
                    p -= decimal_pointlen;
                    memcpy (p, decimal_point, decimal_pointlen);
                    tenths = rounding = 0;
                  }
              }
          }
      }

    if (inexact_style == human_round_to_nearest
        ? 5 < tenths + (0 < rounding + (amt & 1))
        : inexact_style == human_ceiling && 0 < tenths + rounding)
      {
        amt++;

        if ((opts & human_autoscale)
            && amt == base && exponent < exponent_max)
          {
            exponent++;
            if (! (opts & human_suppress_point_zero))
              {
                *--p = '0';
                p -= decimal_pointlen;
                memcpy (p, decimal_point, decimal_pointlen);
              }
            amt = 1;
          }
      }

    integerlim = p;

    do
      {
        int digit = amt % 10;
        *--p = digit + '0';
      }
    while ((amt /= 10) != 0);
  }

 do_grouping:
  if (opts & human_group_digits)
    p = group_number (p, integerlim - p, grouping, thousands_sep);

  if (opts & human_SI)
    {
      if (exponent < 0)
        {
          uintmax_t power;
          exponent = 0;
          for (power = 1; power < to_block_size; power *= base)
            if (++exponent == exponent_max)
              break;
        }

      if ((exponent | (opts & human_B)) && (opts & human_space_before_unit))
        *psuffix++ = ' ';

      if (exponent)
        *psuffix++ = (! (opts & human_base_1024) && exponent == 1
                      ? 'k'
                      : power_letter[exponent]);

      if (opts & human_B)
        {
          if ((opts & human_base_1024) && exponent)
            *psuffix++ = 'i';
          *psuffix++ = 'B';
        }
    }

  *psuffix = '\0';

  return p;
}


 
#ifndef DEFAULT_BLOCK_SIZE
# define DEFAULT_BLOCK_SIZE 1024
#endif

static char const *const block_size_args[] = { "human-readable", "si", 0 };
static int const block_size_opts[] =
  {
    human_autoscale + human_SI + human_base_1024,
    human_autoscale + human_SI
  };

static uintmax_t
default_block_size (void)
{
  return getenv ("POSIXLY_CORRECT") ? 512 : DEFAULT_BLOCK_SIZE;
}

static strtol_error
humblock (char const *spec, uintmax_t *block_size, int *options)
{
  int i;
  int opts = 0;

  if (! spec
      && ! (spec = getenv ("BLOCK_SIZE"))
      && ! (spec = getenv ("BLOCKSIZE")))
    *block_size = default_block_size ();
  else
    {
      if (*spec == '\'')
        {
          opts |= human_group_digits;
          spec++;
        }

      if (0 <= (i = ARGMATCH (spec, block_size_args, block_size_opts)))
        {
          opts |= block_size_opts[i];
          *block_size = 1;
        }
      else
        {
          char *ptr;
          strtol_error e = xstrtoumax (spec, &ptr, 0, block_size,
                                       "eEgGkKmMpPtTyYzZ0");
          if (e != LONGINT_OK)
            {
              *options = 0;
              return e;
            }
          for (; ! ('0' <= *spec && *spec <= '9'); spec++)
            if (spec == ptr)
              {
                opts |= human_SI;
                if (ptr[-1] == 'B')
                  opts |= human_B;
                if (ptr[-1] != 'B' || ptr[-2] == 'i')
                  opts |= human_base_1024;
                break;
              }
        }
    }

  *options = opts;
  return LONGINT_OK;
}

enum strtol_error
human_options (char const *spec, int *opts, uintmax_t *block_size)
{
  strtol_error e = humblock (spec, block_size, opts);
  if (*block_size == 0)
    {
      *block_size = default_block_size ();
      e = LONGINT_INVALID;
    }
  return e;
}
