 

#include <config.h>
#include <stdio.h>
#include <getopt.h>
#include <sys/types.h>

#include "system.h"
#include "assure.h"
#include "dev-ino.h"
#include "filemode.h"
#include "ignore-value.h"
#include "modechange.h"
#include "quote.h"
#include "root-dev-ino.h"
#include "xfts.h"

 
#define PROGRAM_NAME "chmod"

#define AUTHORS \
  proper_name ("David MacKenzie"), \
  proper_name ("Jim Meyering")

struct change_status
{
  enum
    {
      CH_NO_STAT,
      CH_FAILED,
      CH_NOT_APPLIED,
      CH_NO_CHANGE_REQUESTED,
      CH_SUCCEEDED
    }
    status;
  mode_t old_mode;
  mode_t new_mode;
};

enum Verbosity
{
   
  V_high,

   
  V_changes_only,

   
  V_off
};

 
static struct mode_change *change;

 
static mode_t umask_value;

 
static bool recurse;

 
static bool force_silent;

 
static bool diagnose_surprises;

 
static enum Verbosity verbosity = V_off;

 
static struct dev_ino *root_dev_ino;

 
enum
{
  NO_PRESERVE_ROOT = CHAR_MAX + 1,
  PRESERVE_ROOT,
  REFERENCE_FILE_OPTION
};

static struct option const long_options[] =
{
  {"changes", no_argument, nullptr, 'c'},
  {"recursive", no_argument, nullptr, 'R'},
  {"no-preserve-root", no_argument, nullptr, NO_PRESERVE_ROOT},
  {"preserve-root", no_argument, nullptr, PRESERVE_ROOT},
  {"quiet", no_argument, nullptr, 'f'},
  {"reference", required_argument, nullptr, REFERENCE_FILE_OPTION},
  {"silent", no_argument, nullptr, 'f'},
  {"verbose", no_argument, nullptr, 'v'},
  {GETOPT_HELP_OPTION_DECL},
  {GETOPT_VERSION_OPTION_DECL},
  {nullptr, 0, nullptr, 0}
};

 

static bool
mode_changed (int dir_fd, char const *file, char const *file_full_name,
              mode_t old_mode, mode_t new_mode)
{
  if (new_mode & (S_ISUID | S_ISGID | S_ISVTX))
    {
       

      struct stat new_stats;

      if (fstatat (dir_fd, file, &new_stats, 0) != 0)
        {
          if (! force_silent)
            error (0, errno, _("getting new attributes of %s"),
                   quoteaf (file_full_name));
          return false;
        }

      new_mode = new_stats.st_mode;
    }

  return ((old_mode ^ new_mode) & CHMOD_MODE_BITS) != 0;
}

 

static void
describe_change (char const *file, struct change_status const *ch)
{
  char perms[12];		 
  char old_perms[12];
  char const *fmt;
  char const *quoted_file = quoteaf (file);

  switch (ch->status)
    {
    case CH_NOT_APPLIED:
      printf (_("neither symbolic link %s nor referent has been changed\n"),
              quoted_file);
      return;

    case CH_NO_STAT:
      printf (_("%s could not be accessed\n"), quoted_file);
      return;

    default:
      break;
  }

  unsigned long int
    old_m = ch->old_mode & CHMOD_MODE_BITS,
    m = ch->new_mode & CHMOD_MODE_BITS;

  strmode (ch->new_mode, perms);
  perms[10] = '\0';		 

  strmode (ch->old_mode, old_perms);
  old_perms[10] = '\0';		 

  switch (ch->status)
    {
    case CH_SUCCEEDED:
      fmt = _("mode of %s changed from %04lo (%s) to %04lo (%s)\n");
      break;
    case CH_FAILED:
      fmt = _("failed to change mode of %s from %04lo (%s) to %04lo (%s)\n");
      break;
    case CH_NO_CHANGE_REQUESTED:
      fmt = _("mode of %s retained as %04lo (%s)\n");
      printf (fmt, quoted_file, m, &perms[1]);
      return;
    default:
      affirm (false);
    }
  printf (fmt, quoted_file, old_m, &old_perms[1], m, &perms[1]);
}

 

