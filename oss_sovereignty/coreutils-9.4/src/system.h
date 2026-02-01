 

#include <attribute.h>

#include <alloca.h>

#include <sys/stat.h>

 
#define MODE_RW_UGO (S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH | S_IWOTH)

#if HAVE_SYS_PARAM_H
# include <sys/param.h>
#endif

#include <unistd.h>

#include <limits.h>

#include "pathmax.h"
#ifndef PATH_MAX
# define PATH_MAX 8192
#endif

#include "configmake.h"

#include <sys/time.h>
#include <time.h>

 
#if MAJOR_IN_MKDEV
# include <sys/mkdev.h>
# define HAVE_MAJOR
#endif
#if MAJOR_IN_SYSMACROS
# include <sys/sysmacros.h>
# define HAVE_MAJOR
#endif
#ifdef major			 
# define HAVE_MAJOR
#endif

#ifndef HAVE_MAJOR
# define major(dev)  (((dev) >> 8) & 0xff)
# define minor(dev)  ((dev) & 0xff)
# define makedev(maj, min)  (((maj) << 8) | (min))
#endif
#undef HAVE_MAJOR

#if ! defined makedev && defined mkdev
# define makedev(maj, min)  mkdev (maj, min)
#endif

#include <stddef.h>
#include <string.h>
#include <errno.h>

 
#ifndef ENODATA
# define ENODATA (-1)
#endif

#include <stdlib.h>
#include "version.h"

 
enum
{
  EXIT_TIMEDOUT = 124,  
  EXIT_CANCELED = 125,  
  EXIT_CANNOT_INVOKE = 126,  
  EXIT_ENOENT = 127  
};

#include "exitfail.h"

 
static inline void
initialize_exit_failure (int status)
{
  if (status != EXIT_FAILURE)
    exit_failure = status;
}

#include <fcntl.h>
#ifdef O_PATH
enum { O_PATHSEARCH = O_PATH };
#else
enum { O_PATHSEARCH = O_SEARCH };
#endif

#include <dirent.h>
#ifndef _D_EXACT_NAMLEN
# define _D_EXACT_NAMLEN(dp) strlen ((dp)->d_name)
#endif

enum
{
  NOT_AN_INODE_NUMBER = 0
};

#ifdef D_INO_IN_DIRENT
# define D_INO(dp) (dp)->d_ino
#else
 
# define D_INO(dp) NOT_AN_INODE_NUMBER
#endif

 
#include <inttypes.h>

 
#ifndef initialize_main
# ifndef __OS2__
#  define initialize_main(ac, av)
# else
#  define initialize_main(ac, av) \
     do { _wildcard (ac, av); _response (ac, av); } while (0)
# endif
#endif

#include "stat-macros.h"

#include "timespec.h"

#include <ctype.h>

 
#define ISDIGIT(c) ((unsigned int) (c) - '0' <= 9)

 
static inline unsigned char to_uchar (char ch) { return ch; }

 
static inline bool
field_sep (unsigned char ch)
{
  return isblank (ch) || ch == '\n';
}

#include <locale.h>

 

#include "gettext.h"
#if ! ENABLE_NLS
# undef textdomain
# define textdomain(Domainname)  
# undef bindtextdomain
# define bindtextdomain(Domainname, Dirname)  
#endif

#define _(msgid) gettext (msgid)
#define N_(msgid) msgid

 
static inline unsigned long int
select_plural (uintmax_t n)
{
   
  enum { PLURAL_REDUCER = 1000000 };
  return (n <= ULONG_MAX ? n : n % PLURAL_REDUCER + PLURAL_REDUCER);
}

#define STREQ(a, b) (strcmp (a, b) == 0)
#define STREQ_LEN(a, b, n) (strncmp (a, b, n) == 0)  
#define STRPREFIX(a, b) (strncmp (a, b, strlen (b)) == 0)

 
#define STRNCMP_LIT(s, lit) strncmp (s, "" lit "", sizeof (lit) - 1)

#if !HAVE_DECL_GETLOGIN
char *getlogin (void);
#endif

#if !HAVE_DECL_TTYNAME
char *ttyname (int);
#endif

#if !HAVE_DECL_GETEUID
uid_t geteuid (void);
#endif

#if !HAVE_DECL_GETPWUID
struct passwd *getpwuid (uid_t);
#endif

#if !HAVE_DECL_GETGRGID
struct group *getgrgid (gid_t);
#endif

 
 
#if ! HAVE_SETGROUPS
# if HAVE_GETGRGID_NOMEMBERS
#  define getgrgid(gid) getgrgid_nomembers(gid)
# endif
# if HAVE_GETGRNAM_NOMEMBERS
#  define getgrnam(nam) getgrnam_nomembers(nam)
# endif
# if HAVE_GETGRENT_NOMEMBERS
#  define getgrent() getgrent_nomembers()
# endif
#endif

