 

 

 

#if !defined (CROSS_COMPILING) 
#  include <config.h>
#else	 
 
#  define HAVE_UNISTD_H
#  define HAVE_STRING_H
#  define HAVE_STDLIB_H

#  define HAVE_RENAME
#endif  

#if defined (HAVE_UNISTD_H)
#  ifdef _MINIX
#    include <sys/types.h>
#  endif
#  include <unistd.h>
#endif

#ifndef _MINIX
#  include "../bashtypes.h"
#  if defined (HAVE_SYS_FILE_H)
#    include <sys/file.h>
#  endif
#endif

#include "posixstat.h"
#include "filecntl.h"

#include "../bashansi.h"
#include <stdio.h>
#include <errno.h>

#include "stdc.h"

#include "../builtins.h"
#include "tmpbuiltins.h"

#if defined (USING_BASH_MALLOC)
#undef xmalloc
#undef xrealloc
#undef xfree

#undef malloc
#undef free		 
#endif

#ifndef errno
extern int errno;
#endif

#if !defined (__STDC__) && !defined (strcpy)
extern char *strcpy ();
#endif  

#define whitespace(c) (((c) == ' ') || ((c) == '\t'))

 
#define BUILTIN_FLAG_SPECIAL	0x01
#define BUILTIN_FLAG_ASSIGNMENT 0x02
#define BUILTIN_FLAG_POSIX_BUILTIN 0x04

#define BASE_INDENT	4

 
int separate_helpfiles = 0;

 
int single_longdoc_strings = 1;

 
char *helpfile_directory;

 

int write_helpfiles PARAMS((struct builtin *));

 
int
main (argc, argv)
     int argc;
     char **argv;
{
  int arg_index = 1;

  while (arg_index < argc && argv[arg_index][0] == '-')
    {
      char *arg = argv[arg_index++];

      if (strcmp (arg, "-noproduction") == 0)
	;
      else if (strcmp (arg, "-H") == 0)
	helpfile_directory = argv[arg_index++];
      else if (strcmp (arg, "-S") == 0)
	single_longdoc_strings = 0;
      else
	{
	  fprintf (stderr, "%s: Unknown flag %s.\n", argv[0], arg);
	  exit (2);
	}
    }

  write_helpfiles(shell_builtins);

  exit (0);
}

 
void
write_documentation (stream, documentation, indentation)
     FILE *stream;
     char *documentation;
     int indentation;
{
  if (stream == 0)
    return;

  if (documentation)
    fprintf (stream, "%*s%s\n", indentation, " ", documentation);
}

int
write_helpfiles (builtins)
     struct builtin *builtins;
{
  char *helpfile, *bname, *fname;
  FILE *helpfp;
  int i, hdlen;
  struct builtin b;

  i = mkdir ("helpfiles", 0777);
  if (i < 0 && errno != EEXIST)
    {
      fprintf (stderr, "write_helpfiles: helpfiles: cannot create directory\n");
      return -1;
    }

  hdlen = strlen ("helpfiles/");
  for (i = 0; i < num_shell_builtins; i++)
    {
      b = builtins[i];

      fname = (char *)b.handle;
      helpfile = (char *)malloc (hdlen + strlen (fname) + 1);
      if (helpfile == 0)
	{
	  fprintf (stderr, "gen-helpfiles: cannot allocate memory\n");
	  exit (1);
	}
      sprintf (helpfile, "helpfiles/%s", fname);

      helpfp = fopen (helpfile, "w");
      if (helpfp == 0)
	{
	  fprintf (stderr, "write_helpfiles: cannot open %s\n", helpfile);
	  free (helpfile);
	  continue;
	}

      write_documentation (helpfp, b.long_doc[0], 4);

      fflush (helpfp);
      fclose (helpfp);
      free (helpfile);
    }
  return 0;
}
