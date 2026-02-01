 

#include <config.h>

#include <getopt.h>
#include <pthread.h>
#include <sys/resource.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <signal.h>
#include "system.h"
#include "argmatch.h"
#include "assure.h"
#include "fadvise.h"
#include "filevercmp.h"
#include "flexmember.h"
#include "hard-locale.h"
#include "hash.h"
#include "heap.h"
#include "ignore-value.h"
#include "md5.h"
#include "mbswidth.h"
#include "nproc.h"
#include "physmem.h"
#include "posixver.h"
#include "quote.h"
#include "randread.h"
#include "readtokens0.h"
#include "stdlib--.h"
#include "strnumcmp.h"
#include "xmemcoll.h"
#include "xnanosleep.h"
#include "xstrtol.h"
#include "xstrtol-error.h"

#ifndef RLIMIT_DATA
struct rlimit { size_t rlim_cur; };
# define getrlimit(Resource, Rlp) (-1)
#endif

 
#define PROGRAM_NAME "sort"

#define AUTHORS \
  proper_name ("Mike Haertel"), \
  proper_name ("Paul Eggert")

#if HAVE_LANGINFO_CODESET
# include <langinfo.h>
#endif

 
#ifndef SA_NOCLDSTOP
# define SA_NOCLDSTOP 0
 
# define sigprocmask(How, Set, Oset) (0)
# define sigset_t int
# if ! HAVE_SIGINTERRUPT
#  define siginterrupt(sig, flag)  
# endif
#endif

#if !defined OPEN_MAX && defined NR_OPEN
# define OPEN_MAX NR_OPEN
#endif
#if !defined OPEN_MAX
# define OPEN_MAX 20
#endif

#define UCHAR_LIM (UCHAR_MAX + 1)

#ifndef DEFAULT_TMPDIR
# define DEFAULT_TMPDIR "/tmp"
#endif

 
#define MAX_MERGE(total, level) (((total) >> (2 * ((level) + 1))) + 1)

 
enum { SUBTHREAD_LINES_HEURISTIC = 128 * 1024 };
static_assert (4 <= SUBTHREAD_LINES_HEURISTIC);

 
enum { DEFAULT_MAX_THREADS = 8 };

 
enum
  {
     
    SORT_OUT_OF_ORDER = 1,

     
    SORT_FAILURE = 2
  };

enum
  {
     
    MAX_FORK_TRIES_COMPRESS = 4,

     
    MAX_FORK_TRIES_DECOMPRESS = 9
  };

enum
  {
     
    MERGE_END = 0,

     
    MERGE_ROOT = 1
  };

 
static char decimal_point;

 
static int thousands_sep;
 
static bool thousands_sep_ignored;

 
static bool hard_LC_COLLATE;
#if HAVE_NL_LANGINFO
static bool hard_LC_TIME;
#endif

#define NONZERO(x) ((x) != 0)

 
enum blanktype { bl_start, bl_end, bl_both };

 
static char eolchar = '\n';

 
struct line
{
  char *text;			 
  size_t length;		 
  char *keybeg;			 
  char *keylim;			 
};

 
struct buffer
{
  char *buf;			 
  size_t used;			 
  size_t nlines;		 
  size_t alloc;			 
  size_t left;			 
  size_t line_bytes;		 
  bool eof;			 
};

 
struct keyfield
{
  size_t sword;			 
  size_t schar;			 
  size_t eword;			 
  size_t echar;			 
  bool const *ignore;		 
  char const *translate;	 
  bool skipsblanks;		 
  bool skipeblanks;		 
  bool numeric;			 
  bool random;			 
  bool general_numeric;		 
  bool human_numeric;		 
  bool month;			 
  bool reverse;			 
  bool version;			 
  bool traditional_used;	 
  struct keyfield *next;	 
};

struct month
{
  char const *name;
  int val;
};

 
struct merge_node
{
  struct line *lo;               
  struct line *hi;               
  struct line *end_lo;           
  struct line *end_hi;           
  struct line **dest;            
  size_t nlo;                    
  size_t nhi;                    
  struct merge_node *parent;     
  struct merge_node *lo_child;   
  struct merge_node *hi_child;   
  unsigned int level;            
  bool queued;                   
  pthread_mutex_t lock;          
};

 
struct merge_node_queue
{
  struct heap *priority_queue;   
  pthread_mutex_t mutex;         
  pthread_cond_t cond;           
};

 
static struct line saved_line;

 

 
static bool blanks[UCHAR_LIM];

 
static bool nonprinting[UCHAR_LIM];

 
static bool nondictionary[UCHAR_LIM];

 
static char fold_toupper[UCHAR_LIM];

#define MONTHS_PER_YEAR 12

 
static struct month monthtab[] =
{
  {"APR", 4},
  {"AUG", 8},
  {"DEC", 12},
  {"FEB", 2},
  {"JAN", 1},
  {"JUL", 7},
  {"JUN", 6},
  {"MAR", 3},
  {"MAY", 5},
  {"NOV", 11},
  {"OCT", 10},
  {"SEP", 9}
};

 
#define NMERGE_DEFAULT 16

 
#define MIN_MERGE_BUFFER_SIZE (2 + sizeof (struct line))

 
#define MIN_SORT_SIZE (nmerge * MIN_MERGE_BUFFER_SIZE)

 
static size_t merge_buffer_size = MAX (MIN_MERGE_BUFFER_SIZE, 256 * 1024);

 
static size_t sort_size;

 
#define INPUT_FILE_SIZE_GUESS (128 * 1024)

 
static char const **temp_dirs;

 
static size_t temp_dir_count;

 
static size_t temp_dir_alloc;

 
static bool reverse;

 
static bool stable;

 
enum { NON_CHAR = CHAR_MAX + 1 };

 
enum { TAB_DEFAULT = CHAR_MAX + 1 };

 
static int tab = TAB_DEFAULT;

 
static bool unique;

 
static bool have_read_stdin;

 
static struct keyfield *keylist;

 
static char const *compress_program;

 
static bool debug;

 
static unsigned int nmerge = NMERGE_DEFAULT;

 

static _Noreturn void
async_safe_die (int errnum, char const *errstr)
{
  ignore_value (write (STDERR_FILENO, errstr, strlen (errstr)));

   
  if (errnum)
    {
      char errbuf[INT_BUFSIZE_BOUND (errnum)];
      char *p = inttostr (errnum, errbuf);
      ignore_value (write (STDERR_FILENO, ": errno ", 8));
      ignore_value (write (STDERR_FILENO, p, strlen (p)));
    }

  ignore_value (write (STDERR_FILENO, "\n", 1));

  _exit (SORT_FAILURE);
}

 

static void
sort_die (char const *message, char const *file)
{
  error (SORT_FAILURE, errno, "%s: %s", message,
         quotef (file ? file : _("standard output")));
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
  or:  %s [OPTION]... --files0-from=F\n\
"),
              program_name, program_name);
      fputs (_("\
Write sorted concatenation of all FILE(s) to standard output.\n\
"), stdout);

      emit_stdin_note ();
      emit_mandatory_arg_note ();

      fputs (_("\
Ordering options:\n\
\n\
"), stdout);
      fputs (_("\
  -b, --ignore-leading-blanks  ignore leading blanks\n\
  -d, --dictionary-order      consider only blanks and alphanumeric characters\
\n\
  -f, --ignore-case           fold lower case to upper case characters\n\
"), stdout);
      fputs (_("\
  -g, --general-numeric-sort  compare according to general numerical value\n\
  -i, --ignore-nonprinting    consider only printable characters\n\
  -M, --month-sort            compare (unknown) < 'JAN' < ... < 'DEC'\n\
"), stdout);
      fputs (_("\
  -h, --human-numeric-sort    compare human readable numbers (e.g., 2K 1G)\n\
"), stdout);
      fputs (_("\
  -n, --numeric-sort          compare according to string numerical value\n\
  -R, --random-sort           shuffle, but group identical keys.  See shuf(1)\n\
      --random-source=FILE    get random bytes from FILE\n\
  -r, --reverse               reverse the result of comparisons\n\
"), stdout);
      fputs (_("\
      --sort=WORD             sort according to WORD:\n\
                                general-numeric -g, human-numeric -h, month -M,\
\n\
                                numeric -n, random -R, version -V\n\
  -V, --version-sort          natural sort of (version) numbers within text\n\
\n\
"), stdout);
      fputs (_("\
Other options:\n\
\n\
"), stdout);
      fputs (_("\
      --batch-size=NMERGE   merge at most NMERGE inputs at once;\n\
                            for more use temp files\n\
"), stdout);
      fputs (_("\
  -c, --check, --check=diagnose-first  check for sorted input; do not sort\n\
  -C, --check=quiet, --check=silent  like -c, but do not report first bad line\
\n\
      --compress-program=PROG  compress temporaries with PROG;\n\
                              decompress them with PROG -d\n\
"), stdout);
      fputs (_("\
      --debug               annotate the part of the line used to sort,\n\
                              and warn about questionable usage to stderr\n\
      --files0-from=F       read input from the files specified by\n\
                            NUL-terminated names in file F;\n\
                            If F is - then read names from standard input\n\
"), stdout);
      fputs (_("\
  -k, --key=KEYDEF          sort via a key; KEYDEF gives location and type\n\
  -m, --merge               merge already sorted files; do not sort\n\
"), stdout);
      fputs (_("\
  -o, --output=FILE         write result to FILE instead of standard output\n\
  -s, --stable              stabilize sort by disabling last-resort comparison\
\n\
  -S, --buffer-size=SIZE    use SIZE for main memory buffer\n\
"), stdout);
      printf (_("\
  -t, --field-separator=SEP  use SEP instead of non-blank to blank transition\n\
  -T, --temporary-directory=DIR  use DIR for temporaries, not $TMPDIR or %s;\n\
                              multiple options specify multiple directories\n\
      --parallel=N          change the number of sorts run concurrently to N\n\
  -u, --unique              with -c, check for strict ordering;\n\
                              without -c, output only the first of an equal run\
\n\
"), DEFAULT_TMPDIR);
      fputs (_("\
  -z, --zero-terminated     line delimiter is NUL, not newline\n\
"), stdout);
      fputs (HELP_OPTION_DESCRIPTION, stdout);
      fputs (VERSION_OPTION_DESCRIPTION, stdout);
      fputs (_("\
\n\
KEYDEF is F[.C][OPTS][,F[.C][OPTS]] for start and stop position, where F is a\n\
field number and C a character position in the field; both are origin 1, and\n\
the stop position defaults to the line's end.  If neither -t nor -b is in\n\
effect, characters in a field are counted from the beginning of the preceding\n\
whitespace.  OPTS is one or more single-letter ordering options [bdfgiMhnRrV],\
\n\
which override global ordering options for that key.  If no key is given, use\n\
the entire line as the key.  Use --debug to diagnose incorrect key usage.\n\
\n\
SIZE may be followed by the following multiplicative suffixes:\n\
"), stdout);
      fputs (_("\
% 1% of memory, b 1, K 1024 (default), and so on for M, G, T, P, E, Z, Y, R, Q.\
\n\n\
*** WARNING ***\n\
The locale specified by the environment affects sort order.\n\
Set LC_ALL=C to get the traditional sort order that uses\n\
native byte values.\n\
"), stdout );
      emit_ancillary_info (PROGRAM_NAME);
    }

  exit (status);
}

/* For long options that have no equivalent short option, use a
   non-character as a pseudo short option, starting with CHAR_MAX + 1.  */
enum
{
  CHECK_OPTION = CHAR_MAX + 1,
  COMPRESS_PROGRAM_OPTION,
  DEBUG_PROGRAM_OPTION,
  FILES0_FROM_OPTION,
  NMERGE_OPTION,
  RANDOM_SOURCE_OPTION,
  SORT_OPTION,
  PARALLEL_OPTION
};

static char const short_options[] = "-bcCdfghik:mMno:rRsS:t:T:uVy:z";

static struct option const long_options[] =
{
  {"ignore-leading-blanks", no_argument, nullptr, 'b'},
  {"check", optional_argument, nullptr, CHECK_OPTION},
  {"compress-program", required_argument, nullptr, COMPRESS_PROGRAM_OPTION},
  {"debug", no_argument, nullptr, DEBUG_PROGRAM_OPTION},
  {"dictionary-order", no_argument, nullptr, 'd'},
  {"ignore-case", no_argument, nullptr, 'f'},
  {"files0-from", required_argument, nullptr, FILES0_FROM_OPTION},
  {"general-numeric-sort", no_argument, nullptr, 'g'},
  {"ignore-nonprinting", no_argument, nullptr, 'i'},
  {"key", required_argument, nullptr, 'k'},
  {"merge", no_argument, nullptr, 'm'},
  {"month-sort", no_argument, nullptr, 'M'},
  {"numeric-sort", no_argument, nullptr, 'n'},
  {"human-numeric-sort", no_argument, nullptr, 'h'},
  {"version-sort", no_argument, nullptr, 'V'},
  {"random-sort", no_argument, nullptr, 'R'},
  {"random-source", required_argument, nullptr, RANDOM_SOURCE_OPTION},
  {"sort", required_argument, nullptr, SORT_OPTION},
  {"output", required_argument, nullptr, 'o'},
  {"reverse", no_argument, nullptr, 'r'},
  {"stable", no_argument, nullptr, 's'},
  {"batch-size", required_argument, nullptr, NMERGE_OPTION},
  {"buffer-size", required_argument, nullptr, 'S'},
  {"field-separator", required_argument, nullptr, 't'},
  {"temporary-directory", required_argument, nullptr, 'T'},
  {"unique", no_argument, nullptr, 'u'},
  {"zero-terminated", no_argument, nullptr, 'z'},
  {"parallel", required_argument, nullptr, PARALLEL_OPTION},
  {GETOPT_HELP_OPTION_DECL},
  {GETOPT_VERSION_OPTION_DECL},
  {nullptr, 0, nullptr, 0},
};

