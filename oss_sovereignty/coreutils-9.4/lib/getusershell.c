 

#include <config.h>

 
#include <unistd.h>

#ifndef SHELLS_FILE
# ifndef __DJGPP__
 
#  define SHELLS_FILE "/etc/shells"
# else
 
#  define SHELLS_FILE "/dev/env/DJDIR/etc/shells"
# endif
#endif

#include <stdlib.h>
#include <ctype.h>

#include "stdio--.h"
#include "xalloc.h"

#if GNULIB_GETUSERSHELL_SINGLE_THREAD
# include "unlocked-io.h"
#endif

static idx_t readname (char **, idx_t *, FILE *);

#if ! defined ADDITIONAL_DEFAULT_SHELLS && defined __MSDOS__
# define ADDITIONAL_DEFAULT_SHELLS \
  "c:/dos/command.com", "c:/windows/command.com", "c:/command.com",
#else
# define ADDITIONAL_DEFAULT_SHELLS  
#endif

 
static char const* const default_shells[] =
{
  ADDITIONAL_DEFAULT_SHELLS
  "/bin/sh", "/bin/csh", "/usr/bin/sh", "/usr/bin/csh", NULL
};

 
static size_t default_index = 0;

 
static FILE *shellstream = NULL;

 
static char *line = NULL;

 
static idx_t line_size = 0;

 

char *
getusershell (void)
{
  if (default_index > 0)
    {
      if (default_shells[default_index])
         
        return xstrdup (default_shells[default_index++]);
      return NULL;
    }

  if (shellstream == NULL)
    {
      shellstream = fopen (SHELLS_FILE, "r");
      if (shellstream == NULL)
        {
           
          default_index = 1;
          return xstrdup (default_shells[0]);
        }
    }

  while (readname (&line, &line_size, shellstream))
    {
      if (*line != '#')
        return line;
    }
  return NULL;                   
}

 

void
setusershell (void)
{
  default_index = 0;
  if (shellstream)
    rewind (shellstream);
}

 

void
endusershell (void)
{
  if (shellstream)
    {
      fclose (shellstream);
      shellstream = NULL;
    }
}

 

static idx_t
readname (char **name, idx_t *size, FILE *stream)
{
  int c;
  size_t name_index = 0;

   
  while ((c = getc (stream)) != EOF && isspace (c))
      ;

  for (;;)
    {
      if (*size <= name_index)
        *name = xpalloc (*name, size, 1, -1, sizeof **name);
      if (c == EOF || isspace (c))
        break;
      (*name)[name_index++] = c;
      c = getc (stream);
    }
  (*name)[name_index] = '\0';
  return name_index;
}

#ifdef TEST
int
main (void)
{
  char *s;

  while (s = getusershell ())
    puts (s);
  exit (0);
}
#endif
