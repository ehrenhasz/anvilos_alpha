 

 

#if defined (HAVE_CONFIG_H)
#include <config.h>
#endif

#include <stdio.h>

#if defined (HAVE_STDLIB_H)
#  include <stdlib.h>
#else
#  include "ansi_stdlib.h"
#endif  

 
#ifndef PTR_T

#if defined (__STDC__)
#  define PTR_T void *
#else
#  define PTR_T char *
#endif

#endif  

 
 
 
 
 

static void
memory_error_and_abort (fname)
     char *fname;
{
  fprintf (stderr, "%s: out of virtual memory\n", fname);
  exit (2);
}

 
PTR_T
xmalloc (bytes)
     size_t bytes;
{
  PTR_T temp;

  temp = malloc (bytes);
  if (temp == 0)
    memory_error_and_abort ("xmalloc");
  return (temp);
}

PTR_T
xrealloc (pointer, bytes)
     PTR_T pointer;
     size_t bytes;
{
  PTR_T temp;

  temp = pointer ? realloc (pointer, bytes) : malloc (bytes);

  if (temp == 0)
    memory_error_and_abort ("xrealloc");
  return (temp);
}

void
xfree (string)
     PTR_T string;
{
  if (string)
    free (string);
}