static bool
process_file (FTS *fts, FTSENT *ent)
{
  char const *file_full_name = ent->fts_path;
  char const *file = ent->fts_accpath;
  const struct stat *file_stats = ent->fts_statp;
  struct change_status ch = { 0, };
  ch.status = CH_NO_STAT;

  switch (ent->fts_info)
    {
    case FTS_DP:
      return true;

    case FTS_NS:
       
      if (ent->fts_level == 0 && ent->fts_number == 0)
        {
          ent->fts_number = 1;
          fts_set (fts, ent, FTS_AGAIN);
          return true;
        }
      if (! force_silent)
        error (0, ent->fts_errno, _("cannot access %s"),
               quoteaf (file_full_name));
      break;

    case FTS_ERR:
      if (! force_silent)
        error (0, ent->fts_errno, "%s", quotef (file_full_name));
      break;

    case FTS_DNR:
      if (! force_silent)
        error (0, ent->fts_errno, _("cannot read directory %s"),
               quoteaf (file_full_name));
      break;

    case FTS_SLNONE:
      if (! force_silent)
        error (0, 0, _("cannot operate on dangling symlink %s"),
               quoteaf (file_full_name));
      break;

    case FTS_DC:		 
      if (cycle_warning_required (fts, ent))
        {
          emit_cycle_warning (file_full_name);
          return false;
        }
      FALLTHROUGH;
    default:
      ch.status = CH_NOT_APPLIED;
      break;
    }

  if (ch.status == CH_NOT_APPLIED
      && ROOT_DEV_INO_CHECK (root_dev_ino, file_stats))
    {
      ROOT_DEV_INO_WARN (file_full_name);
       
      fts_set (fts, ent, FTS_SKIP);
       
      ignore_value (fts_read (fts));
      return false;
    }

  if (ch.status == CH_NOT_APPLIED && ! S_ISLNK (file_stats->st_mode))
    {
      ch.old_mode = file_stats->st_mode;
      ch.new_mode = mode_adjust (ch.old_mode, S_ISDIR (ch.old_mode) != 0,
                                 umask_value, change, nullptr);
      if (chmodat (fts->fts_cwd_fd, file, ch.new_mode) == 0)
        ch.status = CH_SUCCEEDED;
      else
        {
          if (! force_silent)
            error (0, errno, _("changing permissions of %s"),
                   quoteaf (file_full_name));
          ch.status = CH_FAILED;
        }
    }

  if (verbosity != V_off)
    {
      if (ch.status == CH_SUCCEEDED
          && !mode_changed (fts->fts_cwd_fd, file, file_full_name,
                            ch.old_mode, ch.new_mode))
        ch.status = CH_NO_CHANGE_REQUESTED;

      if (ch.status == CH_SUCCEEDED || verbosity == V_high)
        describe_change (file_full_name, &ch);
    }

  if (CH_NO_CHANGE_REQUESTED <= ch.status && diagnose_surprises)
    {
      mode_t naively_expected_mode =
        mode_adjust (ch.old_mode, S_ISDIR (ch.old_mode) != 0,
                     0, change, nullptr);
      if (ch.new_mode & ~naively_expected_mode)
        {
          char new_perms[12];
          char naively_expected_perms[12];
          strmode (ch.new_mode, new_perms);
          strmode (naively_expected_mode, naively_expected_perms);
          new_perms[10] = naively_expected_perms[10] = '\0';
          error (0, 0,
                 _("%s: new permissions are %s, not %s"),
                 quotef (file_full_name),
                 new_perms + 1, naively_expected_perms + 1);
          ch.status = CH_FAILED;
        }
    }

  if ( ! recurse)
    fts_set (fts, ent, FTS_SKIP);

  return CH_NOT_APPLIED <= ch.status;
}

 

static bool
process_files (char **files, int bit_flags)
{
  bool ok = true;

  FTS *fts = xfts_open (files, bit_flags, nullptr);

  while (true)
    {
      FTSENT *ent;

      ent = fts_read (fts);
      if (ent == nullptr)
        {
          if (errno != 0)
            {
               
              if (! force_silent)
                error (0, errno, _("fts_read failed"));
              ok = false;
            }
          break;
        }

      ok &= process_file (fts, ent);
    }

  if (fts_close (fts) != 0)
    {
      error (0, errno, _("fts_close failed"));
      ok = false;
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
Usage: %s [OPTION]... MODE[,MODE]... FILE...\n\
  or:  %s [OPTION]... OCTAL-MODE FILE...\n\
  or:  %s [OPTION]... --reference=RFILE FILE...\n\
"),
              program_name, program_name, program_name);
      fputs (_("\
Change the mode of each FILE to MODE.\n\
With --reference, change the mode of each FILE to that of RFILE.\n\
\n\
"), stdout);
      fputs (_("\
  -c, --changes          like verbose but report only when a change is made\n\
  -f, --silent, --quiet  suppress most error messages\n\
  -v, --verbose          output a diagnostic for every file processed\n\
"), stdout);
      fputs (_("\
      --no-preserve-root  do not treat '/' specially (the default)\n\
      --preserve-root    fail to operate recursively on '/'\n\
"), stdout);
      fputs (_("\
      --reference=RFILE  use RFILE's mode instead of specifying MODE values.\n\
                         RFILE is always dereferenced if a symbolic link.\n\
"), stdout);
      fputs (_("\
  -R, --recursive        change files and directories recursively\n\
"), stdout);
      fputs (HELP_OPTION_DESCRIPTION, stdout);
      fputs (VERSION_OPTION_DESCRIPTION, stdout);
      fputs (_("\
\n\
Each MODE is of the form '[ugoa]*([-+=]([rwxXst]*|[ugo]))+|[-+=][0-7]+'.\n\
"), stdout);
      emit_ancillary_info (PROGRAM_NAME);
    }
  exit (status);
}

