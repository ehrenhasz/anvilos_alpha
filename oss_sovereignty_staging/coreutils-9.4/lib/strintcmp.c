 

#include <config.h>

#include "strnumcmp-in.h"

#include <limits.h>

 

int
strintcmp (char const *a, char const *b)
{
  return numcompare (a, b, CHAR_MAX + 1, CHAR_MAX + 1);
}
