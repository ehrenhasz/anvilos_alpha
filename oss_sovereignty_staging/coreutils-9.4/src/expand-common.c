 
bool convert_entire_line = false;

 
static uintmax_t tab_size = 0;

 
static uintmax_t extend_size = 0;

 
static uintmax_t increment_size = 0;

 
size_t max_column_width;

 
static uintmax_t *tab_list = nullptr;

 
static size_t n_tabs_allocated = 0;

 
static size_t first_free_tab = 0;

 
static char **file_list = nullptr;

 
static char *stdin_argv[] =
{
  (char *) "-", nullptr
};

 
static bool have_read_stdin = false;

 
int exit_status = EXIT_SUCCESS;



 
extern void
add_tab_stop (uintmax_t tabval)
{
  uintmax_t prev_column = first_free_tab ? tab_list[first_free_tab - 1] : 0;
  uintmax_t column_width = prev_column <= tabval ? tabval - prev_column : 0;

  if (first_free_tab == n_tabs_allocated)
    tab_list = X2NREALLOC (tab_list, &n_tabs_allocated);
  tab_list[first_free_tab++] = tabval;

  if (max_column_width < column_width)
    {
      if (SIZE_MAX < column_width)
        error (EXIT_FAILURE, 0, _("tabs are too far apart"));
      max_column_width = column_width;
    }
}

static bool
set_extend_size (uintmax_t tabval)
{
  bool ok = true;

  if (extend_size)
    {
      error (0, 0,
             _("'/' specifier only allowed"
               " with the last value"));
      ok = false;
    }
  extend_size = tabval;

  return ok;
}

static bool
set_increment_size (uintmax_t tabval)
{
  bool ok = true;

  if (increment_size)
    {
      error (0,0,
             _("'+' specifier only allowed"
               " with the last value"));
      ok = false;
    }
  increment_size = tabval;

  return ok;
}

 
extern void
parse_tab_stops (char const *stops)
{
  bool have_tabval = false;
  uintmax_t tabval = 0;
  bool extend_tabval = false;
  bool increment_tabval = false;
  char const *num_start = nullptr;
  bool ok = true;

  for (; *stops; stops++)
    {
      if (*stops == ',' || isblank (to_uchar (*stops)))
        {
          if (have_tabval)
            {
              if (extend_tabval)
                {
                  if (! set_extend_size (tabval))
                    {
                      ok = false;
                      break;
                    }
                }
              else if (increment_tabval)
                {
                  if (! set_increment_size (tabval))
                    {
                      ok = false;
                      break;
                    }
                }
              else
                add_tab_stop (tabval);
            }
          have_tabval = false;
        }
      else if (*stops == '/')
        {
          if (have_tabval)
            {
              error (0, 0, _("'/' specifier not at start of number: %s"),
                     quote (stops));
              ok = false;
            }
          extend_tabval = true;
          increment_tabval = false;
        }
      else if (*stops == '+')
        {
          if (have_tabval)
            {
              error (0, 0, _("'+' specifier not at start of number: %s"),
                     quote (stops));
              ok = false;
            }
          increment_tabval = true;
          extend_tabval = false;
        }
      else if (ISDIGIT (*stops))
        {
          if (!have_tabval)
            {
              tabval = 0;
              have_tabval = true;
              num_start = stops;
            }

           
          if (!DECIMAL_DIGIT_ACCUMULATE (tabval, *stops - '0', uintmax_t))
            {
              size_t len = strspn (num_start, "0123456789");
              char *bad_num = ximemdup0 (num_start, len);
              error (0, 0, _("tab stop is too large %s"), quote (bad_num));
              free (bad_num);
              ok = false;
              stops = num_start + len - 1;
            }
        }
      else
        {
          error (0, 0, _("tab size contains invalid character(s): %s"),
                 quote (stops));
          ok = false;
          break;
        }
    }

  if (ok && have_tabval)
    {
      if (extend_tabval)
        ok &= set_extend_size (tabval);
      else if (increment_tabval)
        ok &= set_increment_size (tabval);
      else
        add_tab_stop (tabval);
    }

  if (! ok)
    exit (EXIT_FAILURE);
}

 

static void
validate_tab_stops (uintmax_t const *tabs, size_t entries)
{
  uintmax_t prev_tab = 0;

  for (size_t i = 0; i < entries; i++)
    {
      if (tabs[i] == 0)
        error (EXIT_FAILURE, 0, _("tab size cannot be 0"));
      if (tabs[i] <= prev_tab)
        error (EXIT_FAILURE, 0, _("tab sizes must be ascending"));
      prev_tab = tabs[i];
    }

  if (increment_size && extend_size)
    error (EXIT_FAILURE, 0, _("'/' specifier is mutually exclusive with '+'"));
}

 
extern void
finalize_tab_stops (void)
{
  validate_tab_stops (tab_list, first_free_tab);

  if (first_free_tab == 0)
    tab_size = max_column_width = extend_size
                                  ? extend_size : increment_size
                                                  ? increment_size : 8;
  else if (first_free_tab == 1 && ! extend_size && ! increment_size)
    tab_size = tab_list[0];
  else
    tab_size = 0;
}


extern uintmax_t
get_next_tab_column (const uintmax_t column, size_t *tab_index,
                     bool *last_tab)
{
  *last_tab = false;

   
  if (tab_size)
    return column + (tab_size - column % tab_size);

   
  for ( ; *tab_index < first_free_tab ; (*tab_index)++ )
    {
        uintmax_t tab = tab_list[*tab_index];
        if (column < tab)
            return tab;
    }

   
  if (extend_size)
    return column + (extend_size - column % extend_size);

   
  if (increment_size)
    {
      uintmax_t end_tab = tab_list[first_free_tab - 1];

      return column + (increment_size - ((column - end_tab) % increment_size));
    }

  *last_tab = true;
  return 0;
}




 
extern void
set_file_list (char **list)
{
  have_read_stdin = false;

  if (!list)
    file_list = stdin_argv;
  else
    file_list = list;
}

 

extern FILE *
next_file (FILE *fp)
{
  static char *prev_file;
  char *file;

  if (fp)
    {
      int err = errno;
      if (!ferror (fp))
        err = 0;
      if (STREQ (prev_file, "-"))
        clearerr (fp);		 
      else if (fclose (fp) != 0)
        err = errno;
      if (err)
        {
          error (0, err, "%s", quotef (prev_file));
          exit_status = EXIT_FAILURE;
        }
    }

  while ((file = *file_list++) != nullptr)
    {
      if (STREQ (file, "-"))
        {
          have_read_stdin = true;
          fp = stdin;
        }
      else
        fp = fopen (file, "r");
      if (fp)
        {
          prev_file = file;
          fadvise (fp, FADVISE_SEQUENTIAL);
          return fp;
        }
      error (0, errno, "%s", quotef (file));
      exit_status = EXIT_FAILURE;
    }
  return nullptr;
}

 
extern void
cleanup_file_list_stdin (void)
{
    if (have_read_stdin && fclose (stdin) != 0)
      error (EXIT_FAILURE, errno, "-");
}


extern void
emit_tab_list_info (void)
{
   
  fputs (_("\
  -t, --tabs=LIST  use comma separated list of tab positions.\n\
"), stdout);
  fputs (_("\
                     The last specified position can be prefixed with '/'\n\
                     to specify a tab size to use after the last\n\
                     explicitly specified tab stop.  Also a prefix of '+'\n\
                     can be used to align remaining tab stops relative to\n\
                     the last specified tab stop instead of the first column\n\
"), stdout);
}
