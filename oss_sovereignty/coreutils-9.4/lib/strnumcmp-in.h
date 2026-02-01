 

#ifndef STRNUMCMP_IN_H
# define STRNUMCMP_IN_H 1

# include "strnumcmp.h"

# include <stddef.h>

# define NEGATION_SIGN   '-'
# define NUMERIC_ZERO    '0'

 
# define ISDIGIT(c) ((unsigned int) (c) - '0' <= 9)


 

 

static inline int _GL_ATTRIBUTE_PURE
fraccompare (char const *a, char const *b, char decimal_point)
{
  if (*a == decimal_point && *b == decimal_point)
    {
      while (*++a == *++b)
        if (! ISDIGIT (*a))
          return 0;
      if (ISDIGIT (*a) && ISDIGIT (*b))
        return *a - *b;
      if (ISDIGIT (*a))
        goto a_trailing_nonzero;
      if (ISDIGIT (*b))
        goto b_trailing_nonzero;
      return 0;
    }
  else if (*a++ == decimal_point)
    {
    a_trailing_nonzero:
      while (*a == NUMERIC_ZERO)
        a++;
      return ISDIGIT (*a);
    }
  else if (*b++ == decimal_point)
    {
    b_trailing_nonzero:
      while (*b == NUMERIC_ZERO)
        b++;
      return - ISDIGIT (*b);
    }
  return 0;
}

 

static inline int _GL_ATTRIBUTE_PURE
numcompare (char const *a, char const *b,
            int decimal_point, int thousands_sep)
{
  unsigned char tmpa = *a;
  unsigned char tmpb = *b;
  int tmp;
  size_t log_a;
  size_t log_b;

  if (tmpa == NEGATION_SIGN)
    {
      do
        tmpa = *++a;
      while (tmpa == NUMERIC_ZERO || tmpa == thousands_sep);
      if (tmpb != NEGATION_SIGN)
        {
          if (tmpa == decimal_point)
            do
              tmpa = *++a;
            while (tmpa == NUMERIC_ZERO);
          if (ISDIGIT (tmpa))
            return -1;
          while (tmpb == NUMERIC_ZERO || tmpb == thousands_sep)
            tmpb = *++b;
          if (tmpb == decimal_point)
            do
              tmpb = *++b;
            while (tmpb == NUMERIC_ZERO);
          return - ISDIGIT (tmpb);
        }
      do
        tmpb = *++b;
      while (tmpb == NUMERIC_ZERO || tmpb == thousands_sep);

      while (tmpa == tmpb && ISDIGIT (tmpa))
        {
          do
            tmpa = *++a;
          while (tmpa == thousands_sep);
          do
            tmpb = *++b;
          while (tmpb == thousands_sep);
        }

      if ((tmpa == decimal_point && !ISDIGIT (tmpb))
          || (tmpb == decimal_point && !ISDIGIT (tmpa)))
        return fraccompare (b, a, decimal_point);

      tmp = tmpb - tmpa;

      for (log_a = 0; ISDIGIT (tmpa); ++log_a)
        do
          tmpa = *++a;
        while (tmpa == thousands_sep);

      for (log_b = 0; ISDIGIT (tmpb); ++log_b)
        do
          tmpb = *++b;
        while (tmpb == thousands_sep);

      if (log_a != log_b)
        return log_a < log_b ? 1 : -1;

      if (!log_a)
        return 0;

      return tmp;
    }
  else if (tmpb == NEGATION_SIGN)
    {
      do
        tmpb = *++b;
      while (tmpb == NUMERIC_ZERO || tmpb == thousands_sep);
      if (tmpb == decimal_point)
        do
          tmpb = *++b;
        while (tmpb == NUMERIC_ZERO);
      if (ISDIGIT (tmpb))
        return 1;
      while (tmpa == NUMERIC_ZERO || tmpa == thousands_sep)
        tmpa = *++a;
      if (tmpa == decimal_point)
        do
          tmpa = *++a;
        while (tmpa == NUMERIC_ZERO);
      return ISDIGIT (tmpa);
    }
  else
    {
      while (tmpa == NUMERIC_ZERO || tmpa == thousands_sep)
        tmpa = *++a;
      while (tmpb == NUMERIC_ZERO || tmpb == thousands_sep)
        tmpb = *++b;

      while (tmpa == tmpb && ISDIGIT (tmpa))
        {
          do
            tmpa = *++a;
          while (tmpa == thousands_sep);
          do
            tmpb = *++b;
          while (tmpb == thousands_sep);
        }

      if ((tmpa == decimal_point && !ISDIGIT (tmpb))
          || (tmpb == decimal_point && !ISDIGIT (tmpa)))
        return fraccompare (a, b, decimal_point);

      tmp = tmpa - tmpb;

      for (log_a = 0; ISDIGIT (tmpa); ++log_a)
        do
          tmpa = *++a;
        while (tmpa == thousands_sep);

      for (log_b = 0; ISDIGIT (tmpb); ++log_b)
        do
          tmpb = *++b;
        while (tmpb == thousands_sep);

      if (log_a != log_b)
        return log_a < log_b ? -1 : 1;

      if (!log_a)
        return 0;

      return tmp;
    }
}

#endif