#define CHECK_TABLE \
  _ct_("quiet",          'C') \
  _ct_("silent",         'C') \
  _ct_("diagnose-first", 'c')

static char const *const check_args[] =
{
#define _ct_(_s, _c) _s,
  CHECK_TABLE nullptr
#undef  _ct_
};
static char const check_types[] =
{
#define _ct_(_s, _c) _c,
  CHECK_TABLE
#undef  _ct_
};

#define SORT_TABLE \
  _st_("general-numeric", 'g') \
  _st_("human-numeric",   'h') \
  _st_("month",           'M') \
  _st_("numeric",         'n') \
  _st_("random",          'R') \
  _st_("version",         'V')

static char const *const sort_args[] =
{
#define _st_(_s, _c) _s,
  SORT_TABLE nullptr
#undef  _st_
};
static char const sort_types[] =
{
#define _st_(_s, _c) _c,
  SORT_TABLE
#undef  _st_
};

 
static sigset_t caught_signals;

 
struct cs_status
{
  bool valid;
  sigset_t sigs;
};

 
static void
cs_enter (struct cs_status *status)
{
  int ret = pthread_sigmask (SIG_BLOCK, &caught_signals, &status->sigs);
  status->valid = ret == 0;
}

 
static void
cs_leave (struct cs_status const *status)
{
  if (status->valid)
    {
       
      pthread_sigmask (SIG_SETMASK, &status->sigs, nullptr);
    }
}

 
enum { UNCOMPRESSED, UNREAPED, REAPED };

 
struct tempnode
{
  struct tempnode *volatile next;
  pid_t pid;      
  char state;
  char name[FLEXIBLE_ARRAY_MEMBER];
};
static struct tempnode *volatile temphead;
static struct tempnode *volatile *temptail = &temphead;

 
struct sortfile
{
   
  char const *name;

   
  struct tempnode *temp;
};

 
static Hash_table *proctab;

enum { INIT_PROCTAB_SIZE = 47 };

static size_t
proctab_hasher (void const *entry, size_t tabsize)
{
  struct tempnode const *node = entry;
  return node->pid % tabsize;
}

static bool
proctab_comparator (void const *e1, void const *e2)
{
  struct tempnode const *n1 = e1;
  struct tempnode const *n2 = e2;
  return n1->pid == n2->pid;
}

 
static pid_t nprocs;

static bool delete_proc (pid_t);

 

static pid_t
reap (pid_t pid)
{
  int status;
  pid_t cpid = waitpid ((pid ? pid : -1), &status, (pid ? 0 : WNOHANG));

  if (cpid < 0)
    error (SORT_FAILURE, errno, _("waiting for %s [-d]"),
           quoteaf (compress_program));
  else if (0 < cpid && (0 < pid || delete_proc (cpid)))
    {
      if (! WIFEXITED (status) || WEXITSTATUS (status))
        error (SORT_FAILURE, 0, _("%s [-d] terminated abnormally"),
               quoteaf (compress_program));
      --nprocs;
    }

  return cpid;
}

 

static void
register_proc (struct tempnode *temp)
{
  if (! proctab)
    {
      proctab = hash_initialize (INIT_PROCTAB_SIZE, nullptr,
                                 proctab_hasher,
                                 proctab_comparator,
                                 nullptr);
      if (! proctab)
        xalloc_die ();
    }

  temp->state = UNREAPED;

  if (! hash_insert (proctab, temp))
    xalloc_die ();
}

 

static bool
delete_proc (pid_t pid)
{
  struct tempnode test;

  test.pid = pid;
  struct tempnode *node = hash_remove (proctab, &test);
  if (! node)
    return false;
  node->state = REAPED;
  return true;
}

 

static void
wait_proc (pid_t pid)
{
  if (delete_proc (pid))
    reap (pid);
}

 

static void
reap_exited (void)
{
  while (0 < nprocs && reap (0))
    continue;
}

 

static void
reap_some (void)
{
  reap (-1);
  reap_exited ();
}

 

static void
reap_all (void)
{
  while (0 < nprocs)
    reap (-1);
}

 

static void
cleanup (void)
{
  struct tempnode const *node;

  for (node = temphead; node; node = node->next)
    unlink (node->name);
  temphead = nullptr;
}

 

static void
exit_cleanup (void)
{
  if (temphead)
    {
       
      struct cs_status cs;
      cs_enter (&cs);
      cleanup ();
      cs_leave (&cs);
    }

  close_stdout ();
}

 

static struct tempnode *
create_temp_file (int *pfd, bool survive_fd_exhaustion)
{
  static char const slashbase[] = "/sortXXXXXX";
  static size_t temp_dir_index;
  int fd;
  int saved_errno;
  char const *temp_dir = temp_dirs[temp_dir_index];
  size_t len = strlen (temp_dir);
  struct tempnode *node =
    xmalloc (FLEXSIZEOF (struct tempnode, name, len + sizeof slashbase));
  char *file = node->name;
  struct cs_status cs;

  memcpy (file, temp_dir, len);
  memcpy (file + len, slashbase, sizeof slashbase);
  node->next = nullptr;
  if (++temp_dir_index == temp_dir_count)
    temp_dir_index = 0;

   
  cs_enter (&cs);
  fd = mkostemp (file, O_CLOEXEC);
  if (0 <= fd)
    {
      *temptail = node;
      temptail = &node->next;
    }
  saved_errno = errno;
  cs_leave (&cs);
  errno = saved_errno;

  if (fd < 0)
    {
      if (! (survive_fd_exhaustion && errno == EMFILE))
        error (SORT_FAILURE, errno, _("cannot create temporary file in %s"),
               quoteaf (temp_dir));
      free (node);
      node = nullptr;
    }

  *pfd = fd;
  return node;
}

 

static struct stat *
get_outstatus (void)
{
  static int outstat_errno;
  static struct stat outstat;
  if (outstat_errno == 0)
    outstat_errno = fstat (STDOUT_FILENO, &outstat) == 0 ? -1 : errno;
  return outstat_errno < 0 ? &outstat : nullptr;
}

 

static FILE *
stream_open (char const *file, char const *how)
{
  FILE *fp;

  if (*how == 'r')
    {
      if (STREQ (file, "-"))
        {
          have_read_stdin = true;
          fp = stdin;
        }
      else
        {
          int fd = open (file, O_RDONLY | O_CLOEXEC);
          fp = fd < 0 ? nullptr : fdopen (fd, how);
        }
      fadvise (fp, FADVISE_SEQUENTIAL);
    }
  else if (*how == 'w')
    {
      if (file && ftruncate (STDOUT_FILENO, 0) != 0)
        {
          int ftruncate_errno = errno;
          struct stat *outst = get_outstatus ();
          if (!outst || S_ISREG (outst->st_mode) || S_TYPEISSHM (outst))
            error (SORT_FAILURE, ftruncate_errno, _("%s: error truncating"),
                   quotef (file));
        }
      fp = stdout;
    }
  else
    affirm (!"unexpected mode passed to stream_open");

  return fp;
}

 

static FILE *
xfopen (char const *file, char const *how)
{
  FILE *fp = stream_open (file, how);
  if (!fp)
    sort_die (_("open failed"), file);
  return fp;
}

 

static void
xfclose (FILE *fp, char const *file)
{
  switch (fileno (fp))
    {
    case STDIN_FILENO:
       
      clearerr (fp);
      break;

    case STDOUT_FILENO:
       
      if (fflush (fp) != 0)
        sort_die (_("fflush failed"), file);
      break;

    default:
      if (fclose (fp) != 0)
        sort_die (_("close failed"), file);
      break;
    }
}

 

static void
move_fd (int oldfd, int newfd)
{
  if (oldfd != newfd)
    {
       
      ignore_value (dup2 (oldfd, newfd));
      ignore_value (close (oldfd));
    }
}

 

static pid_t
pipe_fork (int pipefds[2], size_t tries)
{
#if HAVE_WORKING_FORK
  struct tempnode *saved_temphead;
  int saved_errno;
  double wait_retry = 0.25;
  pid_t pid;
  struct cs_status cs;

  if (pipe2 (pipefds, O_CLOEXEC) < 0)
    return -1;

   

  if (nmerge + 1 < nprocs)
    reap_some ();

  while (tries--)
    {
       
      cs_enter (&cs);
      saved_temphead = temphead;
      temphead = nullptr;

      pid = fork ();
      saved_errno = errno;
      if (pid)
        temphead = saved_temphead;

      cs_leave (&cs);
      errno = saved_errno;

      if (0 <= pid || errno != EAGAIN)
        break;
      else
        {
          xnanosleep (wait_retry);
          wait_retry *= 2;
          reap_exited ();
        }
    }

  if (pid < 0)
    {
      saved_errno = errno;
      close (pipefds[0]);
      close (pipefds[1]);
      errno = saved_errno;
    }
  else if (pid == 0)
    {
      close (STDIN_FILENO);
      close (STDOUT_FILENO);
    }
  else
    ++nprocs;

  return pid;

#else   
  return -1;
#endif
}

 

static struct tempnode *
maybe_create_temp (FILE **pfp, bool survive_fd_exhaustion)
{
  int tempfd;
  struct tempnode *node = create_temp_file (&tempfd, survive_fd_exhaustion);
  if (! node)
    return nullptr;

  node->state = UNCOMPRESSED;

  if (compress_program)
    {
      int pipefds[2];

      node->pid = pipe_fork (pipefds, MAX_FORK_TRIES_COMPRESS);
      if (0 < node->pid)
        {
          close (tempfd);
          close (pipefds[0]);
          tempfd = pipefds[1];

          register_proc (node);
        }
      else if (node->pid == 0)
        {
           
          close (pipefds[1]);
          move_fd (tempfd, STDOUT_FILENO);
          move_fd (pipefds[0], STDIN_FILENO);

          execlp (compress_program, compress_program, (char *) nullptr);

          async_safe_die (errno, "couldn't execute compress program");
        }
    }

  *pfp = fdopen (tempfd, "w");
  if (! *pfp)
    sort_die (_("couldn't create temporary file"), node->name);

  return node;
}

 

static struct tempnode *
create_temp (FILE **pfp)
{
  return maybe_create_temp (pfp, false);
}

 

static FILE *
open_temp (struct tempnode *temp)
{
  int tempfd, pipefds[2];
  FILE *fp = nullptr;

  if (temp->state == UNREAPED)
    wait_proc (temp->pid);

  tempfd = open (temp->name, O_RDONLY);
  if (tempfd < 0)
    return nullptr;

  pid_t child = pipe_fork (pipefds, MAX_FORK_TRIES_DECOMPRESS);

  switch (child)
    {
    case -1:
      if (errno != EMFILE)
        error (SORT_FAILURE, errno, _("couldn't create process for %s -d"),
               quoteaf (compress_program));
      close (tempfd);
      errno = EMFILE;
      break;

    case 0:
       
      close (pipefds[0]);
      move_fd (tempfd, STDIN_FILENO);
      move_fd (pipefds[1], STDOUT_FILENO);

      execlp (compress_program, compress_program, "-d", (char *) nullptr);

      async_safe_die (errno, "couldn't execute compress program (with -d)");

    default:
      temp->pid = child;
      register_proc (temp);
      close (tempfd);
      close (pipefds[1]);

      fp = fdopen (pipefds[0], "r");
      if (! fp)
        {
          int saved_errno = errno;
          close (pipefds[0]);
          errno = saved_errno;
        }
      break;
    }

  return fp;
}

 
static void
add_temp_dir (char const *dir)
{
  if (temp_dir_count == temp_dir_alloc)
    temp_dirs = X2NREALLOC (temp_dirs, &temp_dir_alloc);

  temp_dirs[temp_dir_count++] = dir;
}

 

static void
zaptemp (char const *name)
{
  struct tempnode *volatile *pnode;
  struct tempnode *node;
  struct tempnode *next;
  int unlink_status;
  int unlink_errno = 0;
  struct cs_status cs;

  for (pnode = &temphead; (node = *pnode)->name != name; pnode = &node->next)
    continue;

  if (node->state == UNREAPED)
    wait_proc (node->pid);

   
  next = node->next;
  cs_enter (&cs);
  unlink_status = unlink (name);
  unlink_errno = errno;
  *pnode = next;
  cs_leave (&cs);

  if (unlink_status != 0)
    error (0, unlink_errno, _("warning: cannot remove: %s"), quotef (name));
  if (! next)
    temptail = pnode;
  free (node);
}

#if HAVE_NL_LANGINFO

static int
struct_month_cmp (void const *m1, void const *m2)
{
  struct month const *month1 = m1;
  struct month const *month2 = m2;
  return strcmp (month1->name, month2->name);
}

#endif

 

