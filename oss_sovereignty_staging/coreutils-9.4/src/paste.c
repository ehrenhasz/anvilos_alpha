 

 

#include <config.h>

#include <stdio.h>
#include <getopt.h>
#include <sys/types.h>
#include "system.h"
#include "fadvise.h"

 
#define PROGRAM_NAME "paste"

#define AUTHORS \
  proper_name ("David M. Ihnat"), \
  proper_name ("David MacKenzie")

 
#define EMPTY_DELIM '\0'

 
static bool have_read_stdin;

 
static bool serial_merge;

 
static char *delims;

 
static char const *delim_end;

static unsigned char line_delim = '\n';

static struct option const longopts[] =
{
  {"serial", no_argument, nullptr, 's'},
  {"delimiters", required_argument, nullptr, 'd'},
  {"zero-terminated", no_argument, nullptr, 'z'},
  {GETOPT_HELP_OPTION_DECL},
  {GETOPT_VERSION_OPTION_DECL},
  {nullptr, 0, nullptr, 0}
};

 

static int
collapse_escapes (char const *strptr)
{
  char *strout = xstrdup (strptr);
  bool backslash_at_end = false;

  delims = strout;

  while (*strptr)
    {
      if (*strptr != '\\')	 
        *strout++ = *strptr++;	 
      else
        {
          switch (*++strptr)
            {
            case '0':
              *strout++ = EMPTY_DELIM;
              break;

            case 'b':
              *strout++ = '\b';
              break;

            case 'f':
              *strout++ = '\f';
              break;

            case 'n':
              *strout++ = '\n';
              break;

            case 'r':
              *strout++ = '\r';
              break;

            case 't':
              *strout++ = '\t';
              break;

            case 'v':
              *strout++ = '\v';
              break;

            case '\\':
              *strout++ = '\\';
              break;

            case '\0':
              backslash_at_end = true;
              goto done;

            default:
              *strout++ = *strptr;
              break;
            }
          strptr++;
        }
    }

 done:

  delim_end = strout;
  return backslash_at_end ? 1 : 0;
}

 

static inline void
xputchar (char c)
{
  if (putchar (c) < 0)
    write_error ();
}

 

static bool
paste_parallel (size_t nfiles, char **fnamptr)
{
  bool ok = true;
   
  char *delbuf = xmalloc (nfiles + 2);

   
  FILE **fileptr = xnmalloc (nfiles + 1, sizeof *fileptr);

   
  size_t files_open;

   
  bool opened_stdin = false;

   

  for (files_open = 0; files_open < nfiles; ++files_open)
    {
      if (STREQ (fnamptr[files_open], "-"))
        {
          have_read_stdin = true;
          fileptr[files_open] = stdin;
        }
      else
        {
          fileptr[files_open] = fopen (fnamptr[files_open], "r");
          if (fileptr[files_open] == nullptr)
            error (EXIT_FAILURE, errno, "%s", quotef (fnamptr[files_open]));
          else if (fileno (fileptr[files_open]) == STDIN_FILENO)
            opened_stdin = true;
          fadvise (fileptr[files_open], FADVISE_SEQUENTIAL);
        }
    }

  if (opened_stdin && have_read_stdin)
    error (EXIT_FAILURE, 0, _("standard input is closed"));

   

  while (files_open)
    {
       
      bool somedone = false;
      char const *delimptr = delims;
      size_t delims_saved = 0;	 

      for (size_t i = 0; i < nfiles && files_open; i++)
        {
          int chr;			 
          int err;			 
          bool sometodo = false;	 

          if (fileptr[i])
            {
              chr = getc (fileptr[i]);
              err = errno;
              if (chr != EOF && delims_saved)
                {
                  if (fwrite (delbuf, 1, delims_saved, stdout) != delims_saved)
                    write_error ();
                  delims_saved = 0;
                }

              while (chr != EOF)
                {
                  sometodo = true;
                  if (chr == line_delim)
                    break;
                  xputchar (chr);
                  chr = getc (fileptr[i]);
                  err = errno;
                }
            }

          if (! sometodo)
            {
               
              if (fileptr[i])
                {
                  if (!ferror (fileptr[i]))
                    err = 0;
                  if (fileptr[i] == stdin)
                    clearerr (fileptr[i]);  
                  else if (fclose (fileptr[i]) == EOF && !err)
                    err = errno;
                  if (err)
                    {
                      error (0, err, "%s", quotef (fnamptr[i]));
                      ok = false;
                    }

                  fileptr[i] = nullptr;
                  files_open--;
                }

              if (i + 1 == nfiles)
                {
                   
                  if (somedone)
                    {
                       
                      if (delims_saved)
                        {
                          if (fwrite (delbuf, 1, delims_saved, stdout)
                              != delims_saved)
                            write_error ();
                          delims_saved = 0;
                        }
                      xputchar (line_delim);
                    }
                  continue;	 
                }
              else
                {
                   
                  if (*delimptr != EMPTY_DELIM)
                    delbuf[delims_saved++] = *delimptr;
                  if (++delimptr == delim_end)
                    delimptr = delims;
                }
            }
          else
            {
               
              somedone = true;

               
              if (i + 1 != nfiles)
                {
                  if (chr != line_delim && chr != EOF)
                    xputchar (chr);
                  if (*delimptr != EMPTY_DELIM)
                    xputchar (*delimptr);
                  if (++delimptr == delim_end)
                    delimptr = delims;
                }
              else
                {
                   
                  char c = (chr == EOF ? line_delim : chr);
                  xputchar (c);
                }
            }
        }
    }
  free (fileptr);
  free (delbuf);
  return ok;
}

 

