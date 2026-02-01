 
#ifdef ARGMATCH_DIE_DECL

_Noreturn ARGMATCH_DIE_DECL;
ARGMATCH_DIE_DECL { exit (1); }

#endif

int
main (int argc, char **argv)
{
  int exclude_options = 0;
  struct exclude *exclude = new_exclude ();

  if (argc == 1)
    error (1, 0, "usage: %s file -- words...", argv[0]);

  while (--argc)
    {
      char *opt = *++argv;
      if (opt[0] == '-')
        {
          int neg = 0;
          int flag;
          char *s = opt + 1;

          if (opt[1] == '-' && opt[2] == 0)
            {
              argc--;
              break;
            }
          if (strlen (s) > 3 && memcmp (s, "no-", 3) == 0)
            {
              neg = 1;
              s += 3;
            }
          flag = XARGMATCH (opt, s, exclude_keywords, exclude_flags);
          if (neg)
            exclude_options &= ~flag;
          else
            exclude_options |= flag;

           
          if (strcmp (s, "leading_dir") == 0 && FNM_LEADING_DIR == 0)
            exit (77);

           
          if (strcmp (s, "casefold") == 0 && FNM_CASEFOLD == 0)
            exit (77);
        }
      else if (add_exclude_file (add_exclude, exclude, opt,
                                 exclude_options, '\n') != 0)
        error (1, errno, "error loading %s", opt);
    }

  for (; argc; --argc)
    {
      char *word = *++argv;

      printf ("%s: %d\n", word, excluded_file_name (exclude, word));
    }

  free_exclude (exclude);
  return 0;
}