#if !HAVE_DECL_GETUID
uid_t getuid (void);
#endif

#include "idx.h"
#include "xalloc.h"
#include "verify.h"

 
#define X2NREALLOC(P, PN) verify_expr (sizeof *(P) != 1, \
                                       x2nrealloc (P, PN, sizeof *(P)))

 
#define X2REALLOC(P, PN) verify_expr (sizeof *(P) == 1, \
                                      x2realloc (P, PN))

#include "unlocked-io.h"
#include "same-inode.h"

#include "dirname.h"
#include "openat.h"

static inline bool
dot_or_dotdot (char const *file_name)
{
  if (file_name[0] == '.')
    {
      char sep = file_name[(file_name[1] == '.') + 1];
      return (! sep || ISSLASH (sep));
    }
  else
    return false;
}

 
static inline struct dirent const *
readdir_ignoring_dot_and_dotdot (DIR *dirp)
{
  while (true)
    {
      struct dirent const *dp = readdir (dirp);
      if (dp == nullptr || ! dot_or_dotdot (dp->d_name))
        return dp;
    }
}

 
enum {
    DS_UNKNOWN = -2,
    DS_EMPTY = -1,
    DS_NONEMPTY = 0,
};
static inline int
directory_status (int fd_cwd, char const *dir)
{
  DIR *dirp;
  bool no_direntries;
  int saved_errno;
  int fd = openat (fd_cwd, dir,
                   (O_RDONLY | O_DIRECTORY
                    | O_NOCTTY | O_NOFOLLOW | O_NONBLOCK));

  if (fd < 0)
    return errno;

  dirp = fdopendir (fd);
  if (dirp == nullptr)
    {
      saved_errno = errno;
      close (fd);
      return saved_errno;
    }

  errno = 0;
  no_direntries = !readdir_ignoring_dot_and_dotdot (dirp);
  saved_errno = errno;
  closedir (dirp);
  return no_direntries && saved_errno == 0 ? DS_EMPTY : saved_errno;
}

 

 
enum
{
  GETOPT_HELP_CHAR = (CHAR_MIN - 2),
  GETOPT_VERSION_CHAR = (CHAR_MIN - 3)
};

#define GETOPT_HELP_OPTION_DECL \
  "help", no_argument, nullptr, GETOPT_HELP_CHAR
#define GETOPT_VERSION_OPTION_DECL \
  "version", no_argument, nullptr, GETOPT_VERSION_CHAR
#define GETOPT_SELINUX_CONTEXT_OPTION_DECL \
  "context", optional_argument, nullptr, 'Z'

#define case_GETOPT_HELP_CHAR			\
  case GETOPT_HELP_CHAR:			\
    usage (EXIT_SUCCESS);			\
    break;

 
#define USAGE_BUILTIN_WARNING \
  _("\n" \
"NOTE: your shell may have its own version of %s, which usually supersedes\n" \
"the version described here.  Please refer to your shell's documentation\n" \
"for details about the options it supports.\n")

#define HELP_OPTION_DESCRIPTION \
  _("      --help        display this help and exit\n")
#define VERSION_OPTION_DESCRIPTION \
  _("      --version     output version information and exit\n")

#include "closein.h"
#include "closeout.h"

#define emit_bug_reporting_address unused__emit_bug_reporting_address
#include "version-etc.h"
#undef emit_bug_reporting_address

#include "propername.h"
 
#define proper_name(x) proper_name_lite (x, x)

#include "progname.h"

#define case_GETOPT_VERSION_CHAR(Program_name, Authors)			\
  case GETOPT_VERSION_CHAR:						\
    version_etc (stdout, Program_name, PACKAGE_NAME, Version, Authors,	\
                 (char *) nullptr);					\
    exit (EXIT_SUCCESS);						\
    break;

#include "minmax.h"
#include "intprops.h"

#ifndef SSIZE_MAX
# define SSIZE_MAX TYPE_MAXIMUM (ssize_t)
#endif

#ifndef OFF_T_MIN
# define OFF_T_MIN TYPE_MINIMUM (off_t)
#endif

#ifndef OFF_T_MAX
# define OFF_T_MAX TYPE_MAXIMUM (off_t)
#endif

#ifndef UID_T_MAX
# define UID_T_MAX TYPE_MAXIMUM (uid_t)
#endif

#ifndef GID_T_MAX
# define GID_T_MAX TYPE_MAXIMUM (gid_t)
#endif

#ifndef PID_T_MAX
# define PID_T_MAX TYPE_MAXIMUM (pid_t)
#endif

 
#ifdef lint
# define IF_LINT(Code) Code
#else
# define IF_LINT(Code)  
#endif

 
#ifdef lint
# define main_exit(status) exit (status)
#else
# define main_exit(status) return status
#endif

