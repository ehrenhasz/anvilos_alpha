 

 

#if defined (HAVE_CONFIG_H)
#include <config.h>
#endif

#include "bashtypes.h"
#include <stdio.h>

#if defined (HAVE_UNISTD_H)
#  include <unistd.h>
#endif

#if defined (HAVE_STDLIB_H)
#  include <stdlib.h>
#else
#  include "ansi_stdlib.h"
#endif  

#include "error.h"

#include "bashintl.h"

#if !defined (PTR_T)
#  if defined (__STDC__)
#    define PTR_T void *
#  else
#    define PTR_T char *
#  endif  
#endif  

#if HAVE_SBRK && !HAVE_DECL_SBRK
extern char *sbrk();
#endif

#if HAVE_SBRK && defined (USING_BASH_MALLOC)
static PTR_T lbreak;
static int brkfound;
static size_t allocated;
#endif

 
 
 
 
 

#if HAVE_SBRK && defined (USING_BASH_MALLOC)
#define FINDBRK() \
do { \
  if (brkfound == 0) \
    { \
      lbreak = (PTR_T)sbrk (0); \
      brkfound++; \
    } \
} while (0)

static size_t
findbrk ()
{
  FINDBRK();
  return (char *)sbrk (0) - (char *)lbreak;
}
#else
#define FINDBRK()
#endif

static void
allocerr (func, bytes)
     const char *func;
     size_t bytes;
{
#if HAVE_SBRK && defined (USING_BASH_MALLOC)
      allocated = findbrk ();
      fatal_error (_("%s: cannot allocate %lu bytes (%lu bytes allocated)"), func, (unsigned long)bytes, (unsigned long)allocated);
#else
      fatal_error (_("%s: cannot allocate %lu bytes"), func, (unsigned long)bytes);
#endif  
}

 
PTR_T
xmalloc (bytes)
     size_t bytes;
{
  PTR_T temp;

#if defined (DEBUG)
  if (bytes == 0)
    internal_warning("xmalloc: size argument is 0");
#endif

  FINDBRK();
  temp = malloc (bytes);

  if (temp == 0)
    allocerr ("xmalloc", bytes);

  return (temp);
}

PTR_T
xrealloc (pointer, bytes)
     PTR_T pointer;
     size_t bytes;
{
  PTR_T temp;

#if defined (DEBUG)
  if (bytes == 0)
    internal_warning("xrealloc: size argument is 0");
#endif

  FINDBRK();
  temp = pointer ? realloc (pointer, bytes) : malloc (bytes);

  if (temp == 0)
    allocerr ("xrealloc", bytes);

  return (temp);
}

 
void
xfree (string)
     PTR_T string;
{
  if (string)
    free (string);
}

#ifdef USING_BASH_MALLOC
#include <malloc/shmalloc.h>

static void
sh_allocerr (func, bytes, file, line)
     const char *func;
     size_t bytes;
     char *file;
     int line;
{
#if HAVE_SBRK
      allocated = findbrk ();
      fatal_error (_("%s: %s:%d: cannot allocate %lu bytes (%lu bytes allocated)"), func, file, line, (unsigned long)bytes, (unsigned long)allocated);
#else
      fatal_error (_("%s: %s:%d: cannot allocate %lu bytes"), func, file, line, (unsigned long)bytes);
#endif  
}

PTR_T
sh_xmalloc (bytes, file, line)
     size_t bytes;
     char *file;
     int line;
{
  PTR_T temp;

#if defined (DEBUG)
  if (bytes == 0)
    internal_warning("xmalloc: %s:%d: size argument is 0", file, line);
#endif

  FINDBRK();
  temp = sh_malloc (bytes, file, line);

  if (temp == 0)
    sh_allocerr ("xmalloc", bytes, file, line);

  return (temp);
}

PTR_T
sh_xrealloc (pointer, bytes, file, line)
     PTR_T pointer;
     size_t bytes;
     char *file;
     int line;
{
  PTR_T temp;

#if defined (DEBUG)
  if (bytes == 0)
    internal_warning("xrealloc: %s:%d: size argument is 0", file, line);
#endif

  FINDBRK();
  temp = pointer ? sh_realloc (pointer, bytes, file, line) : sh_malloc (bytes, file, line);

  if (temp == 0)
    sh_allocerr ("xrealloc", bytes, file, line);

  return (temp);
}

void
sh_xfree (string, file, line)
     PTR_T string;
     char *file;
     int line;
{
  if (string)
    sh_free (string, file, line);
}
#endif
