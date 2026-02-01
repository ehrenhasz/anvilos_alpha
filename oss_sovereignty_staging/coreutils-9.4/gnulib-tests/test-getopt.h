 


 
#if defined __GETOPT_PREFIX || (__GLIBC__ >= 2 && !defined __UCLIBC__)
# define OPTIND_MIN 0
#elif HAVE_DECL_OPTRESET
# define OPTIND_MIN (optreset = 1)
#else
# define OPTIND_MIN 1
#endif

static void
getopt_loop (int argc, const char **argv,
             const char *options,
             int *a_seen, int *b_seen,
             const char **p_value, const char **q_value,
             int *non_options_count, const char **non_options,
             int *unrecognized, bool *message_issued)
{
  int c;
  int pos = ftell (stderr);

  while ((c = getopt (argc, (char **) argv, options)) != -1)
    {
      switch (c)
        {
        case 'a':
          (*a_seen)++;
          break;
        case 'b':
          (*b_seen)++;
          break;
        case 'p':
          *p_value = optarg;
          break;
        case 'q':
          *q_value = optarg;
          break;
        case '\1':
           
          ASSERT (options[0] == '-');
          non_options[(*non_options_count)++] = optarg;
          break;
        case ':':
           
          ASSERT (options[0] == ':'
                  || ((options[0] == '-' || options[0] == '+')
                      && options[1] == ':'));
          FALLTHROUGH;
        case '?':
          *unrecognized = optopt;
          break;
        default:
          *unrecognized = c;
          break;
        }
    }

  *message_issued = pos < ftell (stderr);
}

