 

 

#include <config.h>

#if defined (HAVE_LONG_LONG_INT) && !HAVE_STRTOLL

#define QUAD		1
#undef HAVE_STRTOL

#include "strtol.c"

#endif  