static void
inittables (void)
{
  size_t i;

  for (i = 0; i < UCHAR_LIM; ++i)
    {
      blanks[i] = field_sep (i);
      nonprinting[i] = ! isprint (i);
      nondictionary[i] = ! isalnum (i) && ! field_sep (i);
      fold_toupper[i] = toupper (i);
    }

#if HAVE_NL_LANGINFO
   
  if (hard_LC_TIME)
    {
      for (i = 0; i < MONTHS_PER_YEAR; i++)
        {
          char const *s;
          size_t s_len;
          size_t j, k;
          char *name;

          s = nl_langinfo (ABMON_1 + i);
          s_len = strlen (s);
          monthtab[i].name = name = xmalloc (s_len + 1);
          monthtab[i].val = i + 1;

          for (j = k = 0; j < s_len; j++)
            if (! isblank (to_uchar (s[j])))
              name[k++] = fold_toupper[to_uchar (s[j])];
          name[k] = '\0';
        }
      qsort (monthtab, MONTHS_PER_YEAR, sizeof *monthtab, struct_month_cmp);
    }
#endif
}

 
static void
specify_nmerge (int oi, char c, char const *s)
{
  uintmax_t n;
  struct rlimit rlimit;
  enum strtol_error e = xstrtoumax (s, nullptr, 10, &n, "");

   
  unsigned int max_nmerge = ((getrlimit (RLIMIT_NOFILE, &rlimit) == 0
                              ? rlimit.rlim_cur
                              : OPEN_MAX)
                             - 3);

  if (e == LONGINT_OK)
    {
      nmerge = n;
      if (nmerge != n)
        e = LONGINT_OVERFLOW;
      else
        {
          if (nmerge < 2)
            {
              error (0, 0, _("invalid --%s argument %s"),
                     long_options[oi].name, quote (s));
              error (SORT_FAILURE, 0,
                     _("minimum --%s argument is %s"),
                     long_options[oi].name, quote ("2"));
            }
          else if (max_nmerge < nmerge)
            {
              e = LONGINT_OVERFLOW;
            }
          else
            return;
        }
    }

  if (e == LONGINT_OVERFLOW)
    {
      char max_nmerge_buf[INT_BUFSIZE_BOUND (max_nmerge)];
      error (0, 0, _("--%s argument %s too large"),
             long_options[oi].name, quote (s));
      error (SORT_FAILURE, 0,
             _("maximum --%s argument with current rlimit is %s"),
             long_options[oi].name,
             uinttostr (max_nmerge, max_nmerge_buf));
    }
  else
    xstrtol_fatal (e, oi, c, long_options, s);
}

 
static void
specify_sort_size (int oi, char c, char const *s)
{
  uintmax_t n;
  char *suffix;
  enum strtol_error e = xstrtoumax (s, &suffix, 10, &n, "EgGkKmMPQRtTYZ");

   
  if (e == LONGINT_OK && ISDIGIT (suffix[-1]))
    {
      if (n <= UINTMAX_MAX / 1024)
        n *= 1024;
      else
        e = LONGINT_OVERFLOW;
    }

   
  if (e == LONGINT_INVALID_SUFFIX_CHAR && ISDIGIT (suffix[-1]) && ! suffix[1])
    switch (suffix[0])
      {
      case 'b':
        e = LONGINT_OK;
        break;

      case '%':
        {
          double mem = physmem_total () * n / 100;

           
          if (mem < UINTMAX_MAX)
            {
              n = mem;
              e = LONGINT_OK;
            }
          else
            e = LONGINT_OVERFLOW;
        }
        break;
      }

  if (e == LONGINT_OK)
    {
       
      if (n < sort_size)
        return;

      sort_size = n;
      if (sort_size == n)
        {
          sort_size = MAX (sort_size, MIN_SORT_SIZE);
          return;
        }

      e = LONGINT_OVERFLOW;
    }

  xstrtol_fatal (e, oi, c, long_options, s);
}

 
static size_t
specify_nthreads (int oi, char c, char const *s)
{
  uintmax_t nthreads;
  enum strtol_error e = xstrtoumax (s, nullptr, 10, &nthreads, "");
  if (e == LONGINT_OVERFLOW)
    return SIZE_MAX;
  if (e != LONGINT_OK)
    xstrtol_fatal (e, oi, c, long_options, s);
  if (SIZE_MAX < nthreads)
    nthreads = SIZE_MAX;
  if (nthreads == 0)
    error (SORT_FAILURE, 0, _("number in parallel must be nonzero"));
  return nthreads;
}

 
static size_t
default_sort_size (void)
{
   
  size_t size = SIZE_MAX;
  struct rlimit rlimit;
  if (getrlimit (RLIMIT_DATA, &rlimit) == 0 && rlimit.rlim_cur < size)
    size = rlimit.rlim_cur;
#ifdef RLIMIT_AS
  if (getrlimit (RLIMIT_AS, &rlimit) == 0 && rlimit.rlim_cur < size)
    size = rlimit.rlim_cur;
#endif

   
  size /= 2;

#ifdef RLIMIT_RSS
   
  if (getrlimit (RLIMIT_RSS, &rlimit) == 0 && rlimit.rlim_cur / 16 * 15 < size)
    size = rlimit.rlim_cur / 16 * 15;
#endif

   
  double avail = physmem_available ();
  double total = physmem_total ();
  double mem = MAX (avail, total / 8);

   
  if (total * 0.75 < size)
    size = total * 0.75;

   
  if (mem < size)
    size = mem;
  return MAX (size, MIN_SORT_SIZE);
}

 

static size_t
sort_buffer_size (FILE *const *fps, size_t nfps,
                  char *const *files, size_t nfiles,
                  size_t line_bytes)
{
   
  static size_t size_bound;

   
  size_t worst_case_per_input_byte = line_bytes + 1;

   
  size_t size = worst_case_per_input_byte + 1;

  for (size_t i = 0; i < nfiles; i++)
    {
      struct stat st;
      off_t file_size;
      size_t worst_case;

      if ((i < nfps ? fstat (fileno (fps[i]), &st)
           : STREQ (files[i], "-") ? fstat (STDIN_FILENO, &st)
           : stat (files[i], &st))
          != 0)
        sort_die (_("stat failed"), files[i]);

      if (S_ISREG (st.st_mode))
        file_size = st.st_size;
      else
        {
           
          if (sort_size)
            return sort_size;
          file_size = INPUT_FILE_SIZE_GUESS;
        }

      if (! size_bound)
        {
          size_bound = sort_size;
          if (! size_bound)
            size_bound = default_sort_size ();
        }

       
      worst_case = file_size * worst_case_per_input_byte + 1;
      if (file_size != worst_case / worst_case_per_input_byte
          || size_bound - size <= worst_case)
        return size_bound;
      size += worst_case;
    }

  return size;
}

 

static void
initbuf (struct buffer *buf, size_t line_bytes, size_t alloc)
{
   
  while (true)
    {
      alloc += sizeof (struct line) - alloc % sizeof (struct line);
      buf->buf = malloc (alloc);
      if (buf->buf)
        break;
      alloc /= 2;
      if (alloc <= line_bytes + 1)
        xalloc_die ();
    }

  buf->line_bytes = line_bytes;
  buf->alloc = alloc;
  buf->used = buf->left = buf->nlines = 0;
  buf->eof = false;
}

 

static inline struct line *
buffer_linelim (struct buffer const *buf)
{
  void *linelim = buf->buf + buf->alloc;
  return linelim;
}

 

static char *
begfield (struct line const *line, struct keyfield const *key)
{
  char *ptr = line->text, *lim = ptr + line->length - 1;
  size_t sword = key->sword;
  size_t schar = key->schar;

   

  if (tab != TAB_DEFAULT)
    while (ptr < lim && sword--)
      {
        while (ptr < lim && *ptr != tab)
          ++ptr;
        if (ptr < lim)
          ++ptr;
      }
  else
    while (ptr < lim && sword--)
      {
        while (ptr < lim && blanks[to_uchar (*ptr)])
          ++ptr;
        while (ptr < lim && !blanks[to_uchar (*ptr)])
          ++ptr;
      }

   
  if (key->skipsblanks)
    while (ptr < lim && blanks[to_uchar (*ptr)])
      ++ptr;

   
  ptr = MIN (lim, ptr + schar);

  return ptr;
}

 

ATTRIBUTE_PURE
static char *
limfield (struct line const *line, struct keyfield const *key)
{
  char *ptr = line->text, *lim = ptr + line->length - 1;
  size_t eword = key->eword, echar = key->echar;

  if (echar == 0)
    eword++;  

   
  if (tab != TAB_DEFAULT)
    while (ptr < lim && eword--)
      {
        while (ptr < lim && *ptr != tab)
          ++ptr;
        if (ptr < lim && (eword || echar))
          ++ptr;
      }
  else
    while (ptr < lim && eword--)
      {
        while (ptr < lim && blanks[to_uchar (*ptr)])
          ++ptr;
        while (ptr < lim && !blanks[to_uchar (*ptr)])
          ++ptr;
      }

#ifdef POSIX_UNSPECIFIED
   

   
  if (tab != TAB_DEFAULT)
    {
      char *newlim;
      newlim = memchr (ptr, tab, lim - ptr);
      if (newlim)
        lim = newlim;
    }
  else
    {
      char *newlim;
      newlim = ptr;
      while (newlim < lim && blanks[to_uchar (*newlim)])
        ++newlim;
      while (newlim < lim && !blanks[to_uchar (*newlim)])
        ++newlim;
      lim = newlim;
    }
#endif

  if (echar != 0)  
    {
       
      if (key->skipeblanks)
        while (ptr < lim && blanks[to_uchar (*ptr)])
          ++ptr;

       
      ptr = MIN (lim, ptr + echar);
    }

  return ptr;
}

 

static bool
fillbuf (struct buffer *buf, FILE *fp, char const *file)
{
  struct keyfield const *key = keylist;
  char eol = eolchar;
  size_t line_bytes = buf->line_bytes;
  size_t mergesize = merge_buffer_size - MIN_MERGE_BUFFER_SIZE;

  if (buf->eof)
    return false;

  if (buf->used != buf->left)
    {
      memmove (buf->buf, buf->buf + buf->used - buf->left, buf->left);
      buf->used = buf->left;
      buf->nlines = 0;
    }

  while (true)
    {
      char *ptr = buf->buf + buf->used;
      struct line *linelim = buffer_linelim (buf);
      struct line *line = linelim - buf->nlines;
      size_t avail = (char *) linelim - buf->nlines * line_bytes - ptr;
      char *line_start = buf->nlines ? line->text + line->length : buf->buf;

      while (line_bytes + 1 < avail)
        {
           
          size_t readsize = (avail - 1) / (line_bytes + 1);
          size_t bytes_read = fread (ptr, 1, readsize, fp);
          char *ptrlim = ptr + bytes_read;
          char *p;
          avail -= bytes_read;

          if (bytes_read != readsize)
            {
              if (ferror (fp))
                sort_die (_("read failed"), file);
              if (feof (fp))
                {
                  buf->eof = true;
                  if (buf->buf == ptrlim)
                    return false;
                  if (line_start != ptrlim && ptrlim[-1] != eol)
                    *ptrlim++ = eol;
                }
            }

           
          while ((p = memchr (ptr, eol, ptrlim - ptr)))
            {
               
              *p = '\0';
              ptr = p + 1;
              line--;
              line->text = line_start;
              line->length = ptr - line_start;
              mergesize = MAX (mergesize, line->length);
              avail -= line_bytes;

              if (key)
                {
                   
                  line->keylim = (key->eword == SIZE_MAX
                                  ? p
                                  : limfield (line, key));

                  if (key->sword != SIZE_MAX)
                    line->keybeg = begfield (line, key);
                  else
                    {
                      if (key->skipsblanks)
                        while (blanks[to_uchar (*line_start)])
                          line_start++;
                      line->keybeg = line_start;
                    }
                }

              line_start = ptr;
            }

          ptr = ptrlim;
          if (buf->eof)
            break;
        }

      buf->used = ptr - buf->buf;
      buf->nlines = buffer_linelim (buf) - line;
      if (buf->nlines != 0)
        {
          buf->left = ptr - line_start;
          merge_buffer_size = mergesize + MIN_MERGE_BUFFER_SIZE;
          return true;
        }

      {
         
        size_t line_alloc = buf->alloc / sizeof (struct line);
        buf->buf = x2nrealloc (buf->buf, &line_alloc, sizeof (struct line));
        buf->alloc = line_alloc * sizeof (struct line);
      }
    }
}

 
static char const unit_order[UCHAR_LIM] =
  {
#if ! ('K' == 75 && 'M' == 77 && 'G' == 71 && 'T' == 84 && 'P' == 80 \
       && 'E' == 69 && 'Z' == 90 && 'Y' == 89 && 'R' == 82 && 'Q' == 81 \
       && 'k' == 107)
     
    ['K']=1, ['M']=2, ['G']=3, ['T']=4, ['P']=5, ['E']=6, ['Z']=7, ['Y']=8,
    ['R']=9, ['Q']=10,
    ['k']=1,
#else
     
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 6, 0, 3,
    0, 0, 0, 1, 0, 2, 0, 0, 5, 10, 9, 0, 4, 0, 0, 0, 0, 8, 7, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
#endif
  };

 
