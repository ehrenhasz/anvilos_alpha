 

#include <config.h>
#include <stdio.h>
#include <sys/types.h>
#include <pwd.h>
#include <grp.h>
#include <getopt.h>
#include <selinux/selinux.h>

#include "system.h"
#include "mgetgroups.h"
#include "quote.h"
#include "group-list.h"
#include "smack.h"
#include "userspec.h"

 
#define PROGRAM_NAME "id"

#define AUTHORS \
  proper_name ("Arnold Robbins"), \
  proper_name ("David MacKenzie")

 
static bool just_context = 0;
 
static bool opt_zero = false;
 
static bool just_group_list = false;
 
static bool just_group = false;
 
static bool use_real = false;
 
static bool just_user = false;
 
static bool ok = true;
 
static bool multiple_users = false;
 
static bool use_name = false;

 
static uid_t ruid, euid;
static gid_t rgid, egid;

 
static char *context = nullptr;

static void print_user (uid_t uid);
static void print_full_info (char const *username);
static void print_stuff (char const *pw_name);

static struct option const longopts[] =
{
  {"context", no_argument, nullptr, 'Z'},
  {"group", no_argument, nullptr, 'g'},
  {"groups", no_argument, nullptr, 'G'},
  {"name", no_argument, nullptr, 'n'},
  {"real", no_argument, nullptr, 'r'},
  {"user", no_argument, nullptr, 'u'},
  {"zero", no_argument, nullptr, 'z'},
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
      printf (_("Usage: %s [OPTION]... [USER]...\n"), program_name);
      fputs (_("\
Print user and group information for each specified USER,\n\
or (when USER omitted) for the current process.\n\
\n"),
             stdout);
      fputs (_("\
  -a             ignore, for compatibility with other versions\n\
  -Z, --context  print only the security context of the process\n\
  -g, --group    print only the effective group ID\n\
  -G, --groups   print all group IDs\n\
  -n, --name     print a name instead of a number, for -ugG\n\
  -r, --real     print the real ID instead of the effective ID, with -ugG\n\
  -u, --user     print only the effective user ID\n\
  -z, --zero     delimit entries with NUL characters, not whitespace;\n\
                   not permitted in default format\n\
"), stdout);
      fputs (HELP_OPTION_DESCRIPTION, stdout);
      fputs (VERSION_OPTION_DESCRIPTION, stdout);
      fputs (_("\
\n\
Without any OPTION, print some useful set of identified information.\n\
"), stdout);
      emit_ancillary_info (PROGRAM_NAME);
    }
  exit (status);
}

int
main (int argc, char **argv)
{
  int optc;
  int selinux_enabled = (is_selinux_enabled () > 0);
  bool smack_enabled = is_smack_enabled ();

  initialize_main (&argc, &argv);
  set_program_name (argv[0]);
  setlocale (LC_ALL, "");
  bindtextdomain (PACKAGE, LOCALEDIR);
  textdomain (PACKAGE);

  atexit (close_stdout);

  while ((optc = getopt_long (argc, argv, "agnruzGZ", longopts, nullptr)) != -1)
    {
      switch (optc)
        {
        case 'a':
          /* Ignore -a, for compatibility with SVR4.  */
          break;

        case 'Z':
          /* politely decline if we're not on a SELinux/SMACK-enabled kernel. */
#ifdef HAVE_SMACK
          if (!selinux_enabled && !smack_enabled)
            error (EXIT_FAILURE, 0,
                   _("--context (-Z) works only on "
                     "an SELinux/SMACK-enabled kernel"));
#else
          if (!selinux_enabled)
            error (EXIT_FAILURE, 0,
                   _("--context (-Z) works only on an SELinux-enabled kernel"));
#endif
          just_context = true;
          break;

        case 'g':
          just_group = true;
          break;
        case 'n':
          use_name = true;
          break;
        case 'r':
          use_real = true;
          break;
        case 'u':
          just_user = true;
          break;
        case 'z':
          opt_zero = true;
          break;
        case 'G':
          just_group_list = true;
          break;
        case_GETOPT_HELP_CHAR;
        case_GETOPT_VERSION_CHAR (PROGRAM_NAME, AUTHORS);
        default:
          usage (EXIT_FAILURE);
        }
    }

  size_t n_ids = argc - optind;

  if (n_ids && just_context)
    error (EXIT_FAILURE, 0,
           _("cannot print security context when user specified"));

  if (just_user + just_group + just_group_list + just_context > 1)
    error (EXIT_FAILURE, 0, _("cannot print \"only\" of more than one choice"));

  bool default_format = ! (just_user
                           || just_group
                           || just_group_list
                           || just_context);

  if (default_format && (use_real || use_name))
    error (EXIT_FAILURE, 0,
           _("cannot print only names or real IDs in default format"));

  if (default_format && opt_zero)
    error (EXIT_FAILURE, 0,
           _("option --zero not permitted in default format"));

   
  if (n_ids == 0
      && (just_context
          || (default_format && ! getenv ("POSIXLY_CORRECT"))))
    {
       
      if ((selinux_enabled && getcon (&context) && just_context)
          || (smack_enabled
              && smack_new_label_from_self (&context) < 0
              && just_context))
        error (EXIT_FAILURE, 0, _("can't get process context"));
    }

  if (n_ids >= 1)
    {
      multiple_users = n_ids > 1 ? true : false;
       
      n_ids += optind;
       
      for (; optind < n_ids; optind++)
        {
          char *pw_name = nullptr;
          struct passwd *pwd = nullptr;
          char const *spec = argv[optind];
           
          if (*spec)
            {
              if (! parse_user_spec (spec, &euid, nullptr, &pw_name, nullptr))
                pwd = pw_name ? getpwnam (pw_name) : getpwuid (euid);
            }
          if (pwd == nullptr)
            {
              error (0, errno, _("%s: no such user"), quote (spec));
              ok &= false;
            }
          else
            {
              if (!pw_name)
                pw_name = xstrdup (pwd->pw_name);
              ruid = euid = pwd->pw_uid;
              rgid = egid = pwd->pw_gid;
              print_stuff (pw_name);
            }
          free (pw_name);
        }
    }
  else
    {
       
      uid_t NO_UID = -1;
      gid_t NO_GID = -1;

      if (just_user ? !use_real
          : !just_group && !just_group_list && !just_context)
        {
          errno = 0;
          euid = geteuid ();
          if (euid == NO_UID && errno)
            error (EXIT_FAILURE, errno, _("cannot get effective UID"));
        }

      if (just_user ? use_real
          : !just_group && (just_group_list || !just_context))
        {
          errno = 0;
          ruid = getuid ();
          if (ruid == NO_UID && errno)
            error (EXIT_FAILURE, errno, _("cannot get real UID"));
        }

      if (!just_user && (just_group || just_group_list || !just_context))
        {
          errno = 0;
          egid = getegid ();
          if (egid == NO_GID && errno)
            error (EXIT_FAILURE, errno, _("cannot get effective GID"));

          errno = 0;
          rgid = getgid ();
          if (rgid == NO_GID && errno)
            error (EXIT_FAILURE, errno, _("cannot get real GID"));
        }
        print_stuff (nullptr);
    }

  return ok ? EXIT_SUCCESS : EXIT_FAILURE;
}

 
static char *
gidtostr_ptr (gid_t const *gid)
{
  static char buf[INT_BUFSIZE_BOUND (uintmax_t)];
  return umaxtostr (*gid, buf);
}
#define gidtostr(g) gidtostr_ptr (&(g))

 
static char *
uidtostr_ptr (uid_t const *uid)
{
  static char buf[INT_BUFSIZE_BOUND (uintmax_t)];
  return umaxtostr (*uid, buf);
}
#define uidtostr(u) uidtostr_ptr (&(u))

 