#ifdef __GNUC__
# define LIKELY(cond)    __builtin_expect ((cond), 1)
# define UNLIKELY(cond)  __builtin_expect ((cond), 0)
#else
# define LIKELY(cond)    (cond)
# define UNLIKELY(cond)  (cond)
#endif


#if defined strdupa
# define ASSIGN_STRDUPA(DEST, S)		\
  do { DEST = strdupa (S); } while (0)
#else
# define ASSIGN_STRDUPA(DEST, S)		\
  do						\
    {						\
      char const *s_ = (S);			\
      size_t len_ = strlen (s_) + 1;		\
      char *tmp_dest_ = alloca (len_);		\
      DEST = memcpy (tmp_dest_, s_, len_);	\
    }						\
  while (0)
#endif

#if ! HAVE_SYNC
# define sync()  
#endif

 

ATTRIBUTE_CONST
static inline size_t
gcd (size_t u, size_t v)
{
  do
    {
      size_t t = u % v;
      u = v;
      v = t;
    }
  while (v);

  return u;
}

 

ATTRIBUTE_CONST
static inline size_t
lcm (size_t u, size_t v)
{
  return u * (v / gcd (u, v));
}

 

static inline void *
ptr_align (void const *ptr, size_t alignment)
{
  char const *p0 = ptr;
  char const *p1 = p0 + alignment - 1;
  return (void *) (p1 - (size_t) p1 % alignment);
}

 

ATTRIBUTE_PURE
static inline bool
is_nul (void const *buf, size_t length)
{
  const unsigned char *p = buf;
 
#if 0 && (_STRING_ARCH_unaligned || _STRING_INLINE_unaligned)
  unsigned long word;
#else
  unsigned char word;
#endif

  if (! length)
      return true;

   
  while (UNLIKELY (length & (sizeof word - 1)))
    {
      if (*p)
        return false;
      p++;
      length--;
      if (! length)
        return true;
   }

   
  for (;;)
    {
      memcpy (&word, p, sizeof word);
      if (word)
        return false;
      p += sizeof word;
      length -= sizeof word;
      if (! length)
        return true;
      if (UNLIKELY (length & 15) == 0)
        break;
   }

    
   return memcmp (buf, p, length) == 0;
}

 

#define DECIMAL_DIGIT_ACCUMULATE(Accum, Digit_val, Type)		\
  (									\
   (void) (&(Accum) == (Type *) nullptr),   	\
   verify_expr (! TYPE_SIGNED (Type),        \
                (((Type) -1 / 10 < (Accum)                              \
                  || (Type) ((Accum) * 10 + (Digit_val)) < (Accum))     \
                 ? false                                                \
                 : (((Accum) = (Accum) * 10 + (Digit_val)), true)))     \
  )

static inline void
emit_stdin_note (void)
{
  fputs (_("\n\
With no FILE, or when FILE is -, read standard input.\n\
"), stdout);
}
static inline void
emit_mandatory_arg_note (void)
{
  fputs (_("\n\
Mandatory arguments to long options are mandatory for short options too.\n\
"), stdout);
}

static inline void
emit_size_note (void)
{
  fputs (_("\n\
The SIZE argument is an integer and optional unit (example: 10K is 10*1024).\n\
Units are K,M,G,T,P,E,Z,Y,R,Q (powers of 1024) or KB,MB,... (powers of 1000).\n\
Binary prefixes can be used, too: KiB=K, MiB=M, and so on.\n\
"), stdout);
}

static inline void
emit_blocksize_note (char const *program)
{
  printf (_("\n\
Display values are in units of the first available SIZE from --block-size,\n\
and the %s_BLOCK_SIZE, BLOCK_SIZE and BLOCKSIZE environment variables.\n\
Otherwise, units default to 1024 bytes (or 512 if POSIXLY_CORRECT is set).\n\
"), program);
}

static inline void
emit_update_parameters_note (void)
{
  fputs (_("\
\n\
UPDATE controls which existing files in the destination are replaced.\n\
'all' is the default operation when an --update option is not specified,\n\
and results in all existing files in the destination being replaced.\n\
'none' is similar to the --no-clobber option, in that no files in the\n\
destination are replaced, but also skipped files do not induce a failure.\n\
'older' is the default operation when --update is specified, and results\n\
in files being replaced if they're older than the corresponding source file.\n\
"), stdout);
}

static inline void
emit_backup_suffix_note (void)
{
  fputs (_("\
\n\
The backup suffix is '~', unless set with --suffix or SIMPLE_BACKUP_SUFFIX.\n\
The version control method may be selected via the --backup option or through\n\
the VERSION_CONTROL environment variable.  Here are the values:\n\
\n\
"), stdout);
  fputs (_("\
  none, off       never make backups (even if --backup is given)\n\
  numbered, t     make numbered backups\n\
  existing, nil   numbered if numbered backups exist, simple otherwise\n\
  simple, never   always make simple backups\n\
"), stdout);
}