static char
traverse_raw_number (char const **number)
{
  char const *p = *number;
  char ch;
  char max_digit = '\0';
  bool ends_with_thousands_sep = false;

   

  while (ISDIGIT (ch = *p++))
    {
      if (max_digit < ch)
        max_digit = ch;

       
      ends_with_thousands_sep = (*p == thousands_sep);
      if (ends_with_thousands_sep)
        ++p;
    }

  if (ends_with_thousands_sep)
    {
       
      *number = p - 2;
      return max_digit;
    }

  if (ch == decimal_point)
    while (ISDIGIT (ch = *p++))
      if (max_digit < ch)
        max_digit = ch;

  *number = p - 1;
  return max_digit;
}

 

ATTRIBUTE_PURE
static int
find_unit_order (char const *number)
{
  bool minus_sign = (*number == '-');
  char const *p = number + minus_sign;
  char max_digit = traverse_raw_number (&p);
  if ('0' < max_digit)
    {
      unsigned char ch = *p;
      int order = unit_order[ch];
      return (minus_sign ? -order : order);
    }
  else
    return 0;
}

 

ATTRIBUTE_PURE
static int
human_numcompare (char const *a, char const *b)
{
  while (blanks[to_uchar (*a)])
    a++;
  while (blanks[to_uchar (*b)])
    b++;

  int diff = find_unit_order (a) - find_unit_order (b);
  return (diff ? diff : strnumcmp (a, b, decimal_point, thousands_sep));
}

 

ATTRIBUTE_PURE
static int
numcompare (char const *a, char const *b)
{
  while (blanks[to_uchar (*a)])
    a++;
  while (blanks[to_uchar (*b)])
    b++;

  return strnumcmp (a, b, decimal_point, thousands_sep);
}

static int
nan_compare (long double a, long double b)
{
  char buf[2][sizeof "-nan""()" + CHAR_BIT * sizeof a];
  snprintf (buf[0], sizeof buf[0], "%Lf", a);
  snprintf (buf[1], sizeof buf[1], "%Lf", b);
  return strcmp (buf[0], buf[1]);
}

static int
general_numcompare (char const *sa, char const *sb)
{
   

  char *ea;
  char *eb;
  long double a = strtold (sa, &ea);
  long double b = strtold (sb, &eb);

   
  if (sa == ea)
    return sb == eb ? 0 : -1;
  if (sb == eb)
    return 1;

   
  return (a < b ? -1
          : a > b ? 1
          : a == b ? 0
          : b == b ? -1
          : a == a ? 1
          : nan_compare (a, b));
}

 

static int
getmonth (char const *month, char **ea)
{
  size_t lo = 0;
  size_t hi = MONTHS_PER_YEAR;

  while (blanks[to_uchar (*month)])
    month++;

  do
    {
      size_t ix = (lo + hi) / 2;
      char const *m = month;
      char const *n = monthtab[ix].name;

      for (;; m++, n++)
        {
          if (!*n)
            {
              if (ea)
                *ea = (char *) m;
              return monthtab[ix].val;
            }
          if (to_uchar (fold_toupper[to_uchar (*m)]) < to_uchar (*n))
            {
              hi = ix;
              break;
            }
          else if (to_uchar (fold_toupper[to_uchar (*m)]) > to_uchar (*n))
            {
              lo = ix + 1;
              break;
            }
        }
    }
  while (lo < hi);

  return 0;
}

 
static struct md5_ctx random_md5_state;

 

static void
random_md5_state_init (char const *random_source)
{
  unsigned char buf[MD5_DIGEST_SIZE];
  struct randread_source *r = randread_new (random_source, sizeof buf);
  if (! r)
    sort_die (_("open failed"), random_source ? random_source : "getrandom");
  randread (r, buf, sizeof buf);
  if (randread_free (r) != 0)
    sort_die (_("close failed"), random_source);
  md5_init_ctx (&random_md5_state);
  md5_process_bytes (buf, sizeof buf, &random_md5_state);
}

 

static size_t
xstrxfrm (char *restrict dest, char const *restrict src, size_t destsize)
{
  errno = 0;
  size_t translated_size = strxfrm (dest, src, destsize);

  if (errno)
    {
      error (0, errno, _("string transformation failed"));
      error (0, 0, _("set LC_ALL='C' to work around the problem"));
      error (SORT_FAILURE, 0,
             _("the original string was %s"),
             quotearg_n_style (0, locale_quoting_style, src));
    }

  return translated_size;
}

 

static int
compare_random (char *restrict texta, size_t lena,
                char *restrict textb, size_t lenb)
{
   
  int xfrm_diff = 0;

  char stackbuf[4000];
  char *buf = stackbuf;
  size_t bufsize = sizeof stackbuf;
  void *allocated = nullptr;
  uint32_t dig[2][MD5_DIGEST_SIZE / sizeof (uint32_t)];
  struct md5_ctx s[2];
  s[0] = s[1] = random_md5_state;

  if (hard_LC_COLLATE)
    {
      char const *lima = texta + lena;
      char const *limb = textb + lenb;

      while (true)
        {
           

           

           
          size_t guess_bufsize = 3 * (lena + lenb) + 2;
          if (bufsize < guess_bufsize)
            {
              bufsize = MAX (guess_bufsize, bufsize * 3 / 2);
              free (allocated);
              buf = allocated = malloc (bufsize);
              if (! buf)
                {
                  buf = stackbuf;
                  bufsize = sizeof stackbuf;
                }
            }

          size_t sizea =
            (texta < lima ? xstrxfrm (buf, texta, bufsize) + 1 : 0);
          bool a_fits = sizea <= bufsize;
          size_t sizeb =
            (textb < limb
             ? (xstrxfrm ((a_fits ? buf + sizea : nullptr), textb,
                          (a_fits ? bufsize - sizea : 0))
                + 1)
             : 0);

          if (! (a_fits && sizea + sizeb <= bufsize))
            {
              bufsize = sizea + sizeb;
              if (bufsize < SIZE_MAX / 3)
                bufsize = bufsize * 3 / 2;
              free (allocated);
              buf = allocated = xmalloc (bufsize);
              if (texta < lima)
                strxfrm (buf, texta, sizea);
              if (textb < limb)
                strxfrm (buf + sizea, textb, sizeb);
            }

           
          if (texta < lima)
            texta += strlen (texta) + 1;
          if (textb < limb)
            textb += strlen (textb) + 1;
          if (! (texta < lima || textb < limb))
            {
              lena = sizea; texta = buf;
              lenb = sizeb; textb = buf + sizea;
              break;
            }

           
          md5_process_bytes (buf, sizea, &s[0]);
          md5_process_bytes (buf + sizea, sizeb, &s[1]);

           
          if (! xfrm_diff)
            {
              xfrm_diff = memcmp (buf, buf + sizea, MIN (sizea, sizeb));
              if (! xfrm_diff)
                xfrm_diff = (sizea > sizeb) - (sizea < sizeb);
            }
        }
    }

   
  md5_process_bytes (texta, lena, &s[0]); md5_finish_ctx (&s[0], dig[0]);
  md5_process_bytes (textb, lenb, &s[1]); md5_finish_ctx (&s[1], dig[1]);
  int diff = memcmp (dig[0], dig[1], sizeof dig[0]);

   
  if (! diff)
    {
      if (! xfrm_diff)
        {
          xfrm_diff = memcmp (texta, textb, MIN (lena, lenb));
          if (! xfrm_diff)
            xfrm_diff = (lena > lenb) - (lena < lenb);
        }

      diff = xfrm_diff;
    }

  free (allocated);

  return diff;
}

 

static size_t
debug_width (char const *text, char const *lim)
{
  size_t width = mbsnwidth (text, lim - text, 0);
  while (text < lim)
    width += (*text++ == '\t');
  return width;
}

 

static void
mark_key (size_t offset, size_t width)
{
  while (offset--)
    putchar (' ');

  if (!width)
    printf (_("^ no match for key\n"));
  else
    {
      do
        putchar ('_');
      while (--width);

      putchar ('\n');
    }
}

 

static inline bool
key_numeric (struct keyfield const *key)
{
  return key->numeric || key->general_numeric || key->human_numeric;
}

 

static void
debug_key (struct line const *line, struct keyfield const *key)
{
  char *text = line->text;
  char *beg = text;
  char *lim = text + line->length - 1;

  if (key)
    {
      if (key->sword != SIZE_MAX)
        beg = begfield (line, key);
      if (key->eword != SIZE_MAX)
        lim = limfield (line, key);

      if ((key->skipsblanks && key->sword == SIZE_MAX)
          || key->month || key_numeric (key))
        {
          char saved = *lim;
          *lim = '\0';

          while (blanks[to_uchar (*beg)])
            beg++;

          char *tighter_lim = beg;

          if (lim < beg)
            tighter_lim = lim;
          else if (key->month)
            getmonth (beg, &tighter_lim);
          else if (key->general_numeric)
            ignore_value (strtold (beg, &tighter_lim));
          else if (key->numeric || key->human_numeric)
            {
              char const *p = beg + (beg < lim && *beg == '-');
              char max_digit = traverse_raw_number (&p);
              if ('0' <= max_digit)
                {
                  unsigned char ch = *p;
                  tighter_lim = (char *) p
                    + (key->human_numeric && unit_order[ch]);
                }
            }
          else
            tighter_lim = lim;

          *lim = saved;
          lim = tighter_lim;
        }
    }

  size_t offset = debug_width (text, beg);
  size_t width = debug_width (beg, lim);
  mark_key (offset, width);
}

 

static void
debug_line (struct line const *line)
{
  struct keyfield const *key = keylist;

  do
    debug_key (line, key);
  while (key && ((key = key->next) || ! (unique || stable)));
}

 

static bool
default_key_compare (struct keyfield const *key)
{
  return ! (key->ignore
            || key->translate
            || key->skipsblanks
            || key->skipeblanks
            || key_numeric (key)
            || key->month
            || key->version
            || key->random
             
           );
}

 

static void
key_to_opts (struct keyfield const *key, char *opts)
{
  if (key->skipsblanks || key->skipeblanks)
    *opts++ = 'b'; 
  if (key->ignore == nondictionary)
    *opts++ = 'd';
  if (key->translate)
    *opts++ = 'f';
  if (key->general_numeric)
    *opts++ = 'g';
  if (key->human_numeric)
    *opts++ = 'h';
  if (key->ignore == nonprinting)
    *opts++ = 'i';
  if (key->month)
    *opts++ = 'M';
  if (key->numeric)
    *opts++ = 'n';
  if (key->random)
    *opts++ = 'R';
  if (key->reverse)
    *opts++ = 'r';
  if (key->version)
    *opts++ = 'V';
  *opts = '\0';
}

 

