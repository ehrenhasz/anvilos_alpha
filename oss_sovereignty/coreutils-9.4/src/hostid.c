 

#include <config.h>
#include <stdio.h>
#include <sys/types.h>

#include "system.h"
#include "long-options.h"
#include "quote.h"

 
#define PROGRAM_NAME "hostid"

#define AUTHORS proper_name ("Jim Meyering")

void
usage (int status)
{
  if (status != EXIT_SUCCESS)
    emit_try_help ();
  else
    {
      printf (_("\
Usage: %s [OPTION]\n\
Print the numeric identifier (in hexadecimal) for the current host.\n\
\n\
"), program_name);
      fputs (HELP_OPTION_DESCRIPTION, stdout);
      fputs (VERSION_OPTION_DESCRIPTION, stdout);
      emit_ancillary_info (PROGRAM_NAME);
    }
  exit (status);
}

int
main (int argc, char **argv)
{
  unsigned int id;

  initialize_main (&argc, &argv);
  set_program_name (argv[0]);
  setlocale (LC_ALL, "");
  bindtextdomain (PACKAGE, LOCALEDIR);
  textdomain (PACKAGE);

  atexit (close_stdout);

  parse_gnu_standard_options_only (argc, argv, PROGRAM_NAME, PACKAGE_NAME,
                                   Version, true, usage, AUTHORS,
                                   (char const *) nullptr);

  if (optind < argc)
    {
      error (0, 0, _("extra operand %s"), quote (argv[optind]));
      usage (EXIT_FAILURE);
    }

  id = gethostid ();

  /* POSIX says gethostid returns a "32-bit identifier" but is silent
     whether it's sign-extended.  Turn off any sign-extension.  This
     is a no-op unless unsigned int is wider than 32 bits.  */
  id &= 0xffffffff;

  printf ("%08x\n", id);

  return EXIT_SUCCESS;
}