static void
print_user (uid_t uid)
{
  struct passwd *pwd = nullptr;

  if (use_name)
    {
      pwd = getpwuid (uid);
      if (pwd == nullptr)
        {
          error (0, 0, _("cannot find name for user ID %s"),
                 uidtostr (uid));
          ok &= false;
        }
    }

  char *s = pwd ? pwd->pw_name : uidtostr (uid);
  fputs (s, stdout);
}

 

static void
print_full_info (char const *username)
{
  struct passwd *pwd;
  struct group *grp;

  printf (_("uid=%s"), uidtostr (ruid));
  pwd = getpwuid (ruid);
  if (pwd)
    printf ("(%s)", pwd->pw_name);

  printf (_(" gid=%s"), gidtostr (rgid));
  grp = getgrgid (rgid);
  if (grp)
    printf ("(%s)", grp->gr_name);

  if (euid != ruid)
    {
      printf (_(" euid=%s"), uidtostr (euid));
      pwd = getpwuid (euid);
      if (pwd)
        printf ("(%s)", pwd->pw_name);
    }

  if (egid != rgid)
    {
      printf (_(" egid=%s"), gidtostr (egid));
      grp = getgrgid (egid);
      if (grp)
        printf ("(%s)", grp->gr_name);
    }

  {
    gid_t *groups;

    gid_t primary_group;
    if (username)
      primary_group = pwd ? pwd->pw_gid : -1;
    else
      primary_group = egid;

    int n_groups = xgetgroups (username, primary_group, &groups);
    if (n_groups < 0)
      {
        if (username)
          error (0, errno, _("failed to get groups for user %s"),
                 quote (username));
        else
          error (0, errno, _("failed to get groups for the current process"));
        ok &= false;
        return;
      }

    if (n_groups > 0)
      fputs (_(" groups="), stdout);
    for (int i = 0; i < n_groups; i++)
      {
        if (i > 0)
          putchar (',');
        fputs (gidtostr (groups[i]), stdout);
        grp = getgrgid (groups[i]);
        if (grp)
          printf ("(%s)", grp->gr_name);
      }
    free (groups);
  }

   
  if (context)
    printf (_(" context=%s"), context);
}

 

static void
print_stuff (char const *pw_name)
{
  if (just_user)
      print_user (use_real ? ruid : euid);

   
  else if (just_group)
    ok &= print_group (use_real ? rgid : egid, use_name);
  else if (just_group_list)
    ok &= print_group_list (pw_name, ruid, rgid, egid,
                            use_name, opt_zero ? '\0' : ' ');
  else if (just_context)
    fputs (context, stdout);
  else
    print_full_info (pw_name);

   
  if (opt_zero && just_group_list && multiple_users)
    {
      putchar ('\0');
      putchar ('\0');
    }
  else
    {
      putchar (opt_zero ? '\0' : '\n');
    }
}