static void
key_warnings (struct keyfield const *gkey, bool gkey_only)
{
  struct keyfield const *key;
  struct keyfield ugkey = *gkey;
  unsigned long keynum = 1;
  bool basic_numeric_field = false;
  bool general_numeric_field = false;
  bool basic_numeric_field_span = false;
  bool general_numeric_field_span = false;

  for (key = keylist; key; key = key->next, keynum++)
    {
      if (key_numeric (key))
        {
          if (key->general_numeric)
            general_numeric_field = true;
          else
            basic_numeric_field = true;
        }

      if (key->traditional_used)
        {
          size_t sword = key->sword;
          size_t eword = key->eword;
          char tmp[INT_BUFSIZE_BOUND (uintmax_t)];
           
          char obuf[INT_BUFSIZE_BOUND (sword) * 2 + 4];  
          char nbuf[INT_BUFSIZE_BOUND (sword) * 2 + 5];  
          char *po = obuf;
          char *pn = nbuf;

          if (sword == SIZE_MAX)
            sword++;

          po = stpcpy (stpcpy (po, "+"), umaxtostr (sword, tmp));
          pn = stpcpy (stpcpy (pn, "-k "), umaxtostr (sword + 1, tmp));
          if (key->eword != SIZE_MAX)
            {
              stpcpy (stpcpy (po, " -"), umaxtostr (eword + 1, tmp));
              stpcpy (stpcpy (pn, ","),
                      umaxtostr (eword + 1
                                 + (key->echar == SIZE_MAX), tmp));
            }
          error (0, 0, _("obsolescent key %s used; consider %s instead"),
                 quote_n (0, obuf), quote_n (1, nbuf));
        }

       
      bool zero_width = key->sword != SIZE_MAX && key->eword < key->sword;
      if (zero_width)
        error (0, 0, _("key %lu has zero width and will be ignored"), keynum);

       
      bool implicit_skip = key_numeric (key) || key->month;
      bool line_offset = key->eword == 0 && key->echar != 0;  
      if (!zero_width && !gkey_only && tab == TAB_DEFAULT && !line_offset
          && ((!key->skipsblanks && !implicit_skip)
              || (!key->skipsblanks && key->schar)
              || (!key->skipeblanks && key->echar)))
        error (0, 0, _("leading blanks are significant in key %lu; "
                       "consider also specifying 'b'"), keynum);

       
      if (!gkey_only && key_numeric (key))
        {
          size_t sword = key->sword + 1;
          size_t eword = key->eword + 1;
          if (!sword)
            sword++;
          if (!eword || sword < eword)
            {
              error (0, 0, _("key %lu is numeric and spans multiple fields"),
                     keynum);
              if (key->general_numeric)
                general_numeric_field_span = true;
              else
                basic_numeric_field_span = true;
            }
        }

       
      if (ugkey.ignore && (ugkey.ignore == key->ignore))
        ugkey.ignore = nullptr;
      if (ugkey.translate && (ugkey.translate == key->translate))
        ugkey.translate = nullptr;
      ugkey.skipsblanks &= !key->skipsblanks;
      ugkey.skipeblanks &= !key->skipeblanks;
      ugkey.month &= !key->month;
      ugkey.numeric &= !key->numeric;
      ugkey.general_numeric &= !key->general_numeric;
      ugkey.human_numeric &= !key->human_numeric;
      ugkey.random &= !key->random;
      ugkey.version &= !key->version;
      ugkey.reverse &= !key->reverse;
    }

   
  bool number_locale_warned = false;
  if (basic_numeric_field_span)
    {
      if (tab == TAB_DEFAULT
          ? thousands_sep != NON_CHAR && (isblank (to_uchar (thousands_sep)))
          : tab == thousands_sep)
        {
          error (0, 0,
                 _("field separator %s is treated as a "
                   "group separator in numbers"),
                 quote (((char []) {thousands_sep, 0})));
          number_locale_warned = true;
        }
    }
  if (basic_numeric_field_span || general_numeric_field_span)
    {
      if (tab == TAB_DEFAULT
          ? thousands_sep != NON_CHAR && (isblank (to_uchar (decimal_point)))
          : tab == decimal_point)
        {
          error (0, 0,
                 _("field separator %s is treated as a "
                   "decimal point in numbers"),
                 quote (((char []) {decimal_point, 0})));
          number_locale_warned = true;
        }
      else if (tab == '-')
        {
          error (0, 0,
                 _("field separator %s is treated as a "
                   "minus sign in numbers"),
                 quote (((char []) {tab, 0})));
        }
      else if (general_numeric_field_span && tab == '+')
        {
          error (0, 0,
                 _("field separator %s is treated as a "
                   "plus sign in numbers"),
                 quote (((char []) {tab, 0})));
        }
    }

   
  if ((basic_numeric_field || general_numeric_field) && ! number_locale_warned)
    {
      error (0, 0,
             _("%snumbers use %s as a decimal point in this locale"),
             tab == decimal_point ? "" : _("note "),
             quote (((char []) {decimal_point, 0})));

    }

  if (basic_numeric_field && thousands_sep_ignored)
    {
      error (0, 0,
             _("the multi-byte number group separator "
               "in this locale is not supported"));
    }

   
  if (!default_key_compare (&ugkey)
      || (ugkey.reverse && (stable || unique) && keylist))
    {
      bool ugkey_reverse = ugkey.reverse;
      if (!(stable || unique))
        ugkey.reverse = false;
       
      char opts[sizeof short_options];
      key_to_opts (&ugkey, opts);
      error (0, 0,
             ngettext ("option '-%s' is ignored",
                       "options '-%s' are ignored",
                       select_plural (strlen (opts))), opts);
      ugkey.reverse = ugkey_reverse;
    }
  if (ugkey.reverse && !(stable || unique) && keylist)
    error (0, 0, _("option '-r' only applies to last-resort comparison"));
}

 

static int
diff_reversed (int diff, bool reversed)
{
  return reversed ? (diff < 0) - (diff > 0) : diff;
}

 

static int
keycompare (struct line const *a, struct line const *b)
{
  struct keyfield *key = keylist;

   
  char *texta = a->keybeg;
  char *textb = b->keybeg;
  char *lima = a->keylim;
  char *limb = b->keylim;

  int diff;

  while (true)
    {
      char const *translate = key->translate;
      bool const *ignore = key->ignore;

       
      lima = MAX (texta, lima);
      limb = MAX (textb, limb);

       
      size_t lena = lima - texta;
      size_t lenb = limb - textb;

      if (hard_LC_COLLATE || key_numeric (key)
          || key->month || key->random || key->version)
        {
           
          char *ta = texta;
          char *tb = textb;
          size_t tlena = lena;
          size_t tlenb = lenb;
          char enda = ta[tlena];
          char endb = tb[tlenb];

          void *allocated = nullptr;
          char stackbuf[4000];

          if (ignore || translate)
            {
               

              size_t i;

               
              size_t size = lena + 1 + lenb + 1;
              if (size <= sizeof stackbuf)
                ta = stackbuf;
              else
                ta = allocated = xmalloc (size);
              tb = ta + lena + 1;

               
              for (tlena = i = 0; i < lena; i++)
                if (! (ignore && ignore[to_uchar (texta[i])]))
                  ta[tlena++] = (translate
                                 ? translate[to_uchar (texta[i])]
                                 : texta[i]);

              for (tlenb = i = 0; i < lenb; i++)
                if (! (ignore && ignore[to_uchar (textb[i])]))
                  tb[tlenb++] = (translate
                                 ? translate[to_uchar (textb[i])]
                                 : textb[i]);
            }

          ta[tlena] = '\0';
          tb[tlenb] = '\0';

          if (key->numeric)
            diff = numcompare (ta, tb);
          else if (key->general_numeric)
            diff = general_numcompare (ta, tb);
          else if (key->human_numeric)
            diff = human_numcompare (ta, tb);
          else if (key->month)
            diff = getmonth (ta, nullptr) - getmonth (tb, nullptr);
          else if (key->random)
            diff = compare_random (ta, tlena, tb, tlenb);
          else if (key->version)
            diff = filenvercmp (ta, tlena, tb, tlenb);
          else
            {
               
              if (tlena == 0)
                diff = - NONZERO (tlenb);
              else if (tlenb == 0)
                diff = 1;
              else
                diff = xmemcoll0 (ta, tlena + 1, tb, tlenb + 1);
            }

          ta[tlena] = enda;
          tb[tlenb] = endb;

          free (allocated);
        }
      else if (ignore)
        {
#define CMP_WITH_IGNORE(A, B)						\
  do									\
    {									\
          while (true)							\
            {								\
              while (texta < lima && ignore[to_uchar (*texta)])		\
                ++texta;						\
              while (textb < limb && ignore[to_uchar (*textb)])		\
                ++textb;						\
              if (! (texta < lima && textb < limb))			\
                {							\
                  diff = (texta < lima) - (textb < limb);		\
                  break;						\
                }							\
              diff = to_uchar (A) - to_uchar (B);			\
              if (diff)							\
                break;							\
              ++texta;							\
              ++textb;							\
            }								\
                                                                        \
    }									\
  while (0)

          if (translate)
            CMP_WITH_IGNORE (translate[to_uchar (*texta)],
                             translate[to_uchar (*textb)]);
          else
            CMP_WITH_IGNORE (*texta, *textb);
        }
      else
        {
          size_t lenmin = MIN (lena, lenb);
          if (lenmin == 0)
            diff = 0;
          else if (translate)
            {
              size_t i = 0;
              do
                {
                  diff = (to_uchar (translate[to_uchar (texta[i])])
                          - to_uchar (translate[to_uchar (textb[i])]));
                  if (diff)
                    break;
                  i++;
                }
              while (i < lenmin);
            }
          else
            diff = memcmp (texta, textb, lenmin);

          if (! diff)
            diff = (lena > lenb) - (lena < lenb);
        }

      if (diff)
        break;

      key = key->next;
      if (! key)
        return 0;

       
      if (key->eword != SIZE_MAX)
        lima = limfield (a, key), limb = limfield (b, key);
      else
        lima = a->text + a->length - 1, limb = b->text + b->length - 1;

      if (key->sword != SIZE_MAX)
        texta = begfield (a, key), textb = begfield (b, key);
      else
        {
          texta = a->text, textb = b->text;
          if (key->skipsblanks)
            {
              while (texta < lima && blanks[to_uchar (*texta)])
                ++texta;
              while (textb < limb && blanks[to_uchar (*textb)])
                ++textb;
            }
        }
    }

  return diff_reversed (diff, key->reverse);
}

 

static int
compare (struct line const *a, struct line const *b)
{
  int diff;
  size_t alen, blen;

   
  if (keylist)
    {
      diff = keycompare (a, b);
      if (diff || unique || stable)
        return diff;
    }

   
  alen = a->length - 1, blen = b->length - 1;

  if (alen == 0)
    diff = - NONZERO (blen);
  else if (blen == 0)
    diff = 1;
  else if (hard_LC_COLLATE)
    {
       
      diff = xmemcoll0 (a->text, alen + 1, b->text, blen + 1);
    }
  else
    {
      diff = memcmp (a->text, b->text, MIN (alen, blen));
      if (!diff)
        diff = (alen > blen) - (alen < blen);
    }

  return diff_reversed (diff, reverse);
}

 

static void
write_line (struct line const *line, FILE *fp, char const *output_file)
{
  char *buf = line->text;
  size_t n_bytes = line->length;
  char *ebuf = buf + n_bytes;

  if (!output_file && debug)
    {
       
      char const *c = buf;

      while (c < ebuf)
        {
          char wc = *c++;
          if (wc == '\t')
            wc = '>';
          else if (c == ebuf)
            wc = '\n';
          if (fputc (wc, fp) == EOF)
            sort_die (_("write failed"), output_file);
        }

      debug_line (line);
    }
  else
    {
      ebuf[-1] = eolchar;
      if (fwrite (buf, 1, n_bytes, fp) != n_bytes)
        sort_die (_("write failed"), output_file);
      ebuf[-1] = '\0';
    }
}

 

static bool
check (char const *file_name, char checkonly)
{
  FILE *fp = xfopen (file_name, "r");
  struct buffer buf;		 
  struct line temp;		 
  size_t alloc = 0;
  uintmax_t line_number = 0;
  struct keyfield const *key = keylist;
  bool nonunique = ! unique;
  bool ordered = true;

  initbuf (&buf, sizeof (struct line),
           MAX (merge_buffer_size, sort_size));
  temp.text = nullptr;

  while (fillbuf (&buf, fp, file_name))
    {
      struct line const *line = buffer_linelim (&buf);
      struct line const *linebase = line - buf.nlines;

       
      if (alloc && nonunique <= compare (&temp, line - 1))
        {
        found_disorder:
          {
            if (checkonly == 'c')
              {
                struct line const *disorder_line = line - 1;
                uintmax_t disorder_line_number =
                  buffer_linelim (&buf) - disorder_line + line_number;
                char hr_buf[INT_BUFSIZE_BOUND (disorder_line_number)];
                fprintf (stderr, _("%s: %s:%s: disorder: "),
                         program_name, file_name,
                         umaxtostr (disorder_line_number, hr_buf));
                write_line (disorder_line, stderr, _("standard error"));
              }

            ordered = false;
            break;
          }
        }

       
      while (linebase < --line)
        if (nonunique <= compare (line, line - 1))
          goto found_disorder;

      line_number += buf.nlines;

       
      if (alloc < line->length)
        {
          do
            {
              alloc *= 2;
              if (! alloc)
                {
                  alloc = line->length;
                  break;
                }
            }
          while (alloc < line->length);

          free (temp.text);
          temp.text = xmalloc (alloc);
        }
      memcpy (temp.text, line->text, line->length);
      temp.length = line->length;
      if (key)
        {
          temp.keybeg = temp.text + (line->keybeg - line->text);
          temp.keylim = temp.text + (line->keylim - line->text);
        }
    }

  xfclose (fp, file_name);
  free (buf.buf);
  free (temp.text);
  return ordered;
}

 

static size_t
open_input_files (struct sortfile *files, size_t nfiles, FILE ***pfps)
{
  FILE **fps = *pfps = xnmalloc (nfiles, sizeof *fps);
  int i;

   
  for (i = 0; i < nfiles; i++)
    {
      fps[i] = (files[i].temp && files[i].temp->state != UNCOMPRESSED
                ? open_temp (files[i].temp)
                : stream_open (files[i].name, "r"));
      if (!fps[i])
        break;
    }

  return i;
}

 

