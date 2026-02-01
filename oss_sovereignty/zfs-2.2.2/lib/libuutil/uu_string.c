 

 

 

#include <string.h>
#include <sys/types.h>
#include <ctype.h>
#include "libuutil.h"

 
boolean_t
uu_streq(const char *a, const char *b)
{
	return (strcmp(a, b) == 0);
}

 
boolean_t
uu_strcaseeq(const char *a, const char *b)
{
	return (strcasecmp(a, b) == 0);
}

 
boolean_t
uu_strbw(const char *a, const char *b)
{
	return (strncmp(a, b, strlen(b)) == 0);
}
