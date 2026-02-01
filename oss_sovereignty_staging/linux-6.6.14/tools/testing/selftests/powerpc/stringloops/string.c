
 

#include <stddef.h>

 
size_t test_strlen(const char *s)
{
	const char *sc;

	for (sc = s; *sc != '\0'; ++sc)
		 ;
	return sc - s;
}