static void
mergefps (struct sortfile *files, size_t ntemps, size_t nfiles,
          FILE *ofp, char const *output_file, FILE **fps)
{
  struct buffer *buffer = xnmalloc (nfiles, sizeof *buffer);
                                 
  struct line saved;		 
  struct line const *savedline = nullptr;
                                 
  size_t savealloc = 0;		 
  struct line const **cur = xnmalloc (nfiles, sizeof *cur);
                                 
  struct line const **base = xnmalloc (nfiles, sizeof *base);
                                 
  size_t *ord = xnmalloc (nfiles, sizeof *ord);
                                 
  size_t i;
  size_t j;
  size_t t;
  struct keyfield const *key = keylist;
  saved.text = nullptr;

   
  for (i = 0; i < nfiles; )
    {
      initbuf (&buffer[i], sizeof (struct line),
               MAX (merge_buffer_size, sort_size / nfiles));
      if (fillbuf (&buffer[i], fps[i], files[i].name))
        {
          struct line const *linelim = buffer_linelim (&buffer[i]);
          cur[i] = linelim - 1;
          base[i] = linelim - buffer[i].nlines;
          i++;
        }
      else
        {
           
          xfclose (fps[i], files[i].name);
          if (i < ntemps)
            {
              ntemps--;
              zaptemp (files[i].name);
            }
          free (buffer[i].buf);
          --nfiles;
          for (j = i; j < nfiles; ++j)
            {
              files[j] = files[j + 1];
              fps[j] = fps[j + 1];
            }
        }
    }

   
  for (i = 0; i < nfiles; ++i)
    ord[i] = i;
  for (i = 1; i < nfiles; ++i)
    if (0 < compare (cur[ord[i - 1]], cur[ord[i]]))
      t = ord[i - 1], ord[i - 1] = ord[i], ord[i] = t, i = 0;

   
  while (nfiles)
    {
      struct line const *smallest = cur[ord[0]];

       
      if (unique)
        {
          if (savedline && compare (savedline, smallest))
            {
              savedline = nullptr;
              write_line (&saved, ofp, output_file);
            }
          if (!savedline)
            {
              savedline = &saved;
              if (savealloc < smallest->length)
                {
                  do
                    if (! savealloc)
                      {
                        savealloc = smallest->length;
                        break;
                      }
                  while ((savealloc *= 2) < smallest->length);

                  free (saved.text);
                  saved.text = xmalloc (savealloc);
                }
              saved.length = smallest->length;
              memcpy (saved.text, smallest->text, saved.length);
              if (key)
                {
                  saved.keybeg =
                    saved.text + (smallest->keybeg - smallest->text);
                  saved.keylim =
                    saved.text + (smallest->keylim - smallest->text);
                }
            }
        }
      else
        write_line (smallest, ofp, output_file);

       
      if (base[ord[0]] < smallest)
        cur[ord[0]] = smallest - 1;
      else
        {
          if (fillbuf (&buffer[ord[0]], fps[ord[0]], files[ord[0]].name))
            {
              struct line const *linelim = buffer_linelim (&buffer[ord[0]]);
              cur[ord[0]] = linelim - 1;
              base[ord[0]] = linelim - buffer[ord[0]].nlines;
            }
          else
            {
               
              for (i = 1; i < nfiles; ++i)
                if (ord[i] > ord[0])
                  --ord[i];
              --nfiles;
              xfclose (fps[ord[0]], files[ord[0]].name);
              if (ord[0] < ntemps)
                {
                  ntemps--;
                  zaptemp (files[ord[0]].name);
                }
              free (buffer[ord[0]].buf);
              for (i = ord[0]; i < nfiles; ++i)
                {
                  fps[i] = fps[i + 1];
                  files[i] = files[i + 1];
                  buffer[i] = buffer[i + 1];
                  cur[i] = cur[i + 1];
                  base[i] = base[i + 1];
                }
              for (i = 0; i < nfiles; ++i)
                ord[i] = ord[i + 1];
              continue;
            }
        }

       
      {
        size_t lo = 1;
        size_t hi = nfiles;
        size_t probe = lo;
        size_t ord0 = ord[0];
        size_t count_of_smaller_lines;

        while (lo < hi)
          {
            int cmp = compare (cur[ord0], cur[ord[probe]]);
            if (cmp < 0 || (cmp == 0 && ord0 < ord[probe]))
              hi = probe;
            else
              lo = probe + 1;
            probe = (lo + hi) / 2;
          }

        count_of_smaller_lines = lo - 1;
        for (j = 0; j < count_of_smaller_lines; j++)
          ord[j] = ord[j + 1];
        ord[count_of_smaller_lines] = ord0;
      }
    }

  if (unique && savedline)
    {
      write_line (&saved, ofp, output_file);
      free (saved.text);
    }

  xfclose (ofp, output_file);
  free (fps);
  free (buffer);
  free (ord);
  free (base);
  free (cur);
}

 

static size_t
mergefiles (struct sortfile *files, size_t ntemps, size_t nfiles,
            FILE *ofp, char const *output_file)
{
  FILE **fps;
  size_t nopened = open_input_files (files, nfiles, &fps);
  if (nopened < nfiles && nopened < 2)
    sort_die (_("open failed"), files[nopened].name);
  mergefps (files, ntemps, nopened, ofp, output_file, fps);
  return nopened;
}

 

static void
mergelines (struct line *restrict t, size_t nlines,
            struct line const *restrict lo)
{
  size_t nlo = nlines / 2;
  size_t nhi = nlines - nlo;
  struct line *hi = t - nlo;

  while (true)
    if (compare (lo - 1, hi - 1) <= 0)
      {
        *--t = *--lo;
        if (! --nlo)
          {
             
            return;
          }
      }
    else
      {
        *--t = *--hi;
        if (! --nhi)
          {
            do
              *--t = *--lo;
            while (--nlo);

            return;
          }
      }
}

 

static void
sequential_sort (struct line *restrict lines, size_t nlines,
                 struct line *restrict temp, bool to_temp)
{
  if (nlines == 2)
    {
       
      int swap = (0 < compare (&lines[-1], &lines[-2]));
      if (to_temp)
        {
          temp[-1] = lines[-1 - swap];
          temp[-2] = lines[-2 + swap];
        }
      else if (swap)
        {
          temp[-1] = lines[-1];
          lines[-1] = lines[-2];
          lines[-2] = temp[-1];
        }
    }
  else
    {
      size_t nlo = nlines / 2;
      size_t nhi = nlines - nlo;
      struct line *lo = lines;
      struct line *hi = lines - nlo;

      sequential_sort (hi, nhi, temp - (to_temp ? nlo : 0), to_temp);
      if (1 < nlo)
        sequential_sort (lo, nlo, temp, !to_temp);
      else if (!to_temp)
        temp[-1] = lo[-1];

      struct line *dest;
      struct line const *sorted_lo;
      if (to_temp)
        {
          dest = temp;
          sorted_lo = lines;
        }
      else
        {
          dest = lines;
          sorted_lo = temp;
        }
      mergelines (dest, nlines, sorted_lo);
    }
}

static struct merge_node *init_node (struct merge_node *restrict,
                                     struct merge_node *restrict,
                                     struct line *, size_t, size_t, bool);


 
static struct merge_node *
merge_tree_init (size_t nthreads, size_t nlines, struct line *dest)
{
  struct merge_node *merge_tree = xmalloc (2 * sizeof *merge_tree * nthreads);

  struct merge_node *root = merge_tree;
  root->lo = root->hi = root->end_lo = root->end_hi = nullptr;
  root->dest = nullptr;
  root->nlo = root->nhi = nlines;
  root->parent = nullptr;
  root->level = MERGE_END;
  root->queued = false;
  pthread_mutex_init (&root->lock, nullptr);

  init_node (root, root + 1, dest, nthreads, nlines, false);
  return merge_tree;
}

 
static void
merge_tree_destroy (size_t nthreads, struct merge_node *merge_tree)
{
  size_t n_nodes = nthreads * 2;
  struct merge_node *node = merge_tree;

  while (n_nodes--)
    {
      pthread_mutex_destroy (&node->lock);
      node++;
    }

  free (merge_tree);
}

 

static struct merge_node *
init_node (struct merge_node *restrict parent,
           struct merge_node *restrict node_pool,
           struct line *dest, size_t nthreads,
           size_t total_lines, bool is_lo_child)
{
  size_t nlines = (is_lo_child ? parent->nlo : parent->nhi);
  size_t nlo = nlines / 2;
  size_t nhi = nlines - nlo;
  struct line *lo = dest - total_lines;
  struct line *hi = lo - nlo;
  struct line **parent_end = (is_lo_child ? &parent->end_lo : &parent->end_hi);

  struct merge_node *node = node_pool++;
  node->lo = node->end_lo = lo;
  node->hi = node->end_hi = hi;
  node->dest = parent_end;
  node->nlo = nlo;
  node->nhi = nhi;
  node->parent = parent;
  node->level = parent->level + 1;
  node->queued = false;
  pthread_mutex_init (&node->lock, nullptr);

  if (nthreads > 1)
    {
      size_t lo_threads = nthreads / 2;
      size_t hi_threads = nthreads - lo_threads;
      node->lo_child = node_pool;
      node_pool = init_node (node, node_pool, lo, lo_threads,
                             total_lines, true);
      node->hi_child = node_pool;
      node_pool = init_node (node, node_pool, hi, hi_threads,
                             total_lines, false);
    }
  else
    {
      node->lo_child = nullptr;
      node->hi_child = nullptr;
    }
  return node_pool;
}


 

static int
compare_nodes (void const *a, void const *b)
{
  struct merge_node const *nodea = a;
  struct merge_node const *nodeb = b;
  if (nodea->level == nodeb->level)
      return (nodea->nlo + nodea->nhi) < (nodeb->nlo + nodeb->nhi);
  return nodea->level < nodeb->level;
}

 

static inline void
lock_node (struct merge_node *node)
{
  pthread_mutex_lock (&node->lock);
}

 

static inline void
unlock_node (struct merge_node *node)
{
  pthread_mutex_unlock (&node->lock);
}

 

static void
queue_destroy (struct merge_node_queue *queue)
{
  heap_free (queue->priority_queue);
  pthread_cond_destroy (&queue->cond);
  pthread_mutex_destroy (&queue->mutex);
}

 

static void
queue_init (struct merge_node_queue *queue, size_t nthreads)
{
   
  queue->priority_queue = heap_alloc (compare_nodes, 2 * nthreads);
  pthread_mutex_init (&queue->mutex, nullptr);
  pthread_cond_init (&queue->cond, nullptr);
}

 

static void
queue_insert (struct merge_node_queue *queue, struct merge_node *node)
{
  pthread_mutex_lock (&queue->mutex);
  heap_insert (queue->priority_queue, node);
  node->queued = true;
  pthread_cond_signal (&queue->cond);
  pthread_mutex_unlock (&queue->mutex);
}

 

static struct merge_node *
queue_pop (struct merge_node_queue *queue)
{
  struct merge_node *node;
  pthread_mutex_lock (&queue->mutex);
  while (! (node = heap_remove_top (queue->priority_queue)))
    pthread_cond_wait (&queue->cond, &queue->mutex);
  pthread_mutex_unlock (&queue->mutex);
  lock_node (node);
  node->queued = false;
  return node;
}

 

static void
write_unique (struct line const *line, FILE *tfp, char const *temp_output)
{
  if (unique)
    {
      if (saved_line.text && ! compare (line, &saved_line))
        return;
      saved_line = *line;
    }

  write_line (line, tfp, temp_output);
}

 

static void
mergelines_node (struct merge_node *restrict node, size_t total_lines,
                 FILE *tfp, char const *temp_output)
{
  struct line *lo_orig = node->lo;
  struct line *hi_orig = node->hi;
  size_t to_merge = MAX_MERGE (total_lines, node->level);
  size_t merged_lo;
  size_t merged_hi;

  if (node->level > MERGE_ROOT)
    {
       
      struct line *dest = *node->dest;
      while (node->lo != node->end_lo && node->hi != node->end_hi && to_merge--)
        if (compare (node->lo - 1, node->hi - 1) <= 0)
          *--dest = *--node->lo;
        else
          *--dest = *--node->hi;

      merged_lo = lo_orig - node->lo;
      merged_hi = hi_orig - node->hi;

      if (node->nhi == merged_hi)
        while (node->lo != node->end_lo && to_merge--)
          *--dest = *--node->lo;
      else if (node->nlo == merged_lo)
        while (node->hi != node->end_hi && to_merge--)
          *--dest = *--node->hi;
      *node->dest = dest;
    }
  else
    {
       
      while (node->lo != node->end_lo && node->hi != node->end_hi && to_merge--)
        {
          if (compare (node->lo - 1, node->hi - 1) <= 0)
            write_unique (--node->lo, tfp, temp_output);
          else
            write_unique (--node->hi, tfp, temp_output);
        }

      merged_lo = lo_orig - node->lo;
      merged_hi = hi_orig - node->hi;

      if (node->nhi == merged_hi)
        {
          while (node->lo != node->end_lo && to_merge--)
            write_unique (--node->lo, tfp, temp_output);
        }
      else if (node->nlo == merged_lo)
        {
          while (node->hi != node->end_hi && to_merge--)
            write_unique (--node->hi, tfp, temp_output);
        }
    }

   
  merged_lo = lo_orig - node->lo;
  merged_hi = hi_orig - node->hi;
  node->nlo -= merged_lo;
  node->nhi -= merged_hi;
}

 

static void
queue_check_insert (struct merge_node_queue *queue, struct merge_node *node)
{
  if (! node->queued)
    {
      bool lo_avail = (node->lo - node->end_lo) != 0;
      bool hi_avail = (node->hi - node->end_hi) != 0;
      if (lo_avail ? hi_avail || ! node->nhi : hi_avail && ! node->nlo)
        queue_insert (queue, node);
    }
}

 

static void
queue_check_insert_parent (struct merge_node_queue *queue,
                           struct merge_node *node)
{
  if (node->level > MERGE_ROOT)
    {
      lock_node (node->parent);
      queue_check_insert (queue, node->parent);
      unlock_node (node->parent);
    }
  else if (node->nlo + node->nhi == 0)
    {
       
      queue_insert (queue, node->parent);
    }
}

 

static void
merge_loop (struct merge_node_queue *queue,
            size_t total_lines, FILE *tfp, char const *temp_output)
{
  while (true)
    {
      struct merge_node *node = queue_pop (queue);

      if (node->level == MERGE_END)
        {
          unlock_node (node);
           
          queue_insert (queue, node);
          break;
        }
      mergelines_node (node, total_lines, tfp, temp_output);
      queue_check_insert (queue, node);
      queue_check_insert_parent (queue, node);

      unlock_node (node);
    }
}