static void
test_getopt (void)
{
  int start;
  bool posixly = !!getenv ("POSIXLY_CORRECT");
   
#if defined __GETOPT_PREFIX || !(__GLIBC__ >= 2 || defined __MINGW32__)
   
  posixly = true;
#endif

   
  for (start = OPTIND_MIN; start <= 1; start++)
    {
      int a_seen = 0;
      int b_seen = 0;
      const char *p_value = NULL;
      const char *q_value = NULL;
      int non_options_count = 0;
      const char *non_options[10];
      int unrecognized = 0;
      bool output;
      int argc = 0;
      const char *argv[10];

      argv[argc++] = "program";
      argv[argc++] = "-a";
      argv[argc++] = "foo";
      argv[argc++] = "bar";
      argv[argc] = NULL;
      optind = start;
      opterr = 1;
      getopt_loop (argc, argv, "ab",
                   &a_seen, &b_seen, &p_value, &q_value,
                   &non_options_count, non_options, &unrecognized, &output);
      ASSERT (a_seen == 1);
      ASSERT (b_seen == 0);
      ASSERT (p_value == NULL);
      ASSERT (q_value == NULL);
      ASSERT (non_options_count == 0);
      ASSERT (unrecognized == 0);
      ASSERT (optind == 2);
      ASSERT (!output);
    }
  for (start = OPTIND_MIN; start <= 1; start++)
    {
      int a_seen = 0;
      int b_seen = 0;
      const char *p_value = NULL;
      const char *q_value = NULL;
      int non_options_count = 0;
      const char *non_options[10];
      int unrecognized = 0;
      bool output;
      int argc = 0;
      const char *argv[10];

      argv[argc++] = "program";
      argv[argc++] = "-b";
      argv[argc++] = "-a";
      argv[argc++] = "foo";
      argv[argc++] = "bar";
      argv[argc] = NULL;
      optind = start;
      opterr = 1;
      getopt_loop (argc, argv, "ab",
                   &a_seen, &b_seen, &p_value, &q_value,
                   &non_options_count, non_options, &unrecognized, &output);
      ASSERT (a_seen == 1);
      ASSERT (b_seen == 1);
      ASSERT (p_value == NULL);
      ASSERT (q_value == NULL);
      ASSERT (non_options_count == 0);
      ASSERT (unrecognized == 0);
      ASSERT (optind == 3);
      ASSERT (!output);
    }
  for (start = OPTIND_MIN; start <= 1; start++)
    {
      int a_seen = 0;
      int b_seen = 0;
      const char *p_value = NULL;
      const char *q_value = NULL;
      int non_options_count = 0;
      const char *non_options[10];
      int unrecognized = 0;
      bool output;
      int argc = 0;
      const char *argv[10];

      argv[argc++] = "program";
      argv[argc++] = "-ba";
      argv[argc++] = "foo";
      argv[argc++] = "bar";
      argv[argc] = NULL;
      optind = start;
      opterr = 1;
      getopt_loop (argc, argv, "ab",
                   &a_seen, &b_seen, &p_value, &q_value,
                   &non_options_count, non_options, &unrecognized, &output);
      ASSERT (a_seen == 1);
      ASSERT (b_seen == 1);
      ASSERT (p_value == NULL);
      ASSERT (q_value == NULL);
      ASSERT (non_options_count == 0);
      ASSERT (unrecognized == 0);
      ASSERT (optind == 2);
      ASSERT (!output);
    }
  for (start = OPTIND_MIN; start <= 1; start++)
    {
      int a_seen = 0;
      int b_seen = 0;
      const char *p_value = NULL;
      const char *q_value = NULL;
      int non_options_count = 0;
      const char *non_options[10];
      int unrecognized = 0;
      bool output;
      int argc = 0;
      const char *argv[10];

      argv[argc++] = "program";
      argv[argc++] = "-ab";
      argv[argc++] = "-a";
      argv[argc++] = "foo";
      argv[argc++] = "bar";
      argv[argc] = NULL;
      optind = start;
      opterr = 1;
      getopt_loop (argc, argv, "ab",
                   &a_seen, &b_seen, &p_value, &q_value,
                   &non_options_count, non_options, &unrecognized, &output);
      ASSERT (a_seen == 2);
      ASSERT (b_seen == 1);
      ASSERT (p_value == NULL);
      ASSERT (q_value == NULL);
      ASSERT (non_options_count == 0);
      ASSERT (unrecognized == 0);
      ASSERT (optind == 3);
      ASSERT (!output);
    }

   
  for (start = OPTIND_MIN; start <= 1; start++)
    {
      int a_seen = 0;
      int b_seen = 0;
      const char *p_value = NULL;
      const char *q_value = NULL;
      int non_options_count = 0;
      const char *non_options[10];
      int unrecognized = 0;
      bool output;
      int argc = 0;
      const char *argv[10];

      argv[argc++] = "program";
      argv[argc++] = "-pfoo";
      argv[argc++] = "bar";
      argv[argc] = NULL;
      optind = start;
      opterr = 1;
      getopt_loop (argc, argv, "p:q:",
                   &a_seen, &b_seen, &p_value, &q_value,
                   &non_options_count, non_options, &unrecognized, &output);
      ASSERT (a_seen == 0);
      ASSERT (b_seen == 0);
      ASSERT (p_value != NULL && strcmp (p_value, "foo") == 0);
      ASSERT (q_value == NULL);
      ASSERT (non_options_count == 0);
      ASSERT (unrecognized == 0);
      ASSERT (optind == 2);
      ASSERT (!output);
    }
  for (start = OPTIND_MIN; start <= 1; start++)
    {
      int a_seen = 0;
      int b_seen = 0;
      const char *p_value = NULL;
      const char *q_value = NULL;
      int non_options_count = 0;
      const char *non_options[10];
      int unrecognized = 0;
      bool output;
      int argc = 0;
      const char *argv[10];

      argv[argc++] = "program";
      argv[argc++] = "-p";
      argv[argc++] = "foo";
      argv[argc++] = "bar";
      argv[argc] = NULL;
      optind = start;
      opterr = 1;
      getopt_loop (argc, argv, "p:q:",
                   &a_seen, &b_seen, &p_value, &q_value,
                   &non_options_count, non_options, &unrecognized, &output);
      ASSERT (a_seen == 0);
      ASSERT (b_seen == 0);
      ASSERT (p_value != NULL && strcmp (p_value, "foo") == 0);
      ASSERT (q_value == NULL);
      ASSERT (non_options_count == 0);
      ASSERT (unrecognized == 0);
      ASSERT (optind == 3);
      ASSERT (!output);
    }
  for (start = OPTIND_MIN; start <= 1; start++)
    {
      int a_seen = 0;
      int b_seen = 0;
      const char *p_value = NULL;
      const char *q_value = NULL;
      int non_options_count = 0;
      const char *non_options[10];
      int unrecognized = 0;
      bool output;
      int argc = 0;
      const char *argv[10];

      argv[argc++] = "program";
      argv[argc++] = "-ab";
      argv[argc++] = "-q";
      argv[argc++] = "baz";
      argv[argc++] = "-pfoo";
      argv[argc++] = "bar";
      argv[argc] = NULL;
      optind = start;
      opterr = 1;
      getopt_loop (argc, argv, "abp:q:",
                   &a_seen, &b_seen, &p_value, &q_value,
                   &non_options_count, non_options, &unrecognized, &output);
      ASSERT (a_seen == 1);
      ASSERT (b_seen == 1);
      ASSERT (p_value != NULL && strcmp (p_value, "foo") == 0);
      ASSERT (q_value != NULL && strcmp (q_value, "baz") == 0);
      ASSERT (non_options_count == 0);
      ASSERT (unrecognized == 0);
      ASSERT (optind == 5);
      ASSERT (!output);
    }

#if GNULIB_TEST_GETOPT_GNU
   
  for (start = OPTIND_MIN; start <= 1; start++)
    {
      int a_seen = 0;
      int b_seen = 0;
      const char *p_value = NULL;
      const char *q_value = NULL;
      int non_options_count = 0;
      const char *non_options[10];
      int unrecognized = 0;
      bool output;
      int argc = 0;
      const char *argv[10];

      argv[argc++] = "program";
      argv[argc++] = "-pfoo";
      argv[argc++] = "bar";
      argv[argc] = NULL;
      optind = start;
      opterr = 1;
      getopt_loop (argc, argv, "p::q::",
                   &a_seen, &b_seen, &p_value, &q_value,
                   &non_options_count, non_options, &unrecognized, &output);
      ASSERT (a_seen == 0);
      ASSERT (b_seen == 0);
      ASSERT (p_value != NULL && strcmp (p_value, "foo") == 0);
      ASSERT (q_value == NULL);
      ASSERT (non_options_count == 0);
      ASSERT (unrecognized == 0);
      ASSERT (optind == 2);
      ASSERT (!output);
    }
  for (start = OPTIND_MIN; start <= 1; start++)
    {
      int a_seen = 0;
      int b_seen = 0;
      const char *p_value = NULL;
      const char *q_value = NULL;
      int non_options_count = 0;
      const char *non_options[10];
      int unrecognized = 0;
      bool output;
      int argc = 0;
      const char *argv[10];

      argv[argc++] = "program";
      argv[argc++] = "-p";
      argv[argc++] = "foo";
      argv[argc++] = "bar";
      argv[argc] = NULL;
      optind = start;
      opterr = 1;
      getopt_loop (argc, argv, "p::q::",
                   &a_seen, &b_seen, &p_value, &q_value,
                   &non_options_count, non_options, &unrecognized, &output);
      ASSERT (a_seen == 0);
      ASSERT (b_seen == 0);
      ASSERT (p_value == NULL);
      ASSERT (q_value == NULL);
      ASSERT (non_options_count == 0);
      ASSERT (unrecognized == 0);
      ASSERT (optind == 2);
      ASSERT (!output);
    }
  for (start = OPTIND_MIN; start <= 1; start++)
    {
      int a_seen = 0;
      int b_seen = 0;
      const char *p_value = NULL;
      const char *q_value = NULL;
      int non_options_count = 0;
      const char *non_options[10];
      int unrecognized = 0;
      bool output;
      int argc = 0;
      const char *argv[10];

      argv[argc++] = "program";
      argv[argc++] = "-p";
      argv[argc++] = "-a";
      argv[argc++] = "bar";
      argv[argc] = NULL;
      optind = start;
      opterr = 1;
      getopt_loop (argc, argv, "abp::q::",
                   &a_seen, &b_seen, &p_value, &q_value,
                   &non_options_count, non_options, &unrecognized, &output);
      ASSERT (a_seen == 1);
      ASSERT (b_seen == 0);
      ASSERT (p_value == NULL);
      ASSERT (q_value == NULL);
      ASSERT (non_options_count == 0);
      ASSERT (unrecognized == 0);
      ASSERT (optind == 3);
      ASSERT (!output);
    }
#endif  

   
  for (start = OPTIND_MIN; start <= 1; start++)
    {
      int a_seen = 0;
      int b_seen = 0;
      const char *p_value = NULL;
      const char *q_value = NULL;
      int non_options_count = 0;
      const char *non_options[10];
      int unrecognized = 0;
      bool output;
      int argc = 0;
      const char *argv[10];

      argv[argc++] = "program";
      argv[argc++] = "-p";
      argv[argc++] = "foo";
      argv[argc++] = "-x";
      argv[argc++] = "-a";
      argv[argc++] = "bar";
      argv[argc] = NULL;
      optind = start;
      opterr = 42;
      getopt_loop (argc, argv, "abp:q:",
                   &a_seen, &b_seen, &p_value, &q_value,
                   &non_options_count, non_options, &unrecognized, &output);
      ASSERT (a_seen == 1);
      ASSERT (b_seen == 0);
      ASSERT (p_value != NULL && strcmp (p_value, "foo") == 0);
      ASSERT (q_value == NULL);
      ASSERT (non_options_count == 0);
      ASSERT (unrecognized == 'x');
      ASSERT (optind == 5);
      ASSERT (output);
    }
  for (start = OPTIND_MIN; start <= 1; start++)
    {
      int a_seen = 0;
      int b_seen = 0;
      const char *p_value = NULL;
      const char *q_value = NULL;
      int non_options_count = 0;
      const char *non_options[10];
      int unrecognized = 0;
      bool output;
      int argc = 0;
      const char *argv[10];

      argv[argc++] = "program";
      argv[argc++] = "-p";
      argv[argc++] = "foo";
      argv[argc++] = "-x";
      argv[argc++] = "-a";
      argv[argc++] = "bar";
      argv[argc] = NULL;
      optind = start;
      opterr = 0;
      getopt_loop (argc, argv, "abp:q:",
                   &a_seen, &b_seen, &p_value, &q_value,
                   &non_options_count, non_options, &unrecognized, &output);
      ASSERT (a_seen == 1);
      ASSERT (b_seen == 0);
      ASSERT (p_value != NULL && strcmp (p_value, "foo") == 0);
      ASSERT (q_value == NULL);
      ASSERT (non_options_count == 0);
      ASSERT (unrecognized == 'x');
      ASSERT (optind == 5);
      ASSERT (!output);
    }
  for (start = OPTIND_MIN; start <= 1; start++)
    {
      int a_seen = 0;
      int b_seen = 0;
      const char *p_value = NULL;
      const char *q_value = NULL;
      int non_options_count = 0;
      const char *non_options[10];
      int unrecognized = 0;
      bool output;
      int argc = 0;
      const char *argv[10];

      argv[argc++] = "program";
      argv[argc++] = "-p";
      argv[argc++] = "foo";
      argv[argc++] = "-x";
      argv[argc++] = "-a";
      argv[argc++] = "bar";
      argv[argc] = NULL;
      optind = start;
      opterr = 1;
      getopt_loop (argc, argv, ":abp:q:",
                   &a_seen, &b_seen, &p_value, &q_value,
                   &non_options_count, non_options, &unrecognized, &output);
      ASSERT (a_seen == 1);
      ASSERT (b_seen == 0);
      ASSERT (p_value != NULL && strcmp (p_value, "foo") == 0);
      ASSERT (q_value == NULL);
      ASSERT (non_options_count == 0);
      ASSERT (unrecognized == 'x');
      ASSERT (optind == 5);
      ASSERT (!output);
    }
  for (start = OPTIND_MIN; start <= 1; start++)
    {
      int a_seen = 0;
      int b_seen = 0;
      const char *p_value = NULL;
      const char *q_value = NULL;
      int non_options_count = 0;
      const char *non_options[10];
      int unrecognized = 0;
      bool output;
      int argc = 0;
      const char *argv[10];

      argv[argc++] = "program";
      argv[argc++] = "-p";
      argv[argc++] = "foo";
      argv[argc++] = "-:";
      argv[argc++] = "-a";
      argv[argc++] = "bar";
      argv[argc] = NULL;
      optind = start;
      opterr = 42;
      getopt_loop (argc, argv, "abp:q:",
                   &a_seen, &b_seen, &p_value, &q_value,
                   &non_options_count, non_options, &unrecognized, &output);
      ASSERT (a_seen == 1);
      ASSERT (b_seen == 0);
      ASSERT (p_value != NULL && strcmp (p_value, "foo") == 0);
      ASSERT (q_value == NULL);
      ASSERT (non_options_count == 0);
      ASSERT (unrecognized == ':');
      ASSERT (optind == 5);
      ASSERT (output);
    }
  for (start = OPTIND_MIN; start <= 1; start++)
    {
      int a_seen = 0;
      int b_seen = 0;
      const char *p_value = NULL;
      const char *q_value = NULL;
      int non_options_count = 0;
      const char *non_options[10];
      int unrecognized = 0;
      bool output;
      int argc = 0;
      const char *argv[10];

      argv[argc++] = "program";
      argv[argc++] = "-p";
      argv[argc++] = "foo";
      argv[argc++] = "-:";
      argv[argc++] = "-a";
      argv[argc++] = "bar";
      argv[argc] = NULL;
      optind = start;
      opterr = 0;
      getopt_loop (argc, argv, "abp:q:",
                   &a_seen, &b_seen, &p_value, &q_value,
                   &non_options_count, non_options, &unrecognized, &output);
      ASSERT (a_seen == 1);
      ASSERT (b_seen == 0);
      ASSERT (p_value != NULL && strcmp (p_value, "foo") == 0);
      ASSERT (q_value == NULL);
      ASSERT (non_options_count == 0);
      ASSERT (unrecognized == ':');
      ASSERT (optind == 5);
      ASSERT (!output);
    }
  for (start = OPTIND_MIN; start <= 1; start++)
    {
      int a_seen = 0;
      int b_seen = 0;
      const char *p_value = NULL;
      const char *q_value = NULL;
      int non_options_count = 0;
      const char *non_options[10];
      int unrecognized = 0;
      bool output;
      int argc = 0;
      const char *argv[10];

      argv[argc++] = "program";
      argv[argc++] = "-p";
      argv[argc++] = "foo";
      argv[argc++] = "-:";
      argv[argc++] = "-a";
      argv[argc++] = "bar";
      argv[argc] = NULL;
      optind = start;
      opterr = 1;
      getopt_loop (argc, argv, ":abp:q:",
                   &a_seen, &b_seen, &p_value, &q_value,
                   &non_options_count, non_options, &unrecognized, &output);
      ASSERT (a_seen == 1);
      ASSERT (b_seen == 0);
      ASSERT (p_value != NULL && strcmp (p_value, "foo") == 0);
      ASSERT (q_value == NULL);
      ASSERT (non_options_count == 0);
      ASSERT (unrecognized == ':');
      ASSERT (optind == 5);
      ASSERT (!output);
    }

   
  for (start = OPTIND_MIN; start <= 1; start++)
    {
      int a_seen = 0;
      int b_seen = 0;
      const char *p_value = NULL;
      const char *q_value = NULL;
      int non_options_count = 0;
      const char *non_options[10];
      int unrecognized = 0;
      bool output;
      int argc = 0;
      const char *argv[10];

      argv[argc++] = "program";
      argv[argc++] = "-ap";
      argv[argc] = NULL;
      optind = start;
      opterr = 1;
      getopt_loop (argc, argv, "abp:q:",
                   &a_seen, &b_seen, &p_value, &q_value,
                   &non_options_count, non_options, &unrecognized, &output);
      ASSERT (a_seen == 1);
      ASSERT (b_seen == 0);
      ASSERT (p_value == NULL);
      ASSERT (q_value == NULL);
      ASSERT (non_options_count == 0);
      ASSERT (unrecognized == 'p');
      ASSERT (optind == 2);
      ASSERT (output);
    }
  for (start = OPTIND_MIN; start <= 1; start++)
    {
      int a_seen = 0;
      int b_seen = 0;
      const char *p_value = NULL;
      const char *q_value = NULL;
      int non_options_count = 0;
      const char *non_options[10];
      int unrecognized = 0;
      bool output;
      int argc = 0;
      const char *argv[10];

      argv[argc++] = "program";
      argv[argc++] = "-ap";
      argv[argc] = NULL;
      optind = start;
      opterr = 0;
      getopt_loop (argc, argv, "abp:q:",
                   &a_seen, &b_seen, &p_value, &q_value,
                   &non_options_count, non_options, &unrecognized, &output);
      ASSERT (a_seen == 1);
      ASSERT (b_seen == 0);
      ASSERT (p_value == NULL);
      ASSERT (q_value == NULL);
      ASSERT (non_options_count == 0);
      ASSERT (unrecognized == 'p');
      ASSERT (optind == 2);
      ASSERT (!output);
    }
  for (start = OPTIND_MIN; start <= 1; start++)
    {
      int a_seen = 0;
      int b_seen = 0;
      const char *p_value = NULL;
      const char *q_value = NULL;
      int non_options_count = 0;
      const char *non_options[10];
      int unrecognized = 0;
      bool output;
      int argc = 0;
      const char *argv[10];

      argv[argc++] = "program";
      argv[argc++] = "-ap";
      argv[argc] = NULL;
      optind = start;
      opterr = 1;
      getopt_loop (argc, argv, ":abp:q:",
                   &a_seen, &b_seen, &p_value, &q_value,
                   &non_options_count, non_options, &unrecognized, &output);
      ASSERT (a_seen == 1);
      ASSERT (b_seen == 0);
      ASSERT (p_value == NULL);
      ASSERT (q_value == NULL);
      ASSERT (non_options_count == 0);
      ASSERT (unrecognized == 'p');
      ASSERT (optind == 2);
      ASSERT (!output);
    }

   
  for (start = OPTIND_MIN; start <= 1; start++)
    {
      int a_seen = 0;
      int b_seen = 0;
      const char *p_value = NULL;
      const char *q_value = NULL;
      int non_options_count = 0;
      const char *non_options[10];
      int unrecognized = 0;
      bool output;
      int argc = 0;
      const char *argv[10];

      argv[argc++] = "program";
      argv[argc++] = "donald";
      argv[argc++] = "-p";
      argv[argc++] = "billy";
      argv[argc++] = "duck";
      argv[argc++] = "-a";
      argv[argc++] = "bar";
      argv[argc] = NULL;
      optind = start;
      opterr = 1;
      getopt_loop (argc, argv, "abp:q:",
                   &a_seen, &b_seen, &p_value, &q_value,
                   &non_options_count, non_options, &unrecognized, &output);
      if (posixly)
        {
          ASSERT (strcmp (argv[0], "program") == 0);
          ASSERT (strcmp (argv[1], "donald") == 0);
          ASSERT (strcmp (argv[2], "-p") == 0);
          ASSERT (strcmp (argv[3], "billy") == 0);
          ASSERT (strcmp (argv[4], "duck") == 0);
          ASSERT (strcmp (argv[5], "-a") == 0);
          ASSERT (strcmp (argv[6], "bar") == 0);
          ASSERT (argv[7] == NULL);
          ASSERT (a_seen == 0);
          ASSERT (b_seen == 0);
          ASSERT (p_value == NULL);
          ASSERT (q_value == NULL);
          ASSERT (non_options_count == 0);
          ASSERT (unrecognized == 0);
          ASSERT (optind == 1);
          ASSERT (!output);
        }
      else
        {
          ASSERT (strcmp (argv[0], "program") == 0);
          ASSERT (strcmp (argv[1], "-p") == 0);
          ASSERT (strcmp (argv[2], "billy") == 0);
          ASSERT (strcmp (argv[3], "-a") == 0);
          ASSERT (strcmp (argv[4], "donald") == 0);
          ASSERT (strcmp (argv[5], "duck") == 0);
          ASSERT (strcmp (argv[6], "bar") == 0);
          ASSERT (argv[7] == NULL);
          ASSERT (a_seen == 1);
          ASSERT (b_seen == 0);
          ASSERT (p_value != NULL && strcmp (p_value, "billy") == 0);
          ASSERT (q_value == NULL);
          ASSERT (non_options_count == 0);
          ASSERT (unrecognized == 0);
          ASSERT (optind == 4);
          ASSERT (!output);
        }
    }

   
  for (start = OPTIND_MIN; start <= 1; start++)
    {
      int a_seen = 0;
      int b_seen = 0;
      const char *p_value = NULL;
      const char *q_value = NULL;
      int non_options_count = 0;
      const char *non_options[10];
      int unrecognized = 0;
      bool output;
      int argc = 0;
      const char *argv[20];

      argv[argc++] = "program";
      argv[argc++] = "donald";
      argv[argc++] = "-p";
      argv[argc++] = "billy";
      argv[argc++] = "duck";
      argv[argc++] = "-a";
      argv[argc++] = "--";
      argv[argc++] = "-b";
      argv[argc++] = "foo";
      argv[argc++] = "-q";
      argv[argc++] = "johnny";
      argv[argc++] = "bar";
      argv[argc] = NULL;
      optind = start;
      opterr = 1;
      getopt_loop (argc, argv, "abp:q:",
                   &a_seen, &b_seen, &p_value, &q_value,
                   &non_options_count, non_options, &unrecognized, &output);
      if (posixly)
        {
          ASSERT (strcmp (argv[0], "program") == 0);
          ASSERT (strcmp (argv[1], "donald") == 0);
          ASSERT (strcmp (argv[2], "-p") == 0);
          ASSERT (strcmp (argv[3], "billy") == 0);
          ASSERT (strcmp (argv[4], "duck") == 0);
          ASSERT (strcmp (argv[5], "-a") == 0);
          ASSERT (strcmp (argv[6], "--") == 0);
          ASSERT (strcmp (argv[7], "-b") == 0);
          ASSERT (strcmp (argv[8], "foo") == 0);
          ASSERT (strcmp (argv[9], "-q") == 0);
          ASSERT (strcmp (argv[10], "johnny") == 0);
          ASSERT (strcmp (argv[11], "bar") == 0);
          ASSERT (argv[12] == NULL);
          ASSERT (a_seen == 0);
          ASSERT (b_seen == 0);
          ASSERT (p_value == NULL);
          ASSERT (q_value == NULL);
          ASSERT (non_options_count == 0);
          ASSERT (unrecognized == 0);
          ASSERT (optind == 1);
          ASSERT (!output);
        }
      else
        {
          ASSERT (strcmp (argv[0], "program") == 0);
          ASSERT (strcmp (argv[1], "-p") == 0);
          ASSERT (strcmp (argv[2], "billy") == 0);
          ASSERT (strcmp (argv[3], "-a") == 0);
          ASSERT (strcmp (argv[4], "--") == 0);
          ASSERT (strcmp (argv[5], "donald") == 0);
          ASSERT (strcmp (argv[6], "duck") == 0);
          ASSERT (strcmp (argv[7], "-b") == 0);
          ASSERT (strcmp (argv[8], "foo") == 0);
          ASSERT (strcmp (argv[9], "-q") == 0);
          ASSERT (strcmp (argv[10], "johnny") == 0);
          ASSERT (strcmp (argv[11], "bar") == 0);
          ASSERT (argv[12] == NULL);
          ASSERT (a_seen == 1);
          ASSERT (b_seen == 0);
          ASSERT (p_value != NULL && strcmp (p_value, "billy") == 0);
          ASSERT (q_value == NULL);
          ASSERT (non_options_count == 0);
          ASSERT (unrecognized == 0);
          ASSERT (optind == 5);
          ASSERT (!output);
        }
    }

#if GNULIB_TEST_GETOPT_GNU
   
  for (start = OPTIND_MIN; start <= 1; start++)
    {
      int a_seen = 0;
      int b_seen = 0;
      const char *p_value = NULL;
      const char *q_value = NULL;
      int non_options_count = 0;
      const char *non_options[10];
      int unrecognized = 0;
      bool output;
      int argc = 0;
      const char *argv[10];

      argv[argc++] = "program";
      argv[argc++] = "donald";
      argv[argc++] = "-p";
      argv[argc++] = "billy";
      argv[argc++] = "duck";
      argv[argc++] = "-a";
      argv[argc++] = "bar";
      argv[argc] = NULL;
      optind = start;
      opterr = 1;
      getopt_loop (argc, argv, "-abp:q:",
                   &a_seen, &b_seen, &p_value, &q_value,
                   &non_options_count, non_options, &unrecognized, &output);
      ASSERT (strcmp (argv[0], "program") == 0);
      ASSERT (strcmp (argv[1], "donald") == 0);
      ASSERT (strcmp (argv[2], "-p") == 0);
      ASSERT (strcmp (argv[3], "billy") == 0);
      ASSERT (strcmp (argv[4], "duck") == 0);
      ASSERT (strcmp (argv[5], "-a") == 0);
      ASSERT (strcmp (argv[6], "bar") == 0);
      ASSERT (argv[7] == NULL);
      ASSERT (a_seen == 1);
      ASSERT (b_seen == 0);
      ASSERT (p_value != NULL && strcmp (p_value, "billy") == 0);
      ASSERT (q_value == NULL);
      ASSERT (non_options_count == 3);
      ASSERT (strcmp (non_options[0], "donald") == 0);
      ASSERT (strcmp (non_options[1], "duck") == 0);
      ASSERT (strcmp (non_options[2], "bar") == 0);
      ASSERT (unrecognized == 0);
      ASSERT (optind == 7);
      ASSERT (!output);
    }

   
  for (start = OPTIND_MIN; start <= 1; start++)
    {
      int a_seen = 0;
      int b_seen = 0;
      const char *p_value = NULL;
      const char *q_value = NULL;
      int non_options_count = 0;
      const char *non_options[10];
      int unrecognized = 0;
      bool output;
      int argc = 0;
      const char *argv[20];

      argv[argc++] = "program";
      argv[argc++] = "donald";
      argv[argc++] = "-p";
      argv[argc++] = "billy";
      argv[argc++] = "duck";
      argv[argc++] = "-a";
      argv[argc++] = "--";
      argv[argc++] = "-b";
      argv[argc++] = "foo";
      argv[argc++] = "-q";
      argv[argc++] = "johnny";
      argv[argc++] = "bar";
      argv[argc] = NULL;
      optind = start;
      opterr = 1;
      getopt_loop (argc, argv, "-abp:q:",
                   &a_seen, &b_seen, &p_value, &q_value,
                   &non_options_count, non_options, &unrecognized, &output);
      ASSERT (strcmp (argv[0], "program") == 0);
      ASSERT (strcmp (argv[1], "donald") == 0);
      ASSERT (strcmp (argv[2], "-p") == 0);
      ASSERT (strcmp (argv[3], "billy") == 0);
      ASSERT (strcmp (argv[4], "duck") == 0);
      ASSERT (strcmp (argv[5], "-a") == 0);
      ASSERT (strcmp (argv[6], "--") == 0);
      ASSERT (strcmp (argv[7], "-b") == 0);
      ASSERT (strcmp (argv[8], "foo") == 0);
      ASSERT (strcmp (argv[9], "-q") == 0);
      ASSERT (strcmp (argv[10], "johnny") == 0);
      ASSERT (strcmp (argv[11], "bar") == 0);
      ASSERT (argv[12] == NULL);
      ASSERT (a_seen == 1);
      ASSERT (b_seen == 0);
      ASSERT (p_value != NULL && strcmp (p_value, "billy") == 0);
      ASSERT (q_value == NULL);
      ASSERT (!output);
      if (non_options_count == 2)
        {
           
          ASSERT (non_options_count == 2);
          ASSERT (strcmp (non_options[0], "donald") == 0);
          ASSERT (strcmp (non_options[1], "duck") == 0);
          ASSERT (unrecognized == 0);
          ASSERT (optind == 7);
        }
      else
        {
           
          ASSERT (non_options_count == 7);
          ASSERT (strcmp (non_options[0], "donald") == 0);
          ASSERT (strcmp (non_options[1], "duck") == 0);
          ASSERT (strcmp (non_options[2], "-b") == 0);
          ASSERT (strcmp (non_options[3], "foo") == 0);
          ASSERT (strcmp (non_options[4], "-q") == 0);
          ASSERT (strcmp (non_options[5], "johnny") == 0);
          ASSERT (strcmp (non_options[6], "bar") == 0);
          ASSERT (unrecognized == 0);
          ASSERT (optind == 12);
        }
    }

   
  for (start = OPTIND_MIN; start <= 1; start++)
    {
      int a_seen = 0;
      int b_seen = 0;
      const char *p_value = NULL;
      const char *q_value = NULL;
      int non_options_count = 0;
      const char *non_options[10];
      int unrecognized = 0;
      bool output;
      int argc = 0;
      const char *argv[10];

      argv[argc++] = "program";
      argv[argc++] = "donald";
      argv[argc++] = "-p";
      argv[argc++] = "billy";
      argv[argc++] = "duck";
      argv[argc++] = "-a";
      argv[argc++] = "bar";
      argv[argc] = NULL;
      optind = start;
      opterr = 1;
      getopt_loop (argc, argv, "abp:q:-",
                   &a_seen, &b_seen, &p_value, &q_value,
                   &non_options_count, non_options, &unrecognized, &output);
      if (posixly)
        {
          ASSERT (strcmp (argv[0], "program") == 0);
          ASSERT (strcmp (argv[1], "donald") == 0);
          ASSERT (strcmp (argv[2], "-p") == 0);
          ASSERT (strcmp (argv[3], "billy") == 0);
          ASSERT (strcmp (argv[4], "duck") == 0);
          ASSERT (strcmp (argv[5], "-a") == 0);
          ASSERT (strcmp (argv[6], "bar") == 0);
          ASSERT (argv[7] == NULL);
          ASSERT (a_seen == 0);
          ASSERT (b_seen == 0);
          ASSERT (p_value == NULL);
          ASSERT (q_value == NULL);
          ASSERT (non_options_count == 0);
          ASSERT (unrecognized == 0);
          ASSERT (optind == 1);
          ASSERT (!output);
        }
      else
        {
          ASSERT (strcmp (argv[0], "program") == 0);
          ASSERT (strcmp (argv[1], "-p") == 0);
          ASSERT (strcmp (argv[2], "billy") == 0);
          ASSERT (strcmp (argv[3], "-a") == 0);
          ASSERT (strcmp (argv[4], "donald") == 0);
          ASSERT (strcmp (argv[5], "duck") == 0);
          ASSERT (strcmp (argv[6], "bar") == 0);
          ASSERT (argv[7] == NULL);
          ASSERT (a_seen == 1);
          ASSERT (b_seen == 0);
          ASSERT (p_value != NULL && strcmp (p_value, "billy") == 0);
          ASSERT (q_value == NULL);
          ASSERT (non_options_count == 0);
          ASSERT (unrecognized == 0);
          ASSERT (optind == 4);
          ASSERT (!output);
        }
    }

   
  for (start = OPTIND_MIN; start <= 1; start++)
    {
      int a_seen = 0;
      int b_seen = 0;
      const char *p_value = NULL;
      const char *q_value = NULL;
      int non_options_count = 0;
      const char *non_options[10];
      int unrecognized = 0;
      bool output;
      int argc = 0;
      const char *argv[10];

      argv[argc++] = "program";
      argv[argc++] = "donald";
      argv[argc++] = "-p";
      argv[argc++] = "billy";
      argv[argc++] = "duck";
      argv[argc++] = "-a";
      argv[argc++] = "bar";
      argv[argc] = NULL;
      optind = start;
      opterr = 1;
      getopt_loop (argc, argv, "+abp:q:",
                   &a_seen, &b_seen, &p_value, &q_value,
                   &non_options_count, non_options, &unrecognized, &output);
      ASSERT (strcmp (argv[0], "program") == 0);
      ASSERT (strcmp (argv[1], "donald") == 0);
      ASSERT (strcmp (argv[2], "-p") == 0);
      ASSERT (strcmp (argv[3], "billy") == 0);
      ASSERT (strcmp (argv[4], "duck") == 0);
      ASSERT (strcmp (argv[5], "-a") == 0);
      ASSERT (strcmp (argv[6], "bar") == 0);
      ASSERT (argv[7] == NULL);
      ASSERT (a_seen == 0);
      ASSERT (b_seen == 0);
      ASSERT (p_value == NULL);
      ASSERT (q_value == NULL);
      ASSERT (non_options_count == 0);
      ASSERT (unrecognized == 0);
      ASSERT (optind == 1);
      ASSERT (!output);
    }
  for (start = OPTIND_MIN; start <= 1; start++)
    {
      int a_seen = 0;
      int b_seen = 0;
      const char *p_value = NULL;
      const char *q_value = NULL;
      int non_options_count = 0;
      const char *non_options[10];
      int unrecognized = 0;
      bool output;
      int argc = 0;
      const char *argv[10];

      argv[argc++] = "program";
      argv[argc++] = "-+";
      argv[argc] = NULL;
      optind = start;
      getopt_loop (argc, argv, "+abp:q:",
                   &a_seen, &b_seen, &p_value, &q_value,
                   &non_options_count, non_options, &unrecognized, &output);
      ASSERT (a_seen == 0);
      ASSERT (b_seen == 0);
      ASSERT (p_value == NULL);
      ASSERT (q_value == NULL);
      ASSERT (non_options_count == 0);
      ASSERT (unrecognized == '+');
      ASSERT (optind == 2);
      ASSERT (output);
    }

   
  for (start = OPTIND_MIN; start <= 1; start++)
    {
      int a_seen = 0;
      int b_seen = 0;
      const char *p_value = NULL;
      const char *q_value = NULL;
      int non_options_count = 0;
      const char *non_options[10];
      int unrecognized = 0;
      bool output;
      int argc = 0;
      const char *argv[20];

      argv[argc++] = "program";
      argv[argc++] = "donald";
      argv[argc++] = "-p";
      argv[argc++] = "billy";
      argv[argc++] = "duck";
      argv[argc++] = "-a";
      argv[argc++] = "--";
      argv[argc++] = "-b";
      argv[argc++] = "foo";
      argv[argc++] = "-q";
      argv[argc++] = "johnny";
      argv[argc++] = "bar";
      argv[argc] = NULL;
      optind = start;
      opterr = 1;
      getopt_loop (argc, argv, "+abp:q:",
                   &a_seen, &b_seen, &p_value, &q_value,
                   &non_options_count, non_options, &unrecognized, &output);
      ASSERT (strcmp (argv[0], "program") == 0);
      ASSERT (strcmp (argv[1], "donald") == 0);
      ASSERT (strcmp (argv[2], "-p") == 0);
      ASSERT (strcmp (argv[3], "billy") == 0);
      ASSERT (strcmp (argv[4], "duck") == 0);
      ASSERT (strcmp (argv[5], "-a") == 0);
      ASSERT (strcmp (argv[6], "--") == 0);
      ASSERT (strcmp (argv[7], "-b") == 0);
      ASSERT (strcmp (argv[8], "foo") == 0);
      ASSERT (strcmp (argv[9], "-q") == 0);
      ASSERT (strcmp (argv[10], "johnny") == 0);
      ASSERT (strcmp (argv[11], "bar") == 0);
      ASSERT (argv[12] == NULL);
      ASSERT (a_seen == 0);
      ASSERT (b_seen == 0);
      ASSERT (p_value == NULL);
      ASSERT (q_value == NULL);
      ASSERT (non_options_count == 0);
      ASSERT (unrecognized == 0);
      ASSERT (optind == 1);
      ASSERT (!output);
    }
#endif  

   
  for (start = OPTIND_MIN; start <= 1; start++)
    {
      int a_seen = 0;
      int b_seen = 0;
      const char *p_value = NULL;
      const char *q_value = NULL;
      int non_options_count = 0;
      const char *non_options[10];
      int unrecognized = 0;
      bool output;
      int argc = 0;
      const char *argv[10];

      argv[argc++] = "program";
      argv[argc++] = "donald";
      argv[argc++] = "-p";
      argv[argc++] = "billy";
      argv[argc++] = "duck";
      argv[argc++] = "-a";
      argv[argc++] = "bar";
      argv[argc] = NULL;
      optind = start;
      opterr = 1;
      getopt_loop (argc, argv, "abp:q:+",
                   &a_seen, &b_seen, &p_value, &q_value,
                   &non_options_count, non_options, &unrecognized, &output);
      if (posixly)
        {
          ASSERT (strcmp (argv[0], "program") == 0);
          ASSERT (strcmp (argv[1], "donald") == 0);
          ASSERT (strcmp (argv[2], "-p") == 0);
          ASSERT (strcmp (argv[3], "billy") == 0);
          ASSERT (strcmp (argv[4], "duck") == 0);
          ASSERT (strcmp (argv[5], "-a") == 0);
          ASSERT (strcmp (argv[6], "bar") == 0);
          ASSERT (argv[7] == NULL);
          ASSERT (a_seen == 0);
          ASSERT (b_seen == 0);
          ASSERT (p_value == NULL);
          ASSERT (q_value == NULL);
          ASSERT (non_options_count == 0);
          ASSERT (unrecognized == 0);
          ASSERT (optind == 1);
          ASSERT (!output);
        }
      else
        {
          ASSERT (strcmp (argv[0], "program") == 0);
          ASSERT (strcmp (argv[1], "-p") == 0);
          ASSERT (strcmp (argv[2], "billy") == 0);
          ASSERT (strcmp (argv[3], "-a") == 0);
          ASSERT (strcmp (argv[4], "donald") == 0);
          ASSERT (strcmp (argv[5], "duck") == 0);
          ASSERT (strcmp (argv[6], "bar") == 0);
          ASSERT (argv[7] == NULL);
          ASSERT (a_seen == 1);
          ASSERT (b_seen == 0);
          ASSERT (p_value != NULL && strcmp (p_value, "billy") == 0);
          ASSERT (q_value == NULL);
          ASSERT (non_options_count == 0);
          ASSERT (unrecognized == 0);
          ASSERT (optind == 4);
          ASSERT (!output);
        }
    }

#if GNULIB_TEST_GETOPT_GNU
   
  for (start = OPTIND_MIN; start <= 1; start++)
    {
      int argc = 0;
      const char *argv[10];
      int pos = ftell (stderr);

      argv[argc++] = "program";
      argv[argc++] = "-W";
      argv[argc++] = "dummy";
      argv[argc] = NULL;
      optind = start;
      opterr = 1;
      ASSERT (getopt (argc, (char **) argv, "W;") == 'W');
      ASSERT (ftell (stderr) == pos);
      ASSERT (optind == 2);
    }
#endif  
}
