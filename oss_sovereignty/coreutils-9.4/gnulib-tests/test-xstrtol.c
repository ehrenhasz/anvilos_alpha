 
static void
print_no_progname (void)
{
}

int
main (int argc, char **argv)
{
  strtol_error s_err;
  int i;

  error_print_progname = print_no_progname;

  for (i = 1; i < argc; i++)
    {
      char *p;
      __strtol_t val;

      s_err = __xstrtol (argv[i], &p, 0, &val, "bckMw0");
      if (s_err == LONGINT_OK)
        {
          printf ("%s->%" __spec " (%s)\n", argv[i], val, p);
        }
      else
        {
          xstrtol_fatal (s_err, -2, 'X', NULL, argv[i]);
        }
    }
  exit (0);
}
