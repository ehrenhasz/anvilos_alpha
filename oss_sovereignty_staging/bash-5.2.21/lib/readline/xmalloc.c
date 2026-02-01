 

 

#define READLINE_LIBRARY

#if defined (HAVE_CONFIG_H)
#include <config.h>
#endif

#include <stdio.h>

#if defined (HAVE_STDLIB_H)
#  include <stdlib.h>
#else
#  include "ansi_stdlib.h"
#endif  

#include "xmalloc.h"

 
 
 
 
 

static void
memory_error_and_abort (char *fname)
{
  fprintf (stderr, "%s: out of virtual memory\n", fname);
  exit (2);
}

 
PTR_T
xmalloc (size_t bytes)
{
  PTR_T temp;

  temp = malloc (bytes);
  if (temp == 0)
    memory_error_and_abort ("xmalloc");
  return (temp);
}

PTR_T
xrealloc (PTR_T pointer, size_t bytes)
{
  PTR_T temp;

  temp = pointer ? realloc (pointer, bytes) : malloc (bytes);

  if (temp == 0)
    memory_error_and_abort ("xrealloc");
  return (temp);
}