static bool
paste_serial (size_t nfiles, char **fnamptr)
{
  bool ok = true;	 
  int charnew, charold;  
  char const *delimptr;	 
  FILE *fileptr;	 

  for (; nfiles; nfiles--, fnamptr++)
    {
      int saved_errno;
      bool is_stdin = STREQ (*fnamptr, "-");
      if (is_stdin)
        {
          have_read_stdin = true;
          fileptr = stdin;
        }
      else
        {
          fileptr = fopen (*fnamptr, "r");
          if (fileptr == nullptr)
            {
              error (0, errno, "%s", quotef (*fnamptr));
              ok = false;
              continue;
            }
          fadvise (fileptr, FADVISE_SEQUENTIAL);
        }

      delimptr = delims;	 

      charold = getc (fileptr);
      saved_errno = errno;
      if (charold != EOF)
        {
           

          while ((charnew = getc (fileptr)) != EOF)
            {
               
              if (charold == line_delim)
                {
                  if (*delimptr != EMPTY_DELIM)
                    xputchar (*delimptr);

                  if (++delimptr == delim_end)
                    delimptr = delims;
                }
              else
                xputchar (charold);

              charold = charnew;
            }
          saved_errno = errno;

           
          xputchar (charold);
        }

      if (charold != line_delim)
        xputchar (line_delim);

      if (!ferror (fileptr))
        saved_errno = 0;
      if (is_stdin)
        clearerr (fileptr);	 
      else if (fclose (fileptr) != 0 && !saved_errno)
        saved_errno = errno;
      if (saved_errno)
        {
          error (0, saved_errno, "%s", quotef (*fnamptr));
          ok = false;
        }
    }
  return ok;
}

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
Write lines consisting of the sequentially corresponding lines from\n\
each FILE, separated by TABs, to standard output.\n\
"), stdout);

      emit_stdin_note ();
      emit_mandatory_arg_note ();

      fputs (_("\
  -d, --delimiters=LIST   reuse characters from LIST instead of TABs\n\
  -s, --serial            paste one file at a time instead of in parallel\n\
"), stdout);
      fputs (_("\
  -z, --zero-terminated    line delimiter is NUL, not newline\n\
"), stdout);
      fputs (HELP_OPTION_DESCRIPTION, stdout);
      fputs (VERSION_OPTION_DESCRIPTION, stdout);
      /* FIXME: add a couple of examples.  */
      emit_ancillary_info (PROGRAM_NAME);
    }
  exit (status);
}

int
main (int argc, char **argv)
{
  int optc;
  char const *delim_arg = "\t";

  initialize_main (&argc, &argv);
  set_program_name (argv[0]);
  setlocale (LC_ALL, "");
  bindtextdomain (PACKAGE, LOCALEDIR);
  textdomain (PACKAGE);

  atexit (close_stdout);

  have_read_stdin = false;
  serial_merge = false;

  while ((optc = getopt_long (argc, argv, "d:sz", longopts, nullptr)) != -1)
    {
      switch (optc)
        {
        case 'd':
          /* Delimiter character(s). */
          delim_arg = (optarg[0] == '\0' ? "\\0" : optarg);
          break;

        case 's':
          serial_merge = true;
          break;

        case 'z':
          line_delim = '\0';
          break;

        case_GETOPT_HELP_CHAR;

        case_GETOPT_VERSION_CHAR (PROGRAM_NAME, AUTHORS);

        default:
          usage (EXIT_FAILURE);
        }
    }

  int nfiles = argc - optind;
  if (nfiles == 0)
    {
      argv[optind] = bad_cast ("-");
      nfiles++;
    }

  if (collapse_escapes (delim_arg))
    {
      /* Don't use the quote() quoting style, because that would double the
         number of displayed backslashes, making the diagnostic look bogus.  */
      error (EXIT_FAILURE, 0,
             _("delimiter list ends with an unescaped backslash: %s"),
             quotearg_n_style_colon (0, c_maybe_quoting_style, delim_arg));
    }

  bool ok = ((serial_merge ? paste_serial : paste_parallel)
             (nfiles, &argv[optind]));

  free (delims);

  if (have_read_stdin && fclose (stdin) == EOF)
    error (EXIT_FAILURE, errno, "-");
  return ok ? EXIT_SUCCESS : EXIT_FAILURE;
}
