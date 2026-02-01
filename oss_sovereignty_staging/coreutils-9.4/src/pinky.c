 

 

#include <config.h>
#include <getopt.h>
#include <pwd.h>
#include <stdckdint.h>
#include <stdio.h>

#include <sys/types.h>
#include "system.h"

#include "canon-host.h"
#include "hard-locale.h"
#include "readutmp.h"

 
#define PROGRAM_NAME "pinky"

#define AUTHORS \
  proper_name ("Joseph Arceneaux"), \
  proper_name ("David MacKenzie"), \
  proper_name ("Kaveh Ghazi")

 
static bool include_idle = true;

 
static bool include_heading = true;

 
static bool include_fullname = true;

 
static bool include_project = true;

 
static bool include_plan = true;

 
static bool include_home_and_shell = true;

 
static bool do_short_format = true;

 
#if HAVE_STRUCT_XTMP_UT_HOST
static bool include_where = true;
#endif

 
static char const *time_format;
static int time_format_width;

static struct option const longopts[] =
{
  {GETOPT_HELP_OPTION_DECL},
  {GETOPT_VERSION_OPTION_DECL},
  {nullptr, 0, nullptr, 0}
};

 

ATTRIBUTE_PURE
static size_t
count_ampersands (char const *str)
{
  size_t count = 0;
  do
    {
      if (*str == '&')
        count++;
    } while (*str++);
  return count;
}

 

static char *
create_fullname (char const *gecos_name, char const *user_name)
{
  size_t rsize = strlen (gecos_name) + 1;
  char *result;
  char *r;
  size_t ampersands = count_ampersands (gecos_name);

  if (ampersands != 0)
    {
      size_t ulen = strlen (user_name);
      size_t product;
      if (ckd_mul (&product, ulen, ampersands - 1)
          || ckd_add (&rsize, rsize, product))
        xalloc_die ();
    }

  r = result = xmalloc (rsize);

  while (*gecos_name)
    {
      if (*gecos_name == '&')
        {
          char const *uname = user_name;
          if (islower (to_uchar (*uname)))
            *r++ = toupper (to_uchar (*uname++));
          while (*uname)
            *r++ = *uname++;
        }
      else
        {
          *r++ = *gecos_name;
        }

      gecos_name++;
    }
  *r = 0;

  return result;
}

 

static char const *
idle_string (time_t when)
{
  static time_t now = 0;
  static char buf[INT_STRLEN_BOUND (intmax_t) + sizeof "d"];
  time_t seconds_idle;

  if (now == 0)
    time (&now);

  seconds_idle = now - when;
  if (seconds_idle < 60)	 
    return "     ";
  if (seconds_idle < (24 * 60 * 60))	 
    {
      int hours = seconds_idle / (60 * 60);
      int minutes = (seconds_idle % (60 * 60)) / 60;
      sprintf (buf, "%02d:%02d", hours, minutes);
    }
  else
    {
      intmax_t days = seconds_idle / (24 * 60 * 60);
      sprintf (buf, "%"PRIdMAX"d", days);
    }
  return buf;
}

 
static char const *
time_string (struct gl_utmp const *utmp_ent)
{
  static char buf[INT_STRLEN_BOUND (intmax_t) + sizeof "-%m-%d %H:%M"];
  struct tm *tmp = localtime (&utmp_ent->ut_ts.tv_sec);

  if (tmp)
    {
      strftime (buf, sizeof buf, time_format, tmp);
      return buf;
    }
  else
    return timetostr (utmp_ent->ut_ts.tv_sec, buf);
}

 

