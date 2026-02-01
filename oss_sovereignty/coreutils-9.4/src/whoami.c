 
 

#include <config.h>
#include <stdio.h>
#include <sys/types.h>
#include <pwd.h>

#include "system.h"
#include "long-options.h"
#include "quote.h"

 
#define PROGRAM_NAME "whoami"

#define AUTHORS proper_name ("Richard Mlynarik")

void
usage (int status)
{
  if (status != EXIT_SUCCESS)
    emit_try_help ();
  else
    {
      printf (_("Usage: %s [OPTION]...\n"), program_name);
      fputs (_("\
Print the user name associated with the current effective user ID.\n\
Same as id -un.\n\
\n\
"), stdout);
      fputs (HELP_OPTION_DESCRIPTION, stdout);
      fputs (VERSION_OPTION_DESCRIPTION, stdout);
      emit_ancillary_info (PROGRAM_NAME);
    }
  exit (status);
}

int
main (int argc, char **argv)
{
  struct passwd *pw;
  uid_t uid;
  uid_t NO_UID = -1;

  initialize_main (&argc, &argv);
  set_program_name (argv[0]);
  setlocale (LC_ALL, "");
  bindtextdomain (PACKAGE, LOCALEDIR);
  textdomain (PACKAGE);

  atexit (close_stdout);

  parse_gnu_standard_options_only (argc, argv, PROGRAM_NAME, PACKAGE_NAME,
                                   Version, true, usage, AUTHORS,
                                   (char const *) nullptr);

  if (optind != argc)
    {
      error (0, 0, _("extra operand %s"), quote (argv[optind]));
      usage (EXIT_FAILURE);
    }

  errno = 0;
  uid = geteuid ();
  pw = uid == NO_UID && errno ? nullptr : getpwuid (uid);
  if (!pw)
    error (EXIT_FAILURE, errno, _("cannot find name for user ID %lu"),
           (unsigned long int) uid);
  puts (pw->pw_name);
  return EXIT_SUCCESS;
}
