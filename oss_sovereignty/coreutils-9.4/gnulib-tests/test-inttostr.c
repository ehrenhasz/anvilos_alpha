 

#include <config.h>

#include "inttostr.h"
#include "intprops.h"
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "macros.h"

#define STREQ(a, b) (strcmp (a, b) == 0)
#define IS_TIGHT(T) (_GL_SIGNED_TYPE_OR_EXPR (T) == TYPE_SIGNED (T))
#define ISDIGIT(c) ((unsigned int) (c) - '0' <= 9)

 
#define CK(T, Fn)                                                       \
  do                                                                    \
    {                                                                   \
      char ref[100];                                                    \
      char *buf = malloc (INT_BUFSIZE_BOUND (T));                       \
      char const *p;                                                    \
      ASSERT (buf);                                                     \
      *buf = '\0';                                                      \
      ASSERT                                                            \
        ((TYPE_SIGNED (T)                                               \
          ? snprintf (ref, sizeof ref, "%jd", (intmax_t) TYPE_MINIMUM (T)) \
          : snprintf (ref, sizeof ref, "%ju", (uintmax_t) TYPE_MINIMUM (T))) \
         < sizeof ref);                                                 \
      ASSERT (STREQ ((p = Fn (TYPE_MINIMUM (T), buf)), ref));           \
          \
      ASSERT (! TYPE_SIGNED (T) || (p == buf && *p == '-'));            \
      ASSERT                                                            \
        ((TYPE_SIGNED (T)                                               \
          ? snprintf (ref, sizeof ref, "%jd", (intmax_t) TYPE_MAXIMUM (T)) \
          : snprintf (ref, sizeof ref, "%ju", (uintmax_t) TYPE_MAXIMUM (T))) \
         < sizeof ref);                                                 \
      ASSERT (STREQ ((p = Fn (TYPE_MAXIMUM (T), buf)), ref));           \
                \
      ASSERT (! IS_TIGHT (T) || TYPE_SIGNED (T)                         \
              || (p == buf && ISDIGIT (*p)));                           \
      free (buf);                                                       \
    }                                                                   \
  while (0)

int
main (void)
{
  size_t b_size = 2;
  char *b = malloc (b_size);
  ASSERT (b);

   
  if (snprintf (b, b_size, "%ju", (uintmax_t) 3) == 1
      && b[0] == '3' && b[1] == '\0')
    {
      CK (int,          inttostr);
      CK (unsigned int, uinttostr);
      CK (off_t,        offtostr);
      CK (uintmax_t,    umaxtostr);
      CK (intmax_t,     imaxtostr);
      free (b);
      return 0;
    }

   
  free (b);
  return 77;
}