static void
print_entry (struct gl_utmp const *utmp_ent)
{
  struct stat stats;
  time_t last_change;
  char mesg;

   
  char *line = utmp_ent->ut_line;
  char *space = strchr (line, ' ');
  line = space ? space + 1 : line;

  int dirfd;
  if (IS_ABSOLUTE_FILE_NAME (line))
    dirfd = AT_FDCWD;
  else
    {
      static int dev_dirfd;
      if (!dev_dirfd)
        {
          dev_dirfd = open ("/dev", O_PATHSEARCH | O_DIRECTORY);
          if (dev_dirfd < 0)
            dev_dirfd = AT_FDCWD - 1;
        }
      dirfd = dev_dirfd;
    }

  if (AT_FDCWD <= dirfd && fstatat (dirfd, line, &stats, 0) == 0)
    {
      mesg = (stats.st_mode & S_IWGRP) ? ' ' : '*';
      last_change = stats.st_atime;
    }
  else
    {
      mesg = '?';
      last_change = 0;
    }

  char *ut_user = utmp_ent->ut_user;
  if (strnlen (ut_user, 8) < 8)
    printf ("%-8s", ut_user);
  else
    fputs (ut_user, stdout);

  if (include_fullname)
    {
      struct passwd *pw = getpwnam (ut_user);
      if (pw == nullptr)
         
        printf (" %19s", _("        ???"));
      else
        {
          char *const comma = strchr (pw->pw_gecos, ',');
          char *result;

          if (comma)
            *comma = '\0';

          result = create_fullname (pw->pw_gecos, pw->pw_name);
          printf (" %-19.19s", result);
          free (result);
        }
    }

  fputc (' ', stdout);
  fputc (mesg, stdout);
  if (strnlen (utmp_ent->ut_line, 8) < 8)
    printf ("%-8s", utmp_ent->ut_line);
  else
    fputs (utmp_ent->ut_line, stdout);

  if (include_idle)
    {
      if (last_change)
        printf (" %-6s", idle_string (last_change));
      else
         
        printf (" %-6s", _("?????"));
    }

  printf (" %s", time_string (utmp_ent));

#ifdef HAVE_STRUCT_XTMP_UT_HOST
  if (include_where && utmp_ent->ut_host[0])
    {
      char *host = nullptr;
      char *display = nullptr;
      char *ut_host = utmp_ent->ut_host;

       
      display = strchr (ut_host, ':');
      if (display)
        *display++ = '\0';

      if (*ut_host)
         
        host = canon_host (ut_host);
      if ( ! host)
        host = ut_host;

      fputc (' ', stdout);
      fputs (host, stdout);
      if (display)
        {
          fputc (':', stdout);
          fputs (display, stdout);
        }

      if (host != ut_host)
        free (host);
    }
#endif

  putchar ('\n');
}

 

static void
print_long_entry (const char name[])
{
  struct passwd *pw;

  pw = getpwnam (name);

  printf (_("Login name: "));
  printf ("%-28s", name);

  printf (_("In real life: "));
  if (pw == nullptr)
    {
       
      printf (" %s", _("???\n"));
      return;
    }
  else
    {
      char *const comma = strchr (pw->pw_gecos, ',');
      char *result;

      if (comma)
        *comma = '\0';

      result = create_fullname (pw->pw_gecos, pw->pw_name);
      printf (" %s", result);
      free (result);
    }

  putchar ('\n');

  if (include_home_and_shell)
    {
      printf (_("Directory: "));
      printf ("%-29s", pw->pw_dir);
      printf (_("Shell: "));
      printf (" %s", pw->pw_shell);
      putchar ('\n');
    }

  if (include_project)
    {
      FILE *stream;
      char buf[1024];
      char const *const baseproject = "/.project";
      char *const project =
        xmalloc (strlen (pw->pw_dir) + strlen (baseproject) + 1);
      stpcpy (stpcpy (project, pw->pw_dir), baseproject);

      stream = fopen (project, "r");
      if (stream)
        {
          size_t bytes;

          printf (_("Project: "));

          while ((bytes = fread (buf, 1, sizeof (buf), stream)) > 0)
            fwrite (buf, 1, bytes, stdout);
          fclose (stream);
        }

      free (project);
    }

  if (include_plan)
    {
      FILE *stream;
      char buf[1024];
      char const *const baseplan = "/.plan";
      char *const plan =
        xmalloc (strlen (pw->pw_dir) + strlen (baseplan) + 1);
      stpcpy (stpcpy (plan, pw->pw_dir), baseplan);

      stream = fopen (plan, "r");
      if (stream)
        {
          size_t bytes;

          printf (_("Plan:\n"));

          while ((bytes = fread (buf, 1, sizeof (buf), stream)) > 0)
            fwrite (buf, 1, bytes, stdout);
          fclose (stream);
        }

      free (plan);
    }

  putchar ('\n');
}

 

static void
print_heading (void)
{
  printf ("%-8s", _("Login"));
  if (include_fullname)
    printf (" %-19s", _("Name"));
  printf (" %-9s", _(" TTY"));
  if (include_idle)
    printf (" %-6s", _("Idle"));
  printf (" %-*s", time_format_width, _("When"));
#ifdef HAVE_STRUCT_XTMP_UT_HOST
  if (include_where)
    printf (" %s", _("Where"));
#endif
  putchar ('\n');
}

 

