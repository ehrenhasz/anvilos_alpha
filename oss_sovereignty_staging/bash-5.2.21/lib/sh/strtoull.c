 

 

#include <config.h>

#if defined (HAVE_UNSIGNED_LONG_LONG_INT) && !HAVE_STRTOULL

#define QUAD		1
#define	UNSIGNED	1
#undef HAVE_STRTOL

#include "strtol.c"

#endif  
