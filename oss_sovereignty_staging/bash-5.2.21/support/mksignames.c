 

 

#include <config.h>

#include <sys/types.h>
#include <signal.h>

#include <stdio.h>
#if defined (HAVE_STDLIB_H)
#  include <stdlib.h>
#else
#  include "ansi_stdlib.h"
#endif  

 
#if !defined (NSIG)
#  define NSIG 64
#endif

#define LASTSIG NSIG+2

 
extern void initialize_signames ();
extern char *signal_names[];

char *progname;

void
write_signames (stream)
     FILE *stream;
{
  register int i;

  fprintf (stream, "/* This file was automatically created by %s.\n",
	   progname);
  fprintf (stream, "   Do not edit.  Edit support/mksignames.c instead. */\n\n");
  fprintf (stream,
	   "/* A translation list so we can be polite to our users. */\n");
#if defined (CROSS_COMPILING)
  fprintf (stream, "extern char *signal_names[];\n\n");
  fprintf (stream, "extern void initialize_signames PARAMS((void));\n\n");
#else
  fprintf (stream, "char *signal_names[NSIG + 4] = {\n");

  for (i = 0; i <= LASTSIG; i++)
    fprintf (stream, "    \"%s\",\n", signal_names[i]);

  fprintf (stream, "    (char *)0x0\n");
  fprintf (stream, "};\n\n");
  fprintf (stream, "#define initialize_signames()\n\n");
#endif
}

int
main (argc, argv)
     int argc;
     char **argv;
{
  char *stream_name;
  FILE *stream;

  progname = argv[0];

  if (argc == 1)
    {
      stream_name = "stdout";
      stream = stdout;
    }
  else if (argc == 2)
    {
      stream_name = argv[1];
      stream = fopen (stream_name, "w");
    }
  else
    {
      fprintf (stderr, "Usage: %s [output-file]\n", progname);
      exit (1);
    }

  if (!stream)
    {
      fprintf (stderr, "%s: %s: cannot open for writing\n",
	       progname, stream_name);
      exit (2);
    }

#if !defined (CROSS_COMPILING)
  initialize_signames ();
#endif
  write_signames (stream);
  exit (0);
}