static void
scan_entries (idx_t n, struct gl_utmp const *utmp_buf,
              const int argc_names, char *const argv_names[])
{
  if (hard_locale (LC_TIME))
    {
      time_format = "%Y-%m-%d %H:%M";
      time_format_width = 4 + 1 + 2 + 1 + 2 + 1 + 2 + 1 + 2;
    }
  else
    {
      time_format = "%b %e %H:%M";
      time_format_width = 3 + 1 + 2 + 1 + 2 + 1 + 2;
    }

  if (include_heading)
    print_heading ();

  while (n--)
    {
      if (IS_USER_PROCESS (utmp_buf))
        {
          if (argc_names)
            {
              for (int i = 0; i < argc_names; i++)
                if (STREQ (utmp_buf->ut_user, argv_names[i]))
                  {
                    print_entry (utmp_buf);
                    break;
                  }
            }
          else
            print_entry (utmp_buf);
        }
      utmp_buf++;
    }
}

 

static void
short_pinky (char const *filename,
             const int argc_names, char *const argv_names[])
{
  idx_t n_users;
  struct gl_utmp *utmp_buf;
  if (read_utmp (filename, &n_users, &utmp_buf, READ_UTMP_USER_PROCESS) != 0)
    error (EXIT_FAILURE, errno, "%s", quotef (filename));

  scan_entries (n_users, utmp_buf, argc_names, argv_names);
  exit (EXIT_SUCCESS);
}

static void
long_pinky (const int argc_names, char *const argv_names[])
{
  for (int i = 0; i < argc_names; i++)
    print_long_entry (argv_names[i]);
}

void
usage (int status)
{
  if (status != EXIT_SUCCESS)
    emit_try_help ();
  else
    {
      printf (_("Usage: %s [OPTION]... [USER]...\n"), program_name);
      fputs (_("\
\n\
  -l              produce long format output for the specified USERs\n\
  -b              omit the user's home directory and shell in long format\n\
  -h              omit the user's project file in long format\n\
  -p              omit the user's plan file in long format\n\
  -s              do short format output, this is the default\n\
"), stdout);
      fputs (_("\
  -f              omit the line of column headings in short format\n\
  -w              omit the user's full name in short format\n\
  -i              omit the user's full name and remote host in short format\n\
  -q              omit the user's full name, remote host and idle time\n\
                  in short format\n\
"), stdout);
      fputs (HELP_OPTION_DESCRIPTION, stdout);
      fputs (VERSION_OPTION_DESCRIPTION, stdout);
      printf (_("\
\n\
A lightweight 'finger' program;  print user information.\n\
The utmp file will be %s.\n\
"), UTMP_FILE);
      emit_ancillary_info (PROGRAM_NAME);
    }
  exit (status);
}

int
main (int argc, char **argv)
{
  int optc;
  int n_users;

  initialize_main (&argc, &argv);
  set_program_name (argv[0]);
  setlocale (LC_ALL, "");
  bindtextdomain (PACKAGE, LOCALEDIR);
  textdomain (PACKAGE);

  atexit (close_stdout);

  while ((optc = getopt_long (argc, argv, "sfwiqbhlp", longopts, nullptr))
         != -1)
    {
      switch (optc)
        {
        case 's':
          do_short_format = true;
          break;

        case 'l':
          do_short_format = false;
          break;

        case 'f':
          include_heading = false;
          break;

        case 'w':
          include_fullname = false;
          break;

        case 'i':
          include_fullname = false;
#ifdef HAVE_STRUCT_XTMP_UT_HOST
          include_where = false;
#endif
          break;

        case 'q':
          include_fullname = false;
#ifdef HAVE_STRUCT_XTMP_UT_HOST
          include_where = false;
#endif
          include_idle = false;
          break;

        case 'h':
          include_project = false;
          break;

        case 'p':
          include_plan = false;
          break;

        case 'b':
          include_home_and_shell = false;
          break;

        case_GETOPT_HELP_CHAR;

        case_GETOPT_VERSION_CHAR (PROGRAM_NAME, AUTHORS);

        default:
          usage (EXIT_FAILURE);
        }
    }

  n_users = argc - optind;

  if (!do_short_format && n_users == 0)
    {
      error (0, 0, _("no username specified; at least one must be\
 specified when using -l"));
      usage (EXIT_FAILURE);
    }

  if (do_short_format)
    short_pinky (UTMP_FILE, n_users, argv + optind);
  else
    long_pinky (n_users, argv + optind);

  return EXIT_SUCCESS;
}