static void sortlines (struct line *restrict, size_t, size_t,
                       struct merge_node *, struct merge_node_queue *,
                       FILE *, char const *);

 

struct thread_args
{
   
  struct line *lines;

   
  size_t nthreads;

   
  size_t const total_lines;

   
  struct merge_node *const node;

   
  struct merge_node_queue *const queue;

   
  FILE *tfp;
  char const *output_temp;
};

 

static void *
sortlines_thread (void *data)
{
  struct thread_args const *args = data;
  sortlines (args->lines, args->nthreads, args->total_lines,
             args->node, args->queue, args->tfp,
             args->output_temp);
  return nullptr;
}

 

static void
sortlines (struct line *restrict lines, size_t nthreads,
           size_t total_lines, struct merge_node *node,
           struct merge_node_queue *queue, FILE *tfp, char const *temp_output)
{
  size_t nlines = node->nlo + node->nhi;

   
  size_t lo_threads = nthreads / 2;
  size_t hi_threads = nthreads - lo_threads;
  pthread_t thread;
  struct thread_args args = {lines, lo_threads, total_lines,
                             node->lo_child, queue, tfp, temp_output};

  if (nthreads > 1 && SUBTHREAD_LINES_HEURISTIC <= nlines
      && pthread_create (&thread, nullptr, sortlines_thread, &args) == 0)
    {
      sortlines (lines - node->nlo, hi_threads, total_lines,
                 node->hi_child, queue, tfp, temp_output);
      pthread_join (thread, nullptr);
    }
  else
    {
       
      size_t nlo = node->nlo;
      size_t nhi = node->nhi;
      struct line *temp = lines - total_lines;
      if (1 < nhi)
        sequential_sort (lines - nlo, nhi, temp - nlo / 2, false);
      if (1 < nlo)
        sequential_sort (lines, nlo, temp, false);

       
      node->lo = lines;
      node->hi = lines - nlo;
      node->end_lo = lines - nlo;
      node->end_hi = lines - nlo - nhi;

      queue_insert (queue, node);
      merge_loop (queue, total_lines, tfp, temp_output);
    }
}

 

static void
avoid_trashing_input (struct sortfile *files, size_t ntemps,
                      size_t nfiles, char const *outfile)
{
  struct tempnode *tempcopy = nullptr;

  for (size_t i = ntemps; i < nfiles; i++)
    {
      bool is_stdin = STREQ (files[i].name, "-");
      bool same;
      struct stat instat;

      if (outfile && STREQ (outfile, files[i].name) && !is_stdin)
        same = true;
      else
        {
          struct stat *outst = get_outstatus ();
          if (!outst)
            break;

          same = (((is_stdin
                    ? fstat (STDIN_FILENO, &instat)
                    : stat (files[i].name, &instat))
                   == 0)
                  && SAME_INODE (instat, *outst));
        }

      if (same)
        {
          if (! tempcopy)
            {
              FILE *tftp;
              tempcopy = create_temp (&tftp);
              mergefiles (&files[i], 0, 1, tftp, tempcopy->name);
            }

          files[i].name = tempcopy->name;
          files[i].temp = tempcopy;
        }
    }
}

 

static void
check_inputs (char *const *files, size_t nfiles)
{
  for (size_t i = 0; i < nfiles; i++)
    {
      if (STREQ (files[i], "-"))
        continue;

      if (euidaccess (files[i], R_OK) != 0)
        sort_die (_("cannot read"), files[i]);
    }
}

 

static void
check_output (char const *outfile)
{
  if (outfile)
    {
      int oflags = O_WRONLY | O_BINARY | O_CLOEXEC | O_CREAT;
      int outfd = open (outfile, oflags, MODE_RW_UGO);
      if (outfd < 0)
        sort_die (_("open failed"), outfile);
      move_fd (outfd, STDOUT_FILENO);
    }
}

 

static void
merge (struct sortfile *files, size_t ntemps, size_t nfiles,
       char const *output_file)
{
  while (nmerge < nfiles)
    {
       
      size_t in;

       
      size_t out;

       
      size_t remainder;

       
      size_t cheap_slots;

       
      for (out = in = 0; nmerge <= nfiles - in; out++)
        {
          FILE *tfp;
          struct tempnode *temp = create_temp (&tfp);
          size_t num_merged = mergefiles (&files[in], MIN (ntemps, nmerge),
                                          nmerge, tfp, temp->name);
          ntemps -= MIN (ntemps, num_merged);
          files[out].name = temp->name;
          files[out].temp = temp;
          in += num_merged;
        }

      remainder = nfiles - in;
      cheap_slots = nmerge - out % nmerge;

      if (cheap_slots < remainder)
        {
           
          size_t nshortmerge = remainder - cheap_slots + 1;
          FILE *tfp;
          struct tempnode *temp = create_temp (&tfp);
          size_t num_merged = mergefiles (&files[in], MIN (ntemps, nshortmerge),
                                          nshortmerge, tfp, temp->name);
          ntemps -= MIN (ntemps, num_merged);
          files[out].name = temp->name;
          files[out++].temp = temp;
          in += num_merged;
        }

       
      memmove (&files[out], &files[in], (nfiles - in) * sizeof *files);
      ntemps += out;
      nfiles -= in - out;
    }

  avoid_trashing_input (files, ntemps, nfiles, output_file);

   

  while (true)
    {
       
      FILE **fps;
      size_t nopened = open_input_files (files, nfiles, &fps);

      if (nopened == nfiles)
        {
          FILE *ofp = stream_open (output_file, "w");
          if (ofp)
            {
              mergefps (files, ntemps, nfiles, ofp, output_file, fps);
              break;
            }
          if (errno != EMFILE || nopened <= 2)
            sort_die (_("open failed"), output_file);
        }
      else if (nopened <= 2)
        sort_die (_("open failed"), files[nopened].name);

       
      FILE *tfp;
      struct tempnode *temp;
      do
        {
          nopened--;
          xfclose (fps[nopened], files[nopened].name);
          temp = maybe_create_temp (&tfp, ! (nopened <= 2));
        }
      while (!temp);

       
      mergefps (&files[0], MIN (ntemps, nopened), nopened, tfp, temp->name,
                fps);
      ntemps -= MIN (ntemps, nopened);
      files[0].name = temp->name;
      files[0].temp = temp;

      memmove (&files[1], &files[nopened], (nfiles - nopened) * sizeof *files);
      ntemps++;
      nfiles -= nopened - 1;
    }
}

 

static void
sort (char *const *files, size_t nfiles, char const *output_file,
      size_t nthreads)
{
  struct buffer buf;
  size_t ntemps = 0;
  bool output_file_created = false;

  buf.alloc = 0;

  while (nfiles)
    {
      char const *temp_output;
      char const *file = *files;
      FILE *fp = xfopen (file, "r");
      FILE *tfp;

      size_t bytes_per_line;
      if (nthreads > 1)
        {
           
          size_t tmp = 1;
          size_t mult = 1;
          while (tmp < nthreads)
            {
              tmp *= 2;
              mult++;
            }
          bytes_per_line = (mult * sizeof (struct line));
        }
      else
        bytes_per_line = sizeof (struct line) * 3 / 2;

      if (! buf.alloc)
        initbuf (&buf, bytes_per_line,
                 sort_buffer_size (&fp, 1, files, nfiles, bytes_per_line));
      buf.eof = false;
      files++;
      nfiles--;

      while (fillbuf (&buf, fp, file))
        {
          struct line *line;

          if (buf.eof && nfiles
              && (bytes_per_line + 1
                  < (buf.alloc - buf.used - bytes_per_line * buf.nlines)))
            {
               
              buf.left = buf.used;
              break;
            }

          saved_line.text = nullptr;
          line = buffer_linelim (&buf);
          if (buf.eof && !nfiles && !ntemps && !buf.left)
            {
              xfclose (fp, file);
              tfp = xfopen (output_file, "w");
              temp_output = output_file;
              output_file_created = true;
            }
          else
            {
              ++ntemps;
              temp_output = create_temp (&tfp)->name;
            }
          if (1 < buf.nlines)
            {
              struct merge_node_queue queue;
              queue_init (&queue, nthreads);
              struct merge_node *merge_tree =
                merge_tree_init (nthreads, buf.nlines, line);

              sortlines (line, nthreads, buf.nlines, merge_tree + 1,
                         &queue, tfp, temp_output);

              merge_tree_destroy (nthreads, merge_tree);
              queue_destroy (&queue);
            }
          else
            write_unique (line - 1, tfp, temp_output);

          xfclose (tfp, temp_output);

          if (output_file_created)
            goto finish;
        }
      xfclose (fp, file);
    }

 finish:
  free (buf.buf);

  if (! output_file_created)
    {
      struct tempnode *node = temphead;
      struct sortfile *tempfiles = xnmalloc (ntemps, sizeof *tempfiles);
      for (size_t i = 0; node; i++)
        {
          tempfiles[i].name = node->name;
          tempfiles[i].temp = node;
          node = node->next;
        }
      merge (tempfiles, ntemps, ntemps, output_file);
      free (tempfiles);
    }

  reap_all ();
}

 

static void
insertkey (struct keyfield *key_arg)
{
  struct keyfield **p;
  struct keyfield *key = xmemdup (key_arg, sizeof *key);

  for (p = &keylist; *p; p = &(*p)->next)
    continue;
  *p = key;
  key->next = nullptr;
}

 

static void
badfieldspec (char const *spec, char const *msgid)
{
  error (SORT_FAILURE, 0, _("%s: invalid field specification %s"),
         _(msgid), quote (spec));
}

 

static void
incompatible_options (char const *opts)
{
  error (SORT_FAILURE, 0, _("options '-%s' are incompatible"), (opts));
}

 

static void
check_ordering_compatibility (void)
{
  struct keyfield *key;

  for (key = keylist; key; key = key->next)
    if (1 < (key->numeric + key->general_numeric + key->human_numeric
             + key->month + (key->version | key->random | !!key->ignore)))
      {
         
        char opts[sizeof short_options];
         
        key->skipsblanks = key->skipeblanks = key->reverse = false;
        key_to_opts (key, opts);
        incompatible_options (opts);
      }
}

 

static char const *
parse_field_count (char const *string, size_t *val, char const *msgid)
{
  char *suffix;
  uintmax_t n;

  switch (xstrtoumax (string, &suffix, 10, &n, ""))
    {
    case LONGINT_OK:
    case LONGINT_INVALID_SUFFIX_CHAR:
      *val = n;
      if (*val == n)
        break;
      FALLTHROUGH;
    case LONGINT_OVERFLOW:
    case LONGINT_OVERFLOW | LONGINT_INVALID_SUFFIX_CHAR:
      *val = SIZE_MAX;
      break;

    case LONGINT_INVALID:
      if (msgid)
        error (SORT_FAILURE, 0, _("%s: invalid count at start of %s"),
               _(msgid), quote (string));
      return nullptr;
    }

  return suffix;
}

 

static void
sighandler (int sig)
{
  if (! SA_NOCLDSTOP)
    signal (sig, SIG_IGN);

  cleanup ();

  signal (sig, SIG_DFL);
  raise (sig);
}

 

static char *
set_ordering (char const *s, struct keyfield *key, enum blanktype blanktype)
{
  while (*s)
    {
      switch (*s)
        {
        case 'b':
          if (blanktype == bl_start || blanktype == bl_both)
            key->skipsblanks = true;
          if (blanktype == bl_end || blanktype == bl_both)
            key->skipeblanks = true;
          break;
        case 'd':
          key->ignore = nondictionary;
          break;
        case 'f':
          key->translate = fold_toupper;
          break;
        case 'g':
          key->general_numeric = true;
          break;
        case 'h':
          key->human_numeric = true;
          break;
        case 'i':
           
          if (! key->ignore)
            key->ignore = nonprinting;
          break;
        case 'M':
          key->month = true;
          break;
        case 'n':
          key->numeric = true;
          break;
        case 'R':
          key->random = true;
          break;
        case 'r':
          key->reverse = true;
          break;
        case 'V':
          key->version = true;
          break;
        default:
          return (char *) s;
        }
      ++s;
    }
  return (char *) s;
}

 

static struct keyfield *
key_init (struct keyfield *key)
{
  memset (key, 0, sizeof *key);
  key->eword = SIZE_MAX;
  return key;
}

