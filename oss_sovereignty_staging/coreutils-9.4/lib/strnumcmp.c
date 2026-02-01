 

#include <config.h>

#include "strnumcmp-in.h"

 

int
strnumcmp (char const *a, char const *b,
           int decimal_point, int thousands_sep)
{
  return numcompare (a, b, decimal_point, thousands_sep);
}
