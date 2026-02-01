 
#include "strnlen1.h"

#include <string.h>

 
 
size_t
strnlen1 (const char *string, size_t maxlen)
{
  const char *end = (const char *) memchr (string, '\0', maxlen);
  if (end != NULL)
    return end - string + 1;
  else
    return maxlen;
}