int
main (int argc, char **argv)
{
  struct keyfield *key;
  struct keyfield key_buf;
  struct keyfield gkey;
  bool gkey_only = false;
  char const *s;
  int c = 0;
  char checkonly = 0;
  bool mergeonly = false;
  char *random_source = nullptr;
  bool need_random = false;
  size_t nthreads = 0;
  size_t nfiles = 0;
  bool posixly_correct = (getenv ("POSIXLY_CORRECT") != nullptr);
  int posix_ver = posix2_version ();
  bool traditional_usage = ! (200112 <= posix_ver && posix_ver < 200809);
  char **files;
  char *files_from = nullptr;
  struct Tokens tok;
  char const *outfile = nullptr;
  bool locale_ok;

  initialize_main (&argc, &argv);
  set_program_name (argv[0]);
  locale_ok = !! setlocale (LC_ALL, "");
  bindtextdomain (PACKAGE, LOCALEDIR);
  textdomain (PACKAGE);

  initialize_exit_failure (SORT_FAILURE);

  hard_LC_COLLATE = hard_locale (LC_COLLATE);
#if HAVE_NL_LANGINFO
  hard_LC_TIME = hard_locale (LC_TIME);
#endif

   
  {
    struct lconv const *locale = localeconv ();

     
    decimal_point = locale->decimal_point[0];
    if (! decimal_point || locale->decimal_point[1])
      decimal_point = '.';

     
    thousands_sep = locale->thousands_sep[0];
    if (thousands_sep && locale->thousands_sep[1])
      thousands_sep_ignored = true;
    if (! thousands_sep || locale->thousands_sep[1])
      thousands_sep = NON_CHAR;
  }

  have_read_stdin = false;
  inittables ();

  {
    size_t i;
    static int const sig[] =
      {
         
        SIGALRM, SIGHUP, SIGINT, SIGPIPE, SIGQUIT, SIGTERM,
#ifdef SIGPOLL
        SIGPOLL,
#endif
#ifdef SIGPROF
        SIGPROF,
#endif
#ifdef SIGVTALRM
        SIGVTALRM,
#endif
#ifdef SIGXCPU
        SIGXCPU,
#endif
#ifdef SIGXFSZ
        SIGXFSZ,
#endif
      };
    enum { nsigs = ARRAY_CARDINALITY (sig) };

#if SA_NOCLDSTOP
    struct sigaction act;

    sigemptyset (&caught_signals);
    for (i = 0; i < nsigs; i++)
      {
        sigaction (sig[i], nullptr, &act);
        if (act.sa_handler != SIG_IGN)
          sigaddset (&caught_signals, sig[i]);
      }

    act.sa_handler = sighandler;
    act.sa_mask = caught_signals;
    act.sa_flags = 0;

    for (i = 0; i < nsigs; i++)
      if (sigismember (&caught_signals, sig[i]))
        sigaction (sig[i], &act, nullptr);
#else
    for (i = 0; i < nsigs; i++)
      if (signal (sig[i], SIG_IGN) != SIG_IGN)
        {
          signal (sig[i], sighandler);
          siginterrupt (sig[i], 1);
        }
#endif
  }
  signal (SIGCHLD, SIG_DFL);  

   
  atexit (exit_cleanup);

  key_init (&gkey);
  gkey.sword = SIZE_MAX;

  files = xnmalloc (argc, sizeof *files);

  while (true)
    {
       
      int oi = -1;

      if (c == -1
          || (posixly_correct && nfiles != 0
              && ! (traditional_usage
                    && ! checkonly
                    && optind != argc
                    && argv[optind][0] == '-' && argv[optind][1] == 'o'
                    && (argv[optind][2] || optind + 1 != argc)))
          || ((c = getopt_long (argc, argv, short_options,
                                long_options, &oi))
              == -1))
        {
          if (argc <= optind)
            break;
          files[nfiles++] = argv[optind++];
        }
      else switch (c)
        {
        case 1:
          key = nullptr;
          if (optarg[0] == '+')
            {
              bool minus_pos_usage = (optind != argc && argv[optind][0] == '-'
                                      && ISDIGIT (argv[optind][1]));
              traditional_usage |= minus_pos_usage && !posixly_correct;
              if (traditional_usage)
                {
                   
                  key = key_init (&key_buf);
                  s = parse_field_count (optarg + 1, &key->sword, nullptr);
                  if (s && *s == '.')
                    s = parse_field_count (s + 1, &key->schar, nullptr);
                  if (! (key->sword || key->schar))
                    key->sword = SIZE_MAX;
                  if (! s || *set_ordering (s, key, bl_start))
                    key = nullptr;
                  else
                    {
                      if (minus_pos_usage)
                        {
                          char const *optarg1 = argv[optind++];
                          s = parse_field_count (optarg1 + 1, &key->eword,
                                             N_("invalid number after '-'"));
                          if (*s == '.')
                            s = parse_field_count (s + 1, &key->echar,
                                               N_("invalid number after '.'"));
                          if (!key->echar && key->eword)
                            {
                               
                              key->eword--;
                            }
                          if (*set_ordering (s, key, bl_end))
                            badfieldspec (optarg1,
                                      N_("stray character in field spec"));
                        }
                      key->traditional_used = true;
                      insertkey (key);
                    }
                }
            }
          if (! key)
            files[nfiles++] = optarg;
          break;

        case SORT_OPTION:
          c = XARGMATCH ("--sort", optarg, sort_args, sort_types);
          FALLTHROUGH;
        case 'b':
        case 'd':
        case 'f':
        case 'g':
        case 'h':
        case 'i':
        case 'M':
        case 'n':
        case 'r':
        case 'R':
        case 'V':
          {
            char str[2];
            str[0] = c;
            str[1] = '\0';
            set_ordering (str, &gkey, bl_both);
          }
          break;

        case CHECK_OPTION:
          c = (optarg
               ? XARGMATCH ("--check", optarg, check_args, check_types)
               : 'c');
          FALLTHROUGH;
        case 'c':
        case 'C':
          if (checkonly && checkonly != c)
            incompatible_options ("cC");
          checkonly = c;
          break;

        case COMPRESS_PROGRAM_OPTION:
          if (compress_program && !STREQ (compress_program, optarg))
            error (SORT_FAILURE, 0, _("multiple compress programs specified"));
          compress_program = optarg;
          break;

        case DEBUG_PROGRAM_OPTION:
          debug = true;
          break;

        case FILES0_FROM_OPTION:
          files_from = optarg;
          break;

        case 'k':
          key = key_init (&key_buf);

           
          s = parse_field_count (optarg, &key->sword,
                                 N_("invalid number at field start"));
          if (! key->sword--)
            {
               
              badfieldspec (optarg, N_("field number is zero"));
            }
          if (*s == '.')
            {
              s = parse_field_count (s + 1, &key->schar,
                                     N_("invalid number after '.'"));
              if (! key->schar--)
                {
                   
                  badfieldspec (optarg, N_("character offset is zero"));
                }
            }
          if (! (key->sword || key->schar))
            key->sword = SIZE_MAX;
          s = set_ordering (s, key, bl_start);
          if (*s != ',')
            {
              key->eword = SIZE_MAX;
              key->echar = 0;
            }
          else
            {
               
              s = parse_field_count (s + 1, &key->eword,
                                     N_("invalid number after ','"));
              if (! key->eword--)
                {
                   
                  badfieldspec (optarg, N_("field number is zero"));
                }
              if (*s == '.')
                {
                  s = parse_field_count (s + 1, &key->echar,
                                         N_("invalid number after '.'"));
                }
              s = set_ordering (s, key, bl_end);
            }
          if (*s)
            badfieldspec (optarg, N_("stray character in field spec"));
          insertkey (key);
          break;

        case 'm':
          mergeonly = true;
          break;

        case NMERGE_OPTION:
          specify_nmerge (oi, c, optarg);
          break;

        case 'o':
          if (outfile && !STREQ (outfile, optarg))
            error (SORT_FAILURE, 0, _("multiple output files specified"));
          outfile = optarg;
          break;

        case RANDOM_SOURCE_OPTION:
          if (random_source && !STREQ (random_source, optarg))
            error (SORT_FAILURE, 0, _("multiple random sources specified"));
          random_source = optarg;
          break;

        case 's':
          stable = true;
          break;

        case 'S':
          specify_sort_size (oi, c, optarg);
          break;

        case 't':
          {
            char newtab = optarg[0];
            if (! newtab)
              error (SORT_FAILURE, 0, _("empty tab"));
            if (optarg[1])
              {
                if (STREQ (optarg, "\\0"))
                  newtab = '\0';
                else
                  {
                     
                    error (SORT_FAILURE, 0, _("multi-character tab %s"),
                           quote (optarg));
                  }
              }
            if (tab != TAB_DEFAULT && tab != newtab)
              error (SORT_FAILURE, 0, _("incompatible tabs"));
            tab = newtab;
          }
          break;

        case 'T':
          add_temp_dir (optarg);
          break;

        case PARALLEL_OPTION:
          nthreads = specify_nthreads (oi, c, optarg);
          break;

        case 'u':
          unique = true;
          break;

        case 'y':
           
          if (optarg == argv[optind - 1])
            {
              char const *p;
              for (p = optarg; ISDIGIT (*p); p++)
                continue;
              optind -= (*p != '\0');
            }
          break;

        case 'z':
          eolchar = 0;
          break;

        case_GETOPT_HELP_CHAR;

        case_GETOPT_VERSION_CHAR (PROGRAM_NAME, AUTHORS);

        default:
          usage (SORT_FAILURE);
        }
    }

  if (files_from)
    {
       
      if (nfiles)
        {
          error (0, 0, _("extra operand %s"), quoteaf (files[0]));
          fprintf (stderr, "%s\n",
                   _("file operands cannot be combined with --files0-from"));
          usage (SORT_FAILURE);
        }

      FILE *stream = xfopen (files_from, "r");

      readtokens0_init (&tok);

      if (! readtokens0 (stream, &tok))
        error (SORT_FAILURE, 0, _("cannot read file names from %s"),
               quoteaf (files_from));
      xfclose (stream, files_from);

      if (tok.n_tok)
        {
          free (files);
          files = tok.tok;
          nfiles = tok.n_tok;
          for (size_t i = 0; i < nfiles; i++)
            {
              if (STREQ (files[i], "-"))
                error (SORT_FAILURE, 0, _("when reading file names from stdin, "
                                          "no file name of %s allowed"),
                       quoteaf (files[i]));
              else if (files[i][0] == '\0')
                {
                   
                  unsigned long int file_number = i + 1;
                  error (SORT_FAILURE, 0,
                         _("%s:%lu: invalid zero-length file name"),
                         quotef (files_from), file_number);
                }
            }
        }
      else
        error (SORT_FAILURE, 0, _("no input from %s"),
               quoteaf (files_from));
    }

   
  for (key = keylist; key; key = key->next)
    {
      if (default_key_compare (key) && !key->reverse)
        {
          key->ignore = gkey.ignore;
          key->translate = gkey.translate;
          key->skipsblanks = gkey.skipsblanks;
          key->skipeblanks = gkey.skipeblanks;
          key->month = gkey.month;
          key->numeric = gkey.numeric;
          key->general_numeric = gkey.general_numeric;
          key->human_numeric = gkey.human_numeric;
          key->version = gkey.version;
          key->random = gkey.random;
          key->reverse = gkey.reverse;
        }

      need_random |= key->random;
    }

  if (!keylist && !default_key_compare (&gkey))
    {
      gkey_only = true;
      insertkey (&gkey);
      need_random |= gkey.random;
    }

  check_ordering_compatibility ();

  if (debug)
    {
      if (checkonly || outfile)
        {
          static char opts[] = "X --debug";
          opts[0] = (checkonly ? checkonly : 'o');
          incompatible_options (opts);
        }

       

       
      if (locale_ok)
        locale_ok = !! setlocale (LC_COLLATE, "");
      if (! locale_ok)
          error (0, 0, "%s", _("failed to set locale"));
      if (hard_LC_COLLATE)
        error (0, 0, _("text ordering performed using %s sorting rules"),
               quote (setlocale (LC_COLLATE, nullptr)));
      else
        error (0, 0, "%s",
               _("text ordering performed using simple byte comparison"));

      key_warnings (&gkey, gkey_only);
    }

  reverse = gkey.reverse;

  if (need_random)
    random_md5_state_init (random_source);

  if (temp_dir_count == 0)
    {
      char const *tmp_dir = getenv ("TMPDIR");
      add_temp_dir (tmp_dir ? tmp_dir : DEFAULT_TMPDIR);
    }

  if (nfiles == 0)
    {
      nfiles = 1;
      free (files);
      files = xmalloc (sizeof *files);
      *files = (char *) "-";
    }

   
  if (0 < sort_size)
    sort_size = MAX (sort_size, MIN_SORT_SIZE);

  if (checkonly)
    {
      if (nfiles > 1)
        error (SORT_FAILURE, 0, _("extra operand %s not allowed with -%c"),
               quoteaf (files[1]), checkonly);

      if (outfile)
        {
          static char opts[] = {0, 'o', 0};
          opts[0] = checkonly;
          incompatible_options (opts);
        }

       
      exit (check (files[0], checkonly) ? EXIT_SUCCESS : SORT_OUT_OF_ORDER);
    }

   
  check_inputs (files, nfiles);

   
  check_output (outfile);

  if (mergeonly)
    {
      struct sortfile *sortfiles = xcalloc (nfiles, sizeof *sortfiles);

      for (size_t i = 0; i < nfiles; ++i)
        sortfiles[i].name = files[i];

      merge (sortfiles, 0, nfiles, outfile);
    }
  else
    {
      if (!nthreads)
        {
          unsigned long int np = num_processors (NPROC_CURRENT_OVERRIDABLE);
          nthreads = MIN (np, DEFAULT_MAX_THREADS);
        }

       
      size_t nthreads_max = SIZE_MAX / (2 * sizeof (struct merge_node));
      nthreads = MIN (nthreads, nthreads_max);

      sort (files, nfiles, outfile, nthreads);
    }

  if (have_read_stdin && fclose (stdin) == EOF)
    sort_die (_("close failed"), "-");

  main_exit (EXIT_SUCCESS);
}