/* Parse the ASCII mode given on the command line into a linked list
   of 'struct mode_change' and apply that to each file argument. */

int
main (int argc, char **argv)
{
  char *mode = nullptr;
  idx_t mode_len = 0;
  idx_t mode_alloc = 0;
  bool ok;
  bool preserve_root = false;
  char const *reference_file = nullptr;
  int c;

  initialize_main (&argc, &argv);
  set_program_name (argv[0]);
  setlocale (LC_ALL, "");
  bindtextdomain (PACKAGE, LOCALEDIR);
  textdomain (PACKAGE);

  atexit (close_stdout);

  recurse = force_silent = diagnose_surprises = false;

  while ((c = getopt_long (argc, argv,
                           ("Rcfvr::w::x::X::s::t::u::g::o::a::,::+::=::"
                            "0::1::2::3::4::5::6::7::"),
                           long_options, nullptr))
         != -1)
    {
      switch (c)
        {
        case 'r':
        case 'w':
        case 'x':
        case 'X':
        case 's':
        case 't':
        case 'u':
        case 'g':
        case 'o':
        case 'a':
        case ',':
        case '+':
        case '=':
        case '0': case '1': case '2': case '3':
        case '4': case '5': case '6': case '7':
          /* Support non-portable uses like "chmod -w", but diagnose
             surprises due to umask confusion.  Even though "--", "--r",
             etc., are valid modes, there is no "case '-'" here since
             getopt_long reserves leading "--" for long options.  */
          {
            /* Allocate a mode string (e.g., "-rwx") by concatenating
               the argument containing this option.  If a previous mode
               string was given, concatenate the previous string, a
               comma, and the new string (e.g., "-s,-rwx").  */

            char const *arg = argv[optind - 1];
            idx_t arg_len = strlen (arg);
            idx_t mode_comma_len = mode_len + !!mode_len;
            idx_t new_mode_len = mode_comma_len + arg_len;
            assume (0 <= new_mode_len);  /* Pacify GCC bug #109613.  */
            if (mode_alloc <= new_mode_len)
              mode = xpalloc (mode, &mode_alloc,
                              new_mode_len + 1 - mode_alloc, -1, 1);
            mode[mode_len] = ',';
            memcpy (mode + mode_comma_len, arg, arg_len + 1);
            mode_len = new_mode_len;

            diagnose_surprises = true;
          }
          break;
        case NO_PRESERVE_ROOT:
          preserve_root = false;
          break;
        case PRESERVE_ROOT:
          preserve_root = true;
          break;
        case REFERENCE_FILE_OPTION:
          reference_file = optarg;
          break;
        case 'R':
          recurse = true;
          break;
        case 'c':
          verbosity = V_changes_only;
          break;
        case 'f':
          force_silent = true;
          break;
        case 'v':
          verbosity = V_high;
          break;
        case_GETOPT_HELP_CHAR;
        case_GETOPT_VERSION_CHAR (PROGRAM_NAME, AUTHORS);
        default:
          usage (EXIT_FAILURE);
        }
    }

  if (reference_file)
    {
      if (mode)
        {
          error (0, 0, _("cannot combine mode and --reference options"));
          usage (EXIT_FAILURE);
        }
    }
  else
    {
      if (!mode)
        mode = argv[optind++];
    }

  if (optind >= argc)
    {
      if (!mode || mode != argv[optind - 1])
        error (0, 0, _("missing operand"));
      else
        error (0, 0, _("missing operand after %s"), quote (argv[argc - 1]));
      usage (EXIT_FAILURE);
    }

  if (reference_file)
    {
      change = mode_create_from_ref (reference_file);
      if (!change)
        error (EXIT_FAILURE, errno, _("failed to get attributes of %s"),
               quoteaf (reference_file));
    }
  else
    {
      change = mode_compile (mode);
      if (!change)
        {
          error (0, 0, _("invalid mode: %s"), quote (mode));
          usage (EXIT_FAILURE);
        }
      umask_value = umask (0);
    }

  if (recurse && preserve_root)
    {
      static struct dev_ino dev_ino_buf;
      root_dev_ino = get_root_dev_ino (&dev_ino_buf);
      if (root_dev_ino == nullptr)
        error (EXIT_FAILURE, errno, _("failed to get attributes of %s"),
               quoteaf ("/"));
    }
  else
    {
      root_dev_ino = nullptr;
    }

  ok = process_files (argv + optind,
                      FTS_COMFOLLOW | FTS_PHYSICAL | FTS_DEFER_STAT);

  main_exit (ok ? EXIT_SUCCESS : EXIT_FAILURE);
}
