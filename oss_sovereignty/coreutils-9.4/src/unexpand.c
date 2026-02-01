 

#include <config.h>

#include <stdio.h>
#include <getopt.h>
#include <sys/types.h>
#include "system.h"
#include "expand-common.h"

 
#define PROGRAM_NAME "unexpand"

#define AUTHORS proper_name ("David MacKenzie")



 
enum
{
  CONVERT_FIRST_ONLY_OPTION = CHAR_MAX + 1
};

static struct option const longopts[] =
{
  {"tabs", required_argument, nullptr, 't'},
  {"all", no_argument, nullptr, 'a'},
  {"first-only", no_argument, nullptr, CONVERT_FIRST_ONLY_OPTION},
  {GETOPT_HELP_OPTION_DECL},
  {GETOPT_VERSION_OPTION_DECL},
  {nullptr, 0, nullptr, 0}
};

void
usage (int status)
{
  if (status != EXIT_SUCCESS)
    emit_try_help ();
  else
    {
      printf (_("\
Usage: %s [OPTION]... [FILE]...\n\
"),
              program_name);
      fputs (_("\
Convert blanks in each FILE to tabs, writing to standard output.\n\
"), stdout);

      emit_stdin_note ();
      emit_mandatory_arg_note ();

      fputs (_("\
  -a, --all        convert all blanks, instead of just initial blanks\n\
      --first-only  convert only leading sequences of blanks (overrides -a)\n\
  -t, --tabs=N     have tabs N characters apart instead of 8 (enables -a)\n\
"), stdout);
      emit_tab_list_info ();
      fputs (HELP_OPTION_DESCRIPTION, stdout);
      fputs (VERSION_OPTION_DESCRIPTION, stdout);
      emit_ancillary_info (PROGRAM_NAME);
    }
  exit (status);
}

/* Change blanks to tabs, writing to stdout.
   Read each file in 'file_list', in order.  */

static void
unexpand (void)
{
  /* Input stream.  */
  FILE *fp = next_file (nullptr);

  /* The array of pending blanks.  In non-POSIX locales, blanks can
     include characters other than spaces, so the blanks must be
     stored, not merely counted.  */
  char *pending_blank;

  if (!fp)
    return;

  /* The worst case is a non-blank character, then one blank, then a
     tab stop, then MAX_COLUMN_WIDTH - 1 blanks, then a non-blank; so
     allocate MAX_COLUMN_WIDTH bytes to store the blanks.  */
  pending_blank = xmalloc (max_column_width);

  while (true)
    {
      /* Input character, or EOF.  */
      int c;

      /* If true, perform translations.  */
      bool convert = true;


      /* The following variables have valid values only when CONVERT
         is true:  */

      /* Column of next input character.  */
      uintmax_t column = 0;

      /* Column the next input tab stop is on.  */
      uintmax_t next_tab_column = 0;

      /* Index in TAB_LIST of next tab stop to examine.  */
      size_t tab_index = 0;

      /* If true, the first pending blank came just before a tab stop.  */
      bool one_blank_before_tab_stop = false;

      /* If true, the previous input character was a blank.  This is
         initially true, since initial strings of blanks are treated
         as if the line was preceded by a blank.  */
      bool prev_blank = true;

      /* Number of pending columns of blanks.  */
      size_t pending = 0;


      /* Convert a line of text.  */

      do
        {
          while ((c = getc (fp)) < 0 && (fp = next_file (fp)))
            continue;

          if (convert)
            {
              bool blank = !! isblank (c);

              if (blank)
                {
                  bool last_tab;

                  next_tab_column = get_next_tab_column (column, &tab_index,
                                                         &last_tab);

                  if (last_tab)
                    convert = false;

                  if (convert)
                    {
                      if (next_tab_column < column)
                        error (EXIT_FAILURE, 0, _("input line is too long"));

                      if (c == '\t')
                        {
                          column = next_tab_column;

                          if (pending)
                            pending_blank[0] = '\t';
                        }
                      else
                        {
                          column++;

                          if (! (prev_blank && column == next_tab_column))
                            {
                              /* It is not yet known whether the pending blanks
                                 will be replaced by tabs.  */
                              if (column == next_tab_column)
                                one_blank_before_tab_stop = true;
                              pending_blank[pending++] = c;
                              prev_blank = true;
                              continue;
                            }

                          /* Replace the pending blanks by a tab or two.  */
                          pending_blank[0] = c = '\t';
                        }

                      /* Discard pending blanks, unless it was a single
                         blank just before the previous tab stop.  */
                      pending = one_blank_before_tab_stop;
                    }
                }
              else if (c == '\b')
                {
                  /* Go back one column, and force recalculation of the
                     next tab stop.  */
                  column -= !!column;
                  next_tab_column = column;
                  tab_index -= !!tab_index;
                }
              else
                {
                  column++;
                  if (!column)
                    error (EXIT_FAILURE, 0, _("input line is too long"));
                }

              if (pending)
                {
                  if (pending > 1 && one_blank_before_tab_stop)
                    pending_blank[0] = '\t';
                  if (fwrite (pending_blank, 1, pending, stdout) != pending)
                    write_error ();
                  pending = 0;
                  one_blank_before_tab_stop = false;
                }

              prev_blank = blank;
              convert &= convert_entire_line || blank;
            }

          if (c < 0)
            {
              free (pending_blank);
              return;
            }

          if (putchar (c) < 0)
            write_error ();
        }
      while (c != '\n');
    }
}

int
main (int argc, char **argv)
{
  bool have_tabval = false;
  uintmax_t tabval IF_LINT ( = 0);
  int c;

  /* If true, cancel the effect of any -a (explicit or implicit in -t),
     so that only leading blanks will be considered.  */
  bool convert_first_only = false;

  initialize_main (&argc, &argv);
  set_program_name (argv[0]);
  setlocale (LC_ALL, "");
  bindtextdomain (PACKAGE, LOCALEDIR);
  textdomain (PACKAGE);

  atexit (close_stdout);

  while ((c = getopt_long (argc, argv, ",0123456789at:", longopts, nullptr))
         != -1)
    {
      switch (c)
        {
        case '?':
          usage (EXIT_FAILURE);
        case 'a':
          convert_entire_line = true;
          break;
        case 't':
          convert_entire_line = true;
          parse_tab_stops (optarg);
          break;
        case CONVERT_FIRST_ONLY_OPTION:
          convert_first_only = true;
          break;
        case ',':
          if (have_tabval)
            add_tab_stop (tabval);
          have_tabval = false;
          break;
        case_GETOPT_HELP_CHAR;
        case_GETOPT_VERSION_CHAR (PROGRAM_NAME, AUTHORS);
        default:
          if (!have_tabval)
            {
              tabval = 0;
              have_tabval = true;
            }
          if (!DECIMAL_DIGIT_ACCUMULATE (tabval, c - '0', uintmax_t))
            error (EXIT_FAILURE, 0, _("tab stop value is too large"));
          break;
        }
    }

  if (convert_first_only)
    convert_entire_line = false;

  if (have_tabval)
    add_tab_stop (tabval);

  finalize_tab_stops ();

  set_file_list ((optind < argc) ? &argv[optind] : nullptr);

  unexpand ();

  cleanup_file_list_stdin ();

  return exit_status;
}
