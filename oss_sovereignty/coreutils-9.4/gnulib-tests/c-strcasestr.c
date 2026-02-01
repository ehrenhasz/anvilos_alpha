 
#include "c-strcasestr.h"

#include <string.h>

#include "c-ctype.h"
#include "c-strcase.h"

 
#define RETURN_TYPE char *
#define AVAILABLE(h, h_l, j, n_l)                       \
  (!memchr ((h) + (h_l), '\0', (j) + (n_l) - (h_l))     \
   && ((h_l) = (j) + (n_l)))
#define CANON_ELEMENT c_tolower
#define CMP_FUNC(p1, p2, l)                             \
  c_strncasecmp ((const char *) (p1), (const char *) (p2), l)
#include "str-two-way.h"

 
char *
c_strcasestr (const char *haystack_start, const char *needle_start)
{
  const char *haystack = haystack_start;
  const char *needle = needle_start;
  size_t needle_len;  
  size_t haystack_len;  
  bool ok = true;  

   
  while (*haystack && *needle)
    ok &= (c_tolower ((unsigned char) *haystack++)
           == c_tolower ((unsigned char) *needle++));
  if (*needle)
    return NULL;
  if (ok)
    return (char *) haystack_start;
  needle_len = needle - needle_start;
  haystack = haystack_start + 1;
  haystack_len = needle_len - 1;

   
  if (needle_len < LONG_NEEDLE_THRESHOLD)
    return two_way_short_needle ((const unsigned char *) haystack,
                                 haystack_len,
                                 (const unsigned char *) needle_start,
                                 needle_len);
  return two_way_long_needle ((const unsigned char *) haystack, haystack_len,
                              (const unsigned char *) needle_start,
                              needle_len);
}

#undef LONG_NEEDLE_THRESHOLD
