 

 

#define READLINE_LIBRARY

#if defined (HAVE_CONFIG_H)
#include <config.h>
#endif

#if defined (HAVE_STDLIB_H)
#  include <stdlib.h>
#else
#  include "ansi_stdlib.h"
#endif  

#include "xmalloc.h"

 
 
 
 
 

 
void
xfree (PTR_T string)
{
  if (string)
    free (string);
}