static inline void
emit_exec_status (char const *program)
{
      printf (_("\n\
Exit status:\n\
  125  if the %s command itself fails\n\
  126  if COMMAND is found but cannot be invoked\n\
  127  if COMMAND cannot be found\n\
  -    the exit status of COMMAND otherwise\n\
"), program);
}

static inline void
emit_ancillary_info (char const *program)
{
  struct infomap { char const *program; char const *node; } const infomap[] = {
    { "[", "test invocation" },
    { "coreutils", "Multi-call invocation" },
    { "sha224sum", "sha2 utilities" },
    { "sha256sum", "sha2 utilities" },
    { "sha384sum", "sha2 utilities" },
    { "sha512sum", "sha2 utilities" },
    { nullptr, nullptr }
  };

  char const *node = program;
  struct infomap const *map_prog = infomap;

  while (map_prog->program && ! STREQ (program, map_prog->program))
    map_prog++;

  if (map_prog->node)
    node = map_prog->node;

  printf (_("\n%s online help: <%s>\n"), PACKAGE_NAME, PACKAGE_URL);

  /* Don't output this redundant message for English locales.
     Note we still output for 'C' so that it gets included in the man page.  */
  char const *lc_messages = setlocale (LC_MESSAGES, nullptr);
  if (lc_messages && STRNCMP_LIT (lc_messages, "en_"))
    {
      /* TRANSLATORS: Replace LANG_CODE in this URL with your language code
         <https://translationproject.org/team/LANG_CODE.html> to form one of
         the URLs at https://translationproject.org/team/.  Otherwise, replace
         the entire URL with your translation team's email address.  */
      fputs (_("Report any translation bugs to "
               "<https:
    }
   
  char const *url_program = STREQ (program, "[") ? "test" : program;
  printf (_("Full documentation <%s%s>\n"),
          PACKAGE_URL, url_program);
  printf (_("or available locally via: info '(coreutils) %s%s'\n"),
          node, node == program ? " invocation" : "");
}

 
#define emit_try_help() \
  do \
    { \
      fprintf (stderr, _("Try '%s --help' for more information.\n"), \
               program_name); \
    } \
  while (0)

#include "inttostr.h"

static inline char *
timetostr (time_t t, char *buf)
{
  return (TYPE_SIGNED (time_t)
          ? imaxtostr (t, buf)
          : umaxtostr (t, buf));
}

static inline char *
bad_cast (char const *s)
{
  return (char *) s;
}

 
static inline bool
usable_st_size (struct stat const *sb)
{
  return (S_ISREG (sb->st_mode) || S_ISLNK (sb->st_mode)
          || S_TYPEISSHM (sb) || S_TYPEISTMO (sb));
}

_Noreturn void usage (int status);

#include "error.h"

 
#define devmsg(...)			\
  do					\
    {					\
      if (dev_debug)			\
        fprintf (stderr, __VA_ARGS__);	\
    }					\
  while (0)

#define emit_cycle_warning(file_name)	\
  do					\
    {					\
      error (0, 0, _("\
WARNING: Circular directory structure.\n\
This almost certainly means that you have a corrupted file system.\n\
NOTIFY YOUR SYSTEM MANAGER.\n\
The following directory is part of the cycle:\n  %s\n"), \
             quotef (file_name));	\
    }					\
  while (0)

 

static inline void
write_error (void)
{
  int saved_errno = errno;
  fflush (stdout);     
  fpurge (stdout);     
  clearerr (stdout);   
  error (EXIT_FAILURE, saved_errno, _("write error"));
}

 
static inline char *
stzncpy (char *restrict dest, char const *restrict src, size_t len)
{
  size_t i;
  for (i = 0; i < len && *src; i++)
    *dest++ = *src++;
  *dest = 0;
  return dest;
}

#ifndef ARRAY_CARDINALITY
# define ARRAY_CARDINALITY(Array) (sizeof (Array) / sizeof *(Array))
#endif

 
static inline bool
is_ENOTSUP (int err)
{
  return err == EOPNOTSUPP || (ENOTSUP != EOPNOTSUPP && err == ENOTSUP);
}


 
#include "quotearg.h"

 
#define quotef(arg) \
  quotearg_n_style_colon (0, shell_escape_quoting_style, arg)
#define quotef_n(n, arg) \
  quotearg_n_style_colon (n, shell_escape_quoting_style, arg)

 
#define quoteaf(arg) \
  quotearg_style (shell_escape_always_quoting_style, arg)
#define quoteaf_n(n, arg) \
  quotearg_n_style (n, shell_escape_always_quoting_style, arg)
