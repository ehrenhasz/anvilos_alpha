 
#ifndef EXIT_STATUS
# define EXIT_STATUS EXIT_SUCCESS
#endif

#if EXIT_STATUS == EXIT_SUCCESS
# define PROGRAM_NAME "true"
#else
# define PROGRAM_NAME "false"
#endif

#define AUTHORS proper_name ("Jim Meyering")

void
usage (int status)
{
  printf (_("\
Usage: %s [ignored command line arguments]\n\
  or:  %s OPTION\n\
"),
          program_name, program_name);
  printf ("%s\n\n",
          _(EXIT_STATUS == EXIT_SUCCESS
            ? N_("Exit with a status code indicating success.")
            : N_("Exit with a status code indicating failure.")));
  fputs (HELP_OPTION_DESCRIPTION, stdout);
  fputs (VERSION_OPTION_DESCRIPTION, stdout);
  printf (USAGE_BUILTIN_WARNING, PROGRAM_NAME);
  emit_ancillary_info (PROGRAM_NAME);
  exit (status);
}

int
main (int argc, char **argv)
{
  /* Recognize --help or --version only if it's the only command-line
     argument.  */
  if (argc == 2)
    {
      initialize_main (&argc, &argv);
      set_program_name (argv[0]);
      setlocale (LC_ALL, "");
      bindtextdomain (PACKAGE, LOCALEDIR);
      textdomain (PACKAGE);

      /* Note true(1) will return EXIT_FAILURE in the
         edge case where writes fail with GNU specific options.  */
      atexit (close_stdout);

      if (STREQ (argv[1], "--help"))
        usage (EXIT_STATUS);

      if (STREQ (argv[1], "--version"))
        version_etc (stdout, PROGRAM_NAME, PACKAGE_NAME, Version, AUTHORS,
                     (char *) nullptr);
    }

  return EXIT_STATUS;
}
