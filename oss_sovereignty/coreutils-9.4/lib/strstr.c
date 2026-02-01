 

#ifndef _LIBC
# include <config.h>
#endif

 
#include <string.h>

#define RETURN_TYPE char *
#define AVAILABLE(h, h_l, j, n_l)                       \
  (!memchr ((h) + (h_l), '\0', (j) + (n_l) - (h_l))     \
   && ((h_l) = (j) + (n_l)))
#include "str-two-way.h"

 
char *
strstr (const char *haystack_start, const char *needle_start)
{
  const char *haystack = haystack_start;
  const char *needle = needle_start;
  size_t needle_len;  
  size_t haystack_len;  
  bool ok = true;  

   
  while (*haystack && *needle)
    ok &= *haystack++ == *needle++;
  if (*needle)
    return NULL;
  if (ok)
    return (char *) haystack_start;

   
  needle_len = needle - needle_start;
  haystack = strchr (haystack_start + 1, *needle_start);
  if (!haystack || __builtin_expect (needle_len == 1, 0))
    return (char *) haystack;
  needle -= needle_len;
  haystack_len = (haystack > haystack_start + needle_len ? 1
                  : needle_len + haystack_start - haystack);

   
  if (needle_len < LONG_NEEDLE_THRESHOLD)
    return two_way_short_needle ((const unsigned char *) haystack,
                                 haystack_len,
                                 (const unsigned char *) needle, needle_len);
  return two_way_long_needle ((const unsigned char *) haystack, haystack_len,
                              (const unsigned char *) needle, needle_len);
}

#undef LONG_NEEDLE_THRESHOLD
