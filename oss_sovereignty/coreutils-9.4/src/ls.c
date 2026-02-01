 

 

 

#include <config.h>
#include <sys/types.h>

#include <termios.h>
#if HAVE_STROPTS_H
# include <stropts.h>
#endif
#include <sys/ioctl.h>

#ifdef WINSIZE_IN_PTEM
# include <sys/stream.h>
# include <sys/ptem.h>
#endif

#include <stdio.h>
#include <setjmp.h>
#include <pwd.h>
#include <getopt.h>
#include <signal.h>
#include <selinux/selinux.h>
#include <wchar.h>

#if HAVE_LANGINFO_CODESET
# include <langinfo.h>
#endif

 
#ifndef SA_NOCLDSTOP
# define SA_NOCLDSTOP 0
# define sigprocmask(How, Set, Oset)  
# define sigset_t int
# if ! HAVE_SIGINTERRUPT
#  define siginterrupt(sig, flag)  
# endif
#endif

 
#ifndef SA_RESTART
# define SA_RESTART 0
#endif

#include "system.h"
#include <fnmatch.h>

#include "acl.h"
#include "argmatch.h"
#include "assure.h"
#include "c-strcase.h"
#include "dev-ino.h"
#include "filenamecat.h"
#include "hard-locale.h"
#include "hash.h"
#include "human.h"
#include "filemode.h"
#include "filevercmp.h"
#include "idcache.h"
#include "ls.h"
#include "mbswidth.h"
#include "mpsort.h"
#include "obstack.h"
#include "quote.h"
#include "smack.h"
#include "stat-size.h"
#include "stat-time.h"
#include "strftime.h"
#include "xdectoint.h"
#include "xstrtol.h"
#include "xstrtol-error.h"
#include "areadlink.h"
#include "mbsalign.h"
#include "dircolors.h"
#include "xgethostname.h"
#include "c-ctype.h"
#include "canonicalize.h"
#include "statx.h"

 

#ifndef READDIR_LIES_ABOUT_MOUNTPOINT_D_INO
# define READDIR_LIES_ABOUT_MOUNTPOINT_D_INO 1
#endif

#if READDIR_LIES_ABOUT_MOUNTPOINT_D_INO
# define RELIABLE_D_INO(dp) NOT_AN_INODE_NUMBER
#else
# define RELIABLE_D_INO(dp) D_INO (dp)
#endif

#if ! HAVE_STRUCT_STAT_ST_AUTHOR
# define st_author st_uid
#endif

enum filetype
  {
    unknown,
    fifo,
    chardev,
    directory,
    blockdev,
    normal,
    symbolic_link,
    sock,
    whiteout,
    arg_directory
  };

 
static char const filetype_letter[] = "?pcdb-lswd";

 
static_assert (sizeof filetype_letter - 1 == arg_directory + 1);

#define FILETYPE_INDICATORS				\
  {							\
    C_ORPHAN, C_FIFO, C_CHR, C_DIR, C_BLK, C_FILE,	\
    C_LINK, C_SOCK, C_FILE, C_DIR			\
  }

enum acl_type
  {
    ACL_T_NONE,
    ACL_T_LSM_CONTEXT_ONLY,
    ACL_T_YES
  };

struct fileinfo
  {
     
    char *name;

     
    char *linkname;

     
    char *absolute_name;

    struct stat stat;

    enum filetype filetype;

     
    mode_t linkmode;

     
    char *scontext;

    bool stat_ok;

     
    bool linkok;

     
    enum acl_type acl_type;

     
    bool has_capability;

     
    int quoted;

     
    size_t width;
  };

#define LEN_STR_PAIR(s) sizeof (s) - 1, s

 

struct bin_str
  {
    size_t len;			 
    char const *string;		 
  };

#if ! HAVE_TCGETPGRP
# define tcgetpgrp(Fd) 0
#endif

static size_t quote_name (char const *name,
                          struct quoting_options const *options,
                          int needs_general_quoting,
                          const struct bin_str *color,
                          bool allow_pad, struct obstack *stack,
                          char const *absolute_name);
static size_t quote_name_buf (char **inbuf, size_t bufsize, char *name,
                              struct quoting_options const *options,
                              int needs_general_quoting, size_t *width,
                              bool *pad);
static int decode_switches (int argc, char **argv);
static bool file_ignored (char const *name);
static uintmax_t gobble_file (char const *name, enum filetype type,
                              ino_t inode, bool command_line_arg,
                              char const *dirname);
static const struct bin_str * get_color_indicator (const struct fileinfo *f,
                                                   bool symlink_target);
static bool print_color_indicator (const struct bin_str *ind);
static void put_indicator (const struct bin_str *ind);
static void add_ignore_pattern (char const *pattern);
static void attach (char *dest, char const *dirname, char const *name);
static void clear_files (void);
static void extract_dirs_from_files (char const *dirname,
                                     bool command_line_arg);
static void get_link_name (char const *filename, struct fileinfo *f,
                           bool command_line_arg);
static void indent (size_t from, size_t to);
static size_t calculate_columns (bool by_columns);
static void print_current_files (void);
static void print_dir (char const *name, char const *realname,
                       bool command_line_arg);
static size_t print_file_name_and_frills (const struct fileinfo *f,
                                          size_t start_col);
static void print_horizontal (void);
static int format_user_width (uid_t u);
static int format_group_width (gid_t g);
static void print_long_format (const struct fileinfo *f);
static void print_many_per_line (void);
static size_t print_name_with_quoting (const struct fileinfo *f,
                                       bool symlink_target,
                                       struct obstack *stack,
                                       size_t start_col);
static void prep_non_filename_text (void);
static bool print_type_indicator (bool stat_ok, mode_t mode,
                                  enum filetype type);
static void print_with_separator (char sep);
static void queue_directory (char const *name, char const *realname,
                             bool command_line_arg);
static void sort_files (void);
static void parse_ls_color (void);

static int getenv_quoting_style (void);

static size_t quote_name_width (char const *name,
                                struct quoting_options const *options,
                                int needs_general_quoting);

 
enum { INITIAL_TABLE_SIZE = 30 };

 
static Hash_table *active_dir_set;

#define LOOP_DETECT (!!active_dir_set)

 

 
static struct fileinfo *cwd_file;

 
static size_t cwd_n_alloc;

 
static size_t cwd_n_used;

 
static bool cwd_some_quoted;

 
static bool align_variable_outer_quotes;

 
static void **sorted_file;
static size_t sorted_file_alloc;

 

static bool color_symlink_as_referent;

static char const *hostname;

 
static mode_t
file_or_link_mode (struct fileinfo const *file)
{
  return (color_symlink_as_referent && file->linkok
          ? file->linkmode : file->stat.st_mode);
}


 

struct pending
  {
    char *name;
     
    char *realname;
    bool command_line_arg;
    struct pending *next;
  };

static struct pending *pending_dirs;

 

static struct timespec current_time;

static bool print_scontext;
static char UNKNOWN_SECURITY_CONTEXT[] = "?";

 

static bool any_has_acl;

 

static int inode_number_width;
static int block_size_width;
static int nlink_width;
static int scontext_width;
static int owner_width;
static int group_width;
static int author_width;
static int major_device_number_width;
static int minor_device_number_width;
static int file_size_width;

 

 

enum format
  {
    long_format,		 
    one_per_line,		 
    many_per_line,		 
    horizontal,			 
    with_commas			 
  };

static enum format format;

 
enum time_style
  {
    full_iso_time_style,	 
    long_iso_time_style,	 
    iso_time_style,		 
    locale_time_style		 
  };

static char const *const time_style_args[] =
{
  "full-iso", "long-iso", "iso", "locale", nullptr
};
static enum time_style const time_style_types[] =
{
  full_iso_time_style, long_iso_time_style, iso_time_style,
  locale_time_style
};
ARGMATCH_VERIFY (time_style_args, time_style_types);

 

enum time_type
  {
    time_mtime = 0,		 
    time_ctime,			 
    time_atime,			 
    time_btime,                  
    time_numtypes		 
  };

static enum time_type time_type;

 

enum sort_type
  {
    sort_name = 0,		 
    sort_extension,		 
    sort_width,
    sort_size,			 
    sort_version,		 
    sort_time,			 
    sort_none,			 
    sort_numtypes		 
  };

static enum sort_type sort_type;

 

static bool sort_reverse;

 

static bool print_owner = true;

 

static bool print_author;

 

static bool print_group = true;

 

static bool numeric_ids;

 

static bool print_block_size;

 
static int human_output_opts;

 
static uintmax_t output_block_size;

 
static int file_human_output_opts;
static uintmax_t file_output_block_size = 1;

 
static bool dired;

 

enum indicator_style
  {
    none = 0,	 
    slash,	 
    file_type,	 
    classify	 
  };

static enum indicator_style indicator_style;

 
static char const *const indicator_style_args[] =
{
  "none", "slash", "file-type", "classify", nullptr
};
static enum indicator_style const indicator_style_types[] =
{
  none, slash, file_type, classify
};
ARGMATCH_VERIFY (indicator_style_args, indicator_style_types);

 

static bool print_with_color;

static bool print_hyperlink;

 

static bool used_color = false;

enum when_type
  {
    when_never,		 
    when_always,	 
    when_if_tty		 
  };

enum Dereference_symlink
  {
    DEREF_UNDEFINED = 0,		 
    DEREF_NEVER,
    DEREF_COMMAND_LINE_ARGUMENTS,	 
    DEREF_COMMAND_LINE_SYMLINK_TO_DIR,	 
    DEREF_ALWAYS			 
  };

enum indicator_no
  {
    C_LEFT, C_RIGHT, C_END, C_RESET, C_NORM, C_FILE, C_DIR, C_LINK,
    C_FIFO, C_SOCK,
    C_BLK, C_CHR, C_MISSING, C_ORPHAN, C_EXEC, C_DOOR, C_SETUID, C_SETGID,
    C_STICKY, C_OTHER_WRITABLE, C_STICKY_OTHER_WRITABLE, C_CAP, C_MULTIHARDLINK,
    C_CLR_TO_EOL
  };

static char const *const indicator_name[]=
  {
    "lc", "rc", "ec", "rs", "no", "fi", "di", "ln", "pi", "so",
    "bd", "cd", "mi", "or", "ex", "do", "su", "sg", "st",
    "ow", "tw", "ca", "mh", "cl", nullptr
  };

struct color_ext_type
  {
    struct bin_str ext;		 
    struct bin_str seq;		 
    bool   exact_match;		 
    struct color_ext_type *next;	 
  };

static struct bin_str color_indicator[] =
  {
    { LEN_STR_PAIR ("\033[") },		 
    { LEN_STR_PAIR ("m") },		 
    { 0, nullptr },			 
    { LEN_STR_PAIR ("0") },		 
    { 0, nullptr },			 
    { 0, nullptr },			 
    { LEN_STR_PAIR ("01;34") },		 
    { LEN_STR_PAIR ("01;36") },		 
    { LEN_STR_PAIR ("33") },		 
    { LEN_STR_PAIR ("01;35") },		 
    { LEN_STR_PAIR ("01;33") },		 
    { LEN_STR_PAIR ("01;33") },		 
    { 0, nullptr },			 
    { 0, nullptr },			 
    { LEN_STR_PAIR ("01;32") },		 
    { LEN_STR_PAIR ("01;35") },		 
    { LEN_STR_PAIR ("37;41") },		 
    { LEN_STR_PAIR ("30;43") },		 
    { LEN_STR_PAIR ("37;44") },		 
    { LEN_STR_PAIR ("34;42") },		 
    { LEN_STR_PAIR ("30;42") },		 
    { 0, nullptr },			 
    { 0, nullptr },			 
    { LEN_STR_PAIR ("\033[K") },	 
  };

 
static struct color_ext_type *color_ext_list = nullptr;

 
static char *color_buf;

 

static bool check_symlink_mode;

 

static bool print_inode;

 

static enum Dereference_symlink dereference;

 

static bool recursive;

 

static bool immediate_dirs;

 

static bool directories_first;

 

static enum
{
   
  IGNORE_DEFAULT = 0,

   
  IGNORE_DOT_AND_DOTDOT,

   
  IGNORE_MINIMAL
} ignore_mode;

 

struct ignore_pattern
  {
    char const *pattern;
    struct ignore_pattern *next;
  };

static struct ignore_pattern *ignore_patterns;

 
static struct ignore_pattern *hide_patterns;

 
static bool qmark_funny_chars;

 

static struct quoting_options *filename_quoting_options;
static struct quoting_options *dirname_quoting_options;

 
static size_t tabsize;

 

static bool print_dir_name;

 

static size_t line_length;

 

static timezone_t localtz;

 

static bool format_needs_stat;

 

static bool format_needs_type;

 

enum { TIME_STAMP_LEN_MAXIMUM = MAX (1000, INT_STRLEN_BOUND (time_t)) };

 

static char const *long_time_format[2] =
  {
     
     
    N_("%b %e  %Y"),
     
     
    N_("%b %e %H:%M")
  };

 

static sigset_t caught_signals;

 

static sig_atomic_t volatile interrupt_signal;

 

static sig_atomic_t volatile stop_signal_count;

 

static int exit_status;

 
enum
  {
     
    LS_MINOR_PROBLEM = 1,

     
    LS_FAILURE = 2
  };

 
enum
{
  AUTHOR_OPTION = CHAR_MAX + 1,
  BLOCK_SIZE_OPTION,
  COLOR_OPTION,
  DEREFERENCE_COMMAND_LINE_SYMLINK_TO_DIR_OPTION,
  FILE_TYPE_INDICATOR_OPTION,
  FORMAT_OPTION,
  FULL_TIME_OPTION,
  GROUP_DIRECTORIES_FIRST_OPTION,
  HIDE_OPTION,
  HYPERLINK_OPTION,
  INDICATOR_STYLE_OPTION,
  QUOTING_STYLE_OPTION,
  SHOW_CONTROL_CHARS_OPTION,
  SI_OPTION,
  SORT_OPTION,
  TIME_OPTION,
  TIME_STYLE_OPTION,
  ZERO_OPTION,
};

static struct option const long_options[] =
{
  {"all", no_argument, nullptr, 'a'},
  {"escape", no_argument, nullptr, 'b'},
  {"directory", no_argument, nullptr, 'd'},
  {"dired", no_argument, nullptr, 'D'},
  {"full-time", no_argument, nullptr, FULL_TIME_OPTION},
  {"group-directories-first", no_argument, nullptr,
   GROUP_DIRECTORIES_FIRST_OPTION},
  {"human-readable", no_argument, nullptr, 'h'},
  {"inode", no_argument, nullptr, 'i'},
  {"kibibytes", no_argument, nullptr, 'k'},
  {"numeric-uid-gid", no_argument, nullptr, 'n'},
  {"no-group", no_argument, nullptr, 'G'},
  {"hide-control-chars", no_argument, nullptr, 'q'},
  {"reverse", no_argument, nullptr, 'r'},
  {"size", no_argument, nullptr, 's'},
  {"width", required_argument, nullptr, 'w'},
  {"almost-all", no_argument, nullptr, 'A'},
  {"ignore-backups", no_argument, nullptr, 'B'},
  {"classify", optional_argument, nullptr, 'F'},
  {"file-type", no_argument, nullptr, FILE_TYPE_INDICATOR_OPTION},
  {"si", no_argument, nullptr, SI_OPTION},
  {"dereference-command-line", no_argument, nullptr, 'H'},
  {"dereference-command-line-symlink-to-dir", no_argument, nullptr,
   DEREFERENCE_COMMAND_LINE_SYMLINK_TO_DIR_OPTION},
  {"hide", required_argument, nullptr, HIDE_OPTION},
  {"ignore", required_argument, nullptr, 'I'},
  {"indicator-style", required_argument, nullptr, INDICATOR_STYLE_OPTION},
  {"dereference", no_argument, nullptr, 'L'},
  {"literal", no_argument, nullptr, 'N'},
  {"quote-name", no_argument, nullptr, 'Q'},
  {"quoting-style", required_argument, nullptr, QUOTING_STYLE_OPTION},
  {"recursive", no_argument, nullptr, 'R'},
  {"format", required_argument, nullptr, FORMAT_OPTION},
  {"show-control-chars", no_argument, nullptr, SHOW_CONTROL_CHARS_OPTION},
  {"sort", required_argument, nullptr, SORT_OPTION},
  {"tabsize", required_argument, nullptr, 'T'},
  {"time", required_argument, nullptr, TIME_OPTION},
  {"time-style", required_argument, nullptr, TIME_STYLE_OPTION},
  {"zero", no_argument, nullptr, ZERO_OPTION},
  {"color", optional_argument, nullptr, COLOR_OPTION},
  {"hyperlink", optional_argument, nullptr, HYPERLINK_OPTION},
  {"block-size", required_argument, nullptr, BLOCK_SIZE_OPTION},
  {"context", no_argument, 0, 'Z'},
  {"author", no_argument, nullptr, AUTHOR_OPTION},
  {GETOPT_HELP_OPTION_DECL},
  {GETOPT_VERSION_OPTION_DECL},
  {nullptr, 0, nullptr, 0}
};

static char const *const format_args[] =
{
  "verbose", "long", "commas", "horizontal", "across",
  "vertical", "single-column", nullptr
};
static enum format const format_types[] =
{
  long_format, long_format, with_commas, horizontal, horizontal,
  many_per_line, one_per_line
};
ARGMATCH_VERIFY (format_args, format_types);

static char const *const sort_args[] =
{
  "none", "time", "size", "extension", "version", "width", nullptr
};
static enum sort_type const sort_types[] =
{
  sort_none, sort_time, sort_size, sort_extension, sort_version, sort_width
};
ARGMATCH_VERIFY (sort_args, sort_types);

static char const *const time_args[] =
{
  "atime", "access", "use",
  "ctime", "status",
  "mtime", "modification",
  "birth", "creation",
  nullptr
};
static enum time_type const time_types[] =
{
  time_atime, time_atime, time_atime,
  time_ctime, time_ctime,
  time_mtime, time_mtime,
  time_btime, time_btime,
};
ARGMATCH_VERIFY (time_args, time_types);

static char const *const when_args[] =
{
   
  "always", "yes", "force",
  "never", "no", "none",
  "auto", "tty", "if-tty", nullptr
};
static enum when_type const when_types[] =
{
  when_always, when_always, when_always,
  when_never, when_never, when_never,
  when_if_tty, when_if_tty, when_if_tty
};
ARGMATCH_VERIFY (when_args, when_types);

 
struct column_info
{
  bool valid_len;
  size_t line_len;
  size_t *col_arr;
};

 
static struct column_info *column_info;

 
static size_t max_idx;

 
enum { MIN_COLUMN_WIDTH = 3 };


 
static off_t dired_pos;

static void
dired_outbyte (char c)
{
  dired_pos++;
  putchar (c);
}

 
static void
dired_outbuf (char const *s, size_t s_len)
{
  dired_pos += s_len;
  fwrite (s, sizeof *s, s_len, stdout);
}

 
static void
dired_outstring (char const *s)
{
  dired_outbuf (s, strlen (s));
}

static void
dired_indent (void)
{
  if (dired)
    dired_outstring ("  ");
}

 
static struct obstack dired_obstack;

 
static struct obstack subdired_obstack;

 
static void
push_current_dired_pos (struct obstack *obs)
{
  if (dired)
    obstack_grow (obs, &dired_pos, sizeof dired_pos);
}

 
static struct obstack dev_ino_obstack;

 
static void
dev_ino_push (dev_t dev, ino_t ino)
{
  void *vdi;
  struct dev_ino *di;
  int dev_ino_size = sizeof *di;
  obstack_blank (&dev_ino_obstack, dev_ino_size);
  vdi = obstack_next_free (&dev_ino_obstack);
  di = vdi;
  di--;
  di->st_dev = dev;
  di->st_ino = ino;
}

 
static struct dev_ino
dev_ino_pop (void)
{
  void *vdi;
  struct dev_ino *di;
  int dev_ino_size = sizeof *di;
  affirm (dev_ino_size <= obstack_object_size (&dev_ino_obstack));
  obstack_blank_fast (&dev_ino_obstack, -dev_ino_size);
  vdi = obstack_next_free (&dev_ino_obstack);
  di = vdi;
  return *di;
}

static void
assert_matching_dev_ino (char const *name, struct dev_ino di)
{
  MAYBE_UNUSED struct stat sb;
  assure (0 <= stat (name, &sb));
  assure (sb.st_dev == di.st_dev);
  assure (sb.st_ino == di.st_ino);
}

static char eolbyte = '\n';

 

static void
dired_dump_obstack (char const *prefix, struct obstack *os)
{
  size_t n_pos;

  n_pos = obstack_object_size (os) / sizeof (dired_pos);
  if (n_pos > 0)
    {
      off_t *pos = obstack_finish (os);
      fputs (prefix, stdout);
      for (size_t i = 0; i < n_pos; i++)
        {
          intmax_t p = pos[i];
          printf (" %"PRIdMAX, p);
        }
      putchar ('\n');
    }
}

 
static struct timespec
get_stat_btime (struct stat const *st)
{
  struct timespec btimespec;

#if HAVE_STATX && defined STATX_INO
  btimespec = get_stat_mtime (st);
#else
  btimespec = get_stat_birthtime (st);
#endif

  return btimespec;
}

#if HAVE_STATX && defined STATX_INO
ATTRIBUTE_PURE
static unsigned int
time_type_to_statx (void)
{
  switch (time_type)
    {
    case time_ctime:
      return STATX_CTIME;
    case time_mtime:
      return STATX_MTIME;
    case time_atime:
      return STATX_ATIME;
    case time_btime:
      return STATX_BTIME;
    default:
      unreachable ();
    }
    return 0;
}

ATTRIBUTE_PURE
static unsigned int
calc_req_mask (void)
{
  unsigned int mask = STATX_MODE;

  if (print_inode)
    mask |= STATX_INO;

  if (print_block_size)
    mask |= STATX_BLOCKS;

  if (format == long_format) {
    mask |= STATX_NLINK | STATX_SIZE | time_type_to_statx ();
    if (print_owner || print_author)
      mask |= STATX_UID;
    if (print_group)
      mask |= STATX_GID;
  }

  switch (sort_type)
    {
    case sort_none:
    case sort_name:
    case sort_version:
    case sort_extension:
    case sort_width:
      break;
    case sort_time:
      mask |= time_type_to_statx ();
      break;
    case sort_size:
      mask |= STATX_SIZE;
      break;
    default:
      unreachable ();
    }

  return mask;
}

static int
do_statx (int fd, char const *name, struct stat *st, int flags,
          unsigned int mask)
{
  struct statx stx;
  bool want_btime = mask & STATX_BTIME;
  int ret = statx (fd, name, flags | AT_NO_AUTOMOUNT, mask, &stx);
  if (ret >= 0)
    {
      statx_to_stat (&stx, st);
       
      if (want_btime)
        {
          if (stx.stx_mask & STATX_BTIME)
            st->st_mtim = statx_timestamp_to_timespec (stx.stx_btime);
          else
            st->st_mtim.tv_sec = st->st_mtim.tv_nsec = -1;
        }
    }

  return ret;
}

static int
do_stat (char const *name, struct stat *st)
{
  return do_statx (AT_FDCWD, name, st, 0, calc_req_mask ());
}

static int
do_lstat (char const *name, struct stat *st)
{
  return do_statx (AT_FDCWD, name, st, AT_SYMLINK_NOFOLLOW, calc_req_mask ());
}

static int
stat_for_mode (char const *name, struct stat *st)
{
  return do_statx (AT_FDCWD, name, st, 0, STATX_MODE);
}

 
static int
stat_for_ino (char const *name, struct stat *st)
{
  return do_statx (AT_FDCWD, name, st, 0, STATX_INO);
}

static int
fstat_for_ino (int fd, struct stat *st)
{
  return do_statx (fd, "", st, AT_EMPTY_PATH, STATX_INO);
}
#else
static int
do_stat (char const *name, struct stat *st)
{
  return stat (name, st);
}

static int
do_lstat (char const *name, struct stat *st)
{
  return lstat (name, st);
}

static int
stat_for_mode (char const *name, struct stat *st)
{
  return stat (name, st);
}

static int
stat_for_ino (char const *name, struct stat *st)
{
  return stat (name, st);
}

static int
fstat_for_ino (int fd, struct stat *st)
{
  return fstat (fd, st);
}
#endif

 

ATTRIBUTE_PURE
static char const *
first_percent_b (char const *fmt)
{
  for (; *fmt; fmt++)
    if (fmt[0] == '%')
      switch (fmt[1])
        {
        case 'b': return fmt;
        case '%': fmt++; break;
        }
  return nullptr;
}

static char RFC3986[256];
static void
file_escape_init (void)
{
  for (int i = 0; i < 256; i++)
    RFC3986[i] |= c_isalnum (i) || i == '~' || i == '-' || i == '.' || i == '_';
}

 

 
enum { MAX_MON_WIDTH = 12 };
 
enum { ABFORMAT_SIZE = 128 };
static char abformat[2][12][ABFORMAT_SIZE];
 
static bool use_abformat;

 

static bool
abmon_init (char abmon[12][ABFORMAT_SIZE])
{
#ifndef HAVE_NL_LANGINFO
  return false;
#else
  size_t required_mon_width = MAX_MON_WIDTH;
  size_t curr_max_width;
  do
    {
      curr_max_width = required_mon_width;
      required_mon_width = 0;
      for (int i = 0; i < 12; i++)
        {
          size_t width = curr_max_width;
          char const *abbr = nl_langinfo (ABMON_1 + i);
          if (strchr (abbr, '%'))
            return false;
          mbs_align_t alignment = isdigit (to_uchar (*abbr))
                                  ? MBS_ALIGN_RIGHT : MBS_ALIGN_LEFT;
          size_t req = mbsalign (abbr, abmon[i], ABFORMAT_SIZE,
                                 &width, alignment, 0);
          if (! (req < ABFORMAT_SIZE))
            return false;
          required_mon_width = MAX (required_mon_width, width);
        }
    }
  while (curr_max_width > required_mon_width);

  return true;
#endif
}

 

static void
abformat_init (void)
{
  char const *pb[2];
  for (int recent = 0; recent < 2; recent++)
    pb[recent] = first_percent_b (long_time_format[recent]);
  if (! (pb[0] || pb[1]))
    return;

  char abmon[12][ABFORMAT_SIZE];
  if (! abmon_init (abmon))
    return;

  for (int recent = 0; recent < 2; recent++)
    {
      char const *fmt = long_time_format[recent];
      for (int i = 0; i < 12; i++)
        {
          char *nfmt = abformat[recent][i];
          int nbytes;

          if (! pb[recent])
            nbytes = snprintf (nfmt, ABFORMAT_SIZE, "%s", fmt);
          else
            {
              if (! (pb[recent] - fmt <= MIN (ABFORMAT_SIZE, INT_MAX)))
                return;
              int prefix_len = pb[recent] - fmt;
              nbytes = snprintf (nfmt, ABFORMAT_SIZE, "%.*s%s%s",
                                 prefix_len, fmt, abmon[i], pb[recent] + 2);
            }

          if (! (0 <= nbytes && nbytes < ABFORMAT_SIZE))
            return;
        }
    }

  use_abformat = true;
}

static size_t
dev_ino_hash (void const *x, size_t table_size)
{
  struct dev_ino const *p = x;
  return (uintmax_t) p->st_ino % table_size;
}

static bool
dev_ino_compare (void const *x, void const *y)
{
  struct dev_ino const *a = x;
  struct dev_ino const *b = y;
  return SAME_INODE (*a, *b) ? true : false;
}

static void
dev_ino_free (void *x)
{
  free (x);
}

 

static bool
visit_dir (dev_t dev, ino_t ino)
{
  struct dev_ino *ent;
  struct dev_ino *ent_from_table;
  bool found_match;

  ent = xmalloc (sizeof *ent);
  ent->st_ino = ino;
  ent->st_dev = dev;

   
  ent_from_table = hash_insert (active_dir_set, ent);

  if (ent_from_table == nullptr)
    {
       
      xalloc_die ();
    }

  found_match = (ent_from_table != ent);

  if (found_match)
    {
       
      free (ent);
    }

  return found_match;
}

static void
free_pending_ent (struct pending *p)
{
  free (p->name);
  free (p->realname);
  free (p);
}

static bool
is_colored (enum indicator_no type)
{
  size_t len = color_indicator[type].len;
  char const *s = color_indicator[type].string;
  return ! (len == 0
            || (len == 1 && STRNCMP_LIT (s, "0") == 0)
            || (len == 2 && STRNCMP_LIT (s, "00") == 0));
}

static void
restore_default_color (void)
{
  put_indicator (&color_indicator[C_LEFT]);
  put_indicator (&color_indicator[C_RIGHT]);
}

static void
set_normal_color (void)
{
  if (print_with_color && is_colored (C_NORM))
    {
      put_indicator (&color_indicator[C_LEFT]);
      put_indicator (&color_indicator[C_NORM]);
      put_indicator (&color_indicator[C_RIGHT]);
    }
}

 

static void
sighandler (int sig)
{
  if (! SA_NOCLDSTOP)
    signal (sig, SIG_IGN);
  if (! interrupt_signal)
    interrupt_signal = sig;
}

 

static void
stophandler (int sig)
{
  if (! SA_NOCLDSTOP)
    signal (sig, stophandler);
  if (! interrupt_signal)
    stop_signal_count++;
}

 

static void
process_signals (void)
{
  while (interrupt_signal || stop_signal_count)
    {
      int sig;
      int stops;
      sigset_t oldset;

      if (used_color)
        restore_default_color ();
      fflush (stdout);

      sigprocmask (SIG_BLOCK, &caught_signals, &oldset);

       
      sig = interrupt_signal;
      stops = stop_signal_count;

       
      if (stops)
        {
          stop_signal_count = stops - 1;
          sig = SIGSTOP;
        }
      else
        signal (sig, SIG_DFL);

       
      raise (sig);
      sigprocmask (SIG_SETMASK, &oldset, nullptr);

       
    }
}

 

static void
signal_setup (bool init)
{
   
  static int const sig[] =
    {
       
      SIGTSTP,

       
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

#if ! SA_NOCLDSTOP
  static bool caught_sig[nsigs];
#endif

  int j;

  if (init)
    {
#if SA_NOCLDSTOP
      struct sigaction act;

      sigemptyset (&caught_signals);
      for (j = 0; j < nsigs; j++)
        {
          sigaction (sig[j], nullptr, &act);
          if (act.sa_handler != SIG_IGN)
            sigaddset (&caught_signals, sig[j]);
        }

      act.sa_mask = caught_signals;
      act.sa_flags = SA_RESTART;

      for (j = 0; j < nsigs; j++)
        if (sigismember (&caught_signals, sig[j]))
          {
            act.sa_handler = sig[j] == SIGTSTP ? stophandler : sighandler;
            sigaction (sig[j], &act, nullptr);
          }
#else
      for (j = 0; j < nsigs; j++)
        {
          caught_sig[j] = (signal (sig[j], SIG_IGN) != SIG_IGN);
          if (caught_sig[j])
            {
              signal (sig[j], sig[j] == SIGTSTP ? stophandler : sighandler);
              siginterrupt (sig[j], 0);
            }
        }
#endif
    }
  else  
    {
#if SA_NOCLDSTOP
      for (j = 0; j < nsigs; j++)
        if (sigismember (&caught_signals, sig[j]))
          signal (sig[j], SIG_DFL);
#else
      for (j = 0; j < nsigs; j++)
        if (caught_sig[j])
          signal (sig[j], SIG_DFL);
#endif
    }
}

static void
signal_init (void)
{
  signal_setup (true);
}

static void
signal_restore (void)
{
  signal_setup (false);
}

int
main (int argc, char **argv)
{
  int i;
  struct pending *thispend;
  int n_files;

  initialize_main (&argc, &argv);
  set_program_name (argv[0]);
  setlocale (LC_ALL, "");
  bindtextdomain (PACKAGE, LOCALEDIR);
  textdomain (PACKAGE);

  initialize_exit_failure (LS_FAILURE);
  atexit (close_stdout);

  static_assert (ARRAY_CARDINALITY (color_indicator) + 1
                 == ARRAY_CARDINALITY (indicator_name));

  exit_status = EXIT_SUCCESS;
  print_dir_name = true;
  pending_dirs = nullptr;

  current_time.tv_sec = TYPE_MINIMUM (time_t);
  current_time.tv_nsec = -1;

  i = decode_switches (argc, argv);

  if (print_with_color)
    parse_ls_color ();

   

  if (print_with_color)
    {
       
      tabsize = 0;
    }

  if (directories_first)
    check_symlink_mode = true;
  else if (print_with_color)
    {
       
      if (is_colored (C_ORPHAN)
          || (is_colored (C_EXEC) && color_symlink_as_referent)
          || (is_colored (C_MISSING) && format == long_format))
        check_symlink_mode = true;
    }

  if (dereference == DEREF_UNDEFINED)
    dereference = ((immediate_dirs
                    || indicator_style == classify
                    || format == long_format)
                   ? DEREF_NEVER
                   : DEREF_COMMAND_LINE_SYMLINK_TO_DIR);

   
  if (recursive)
    {
      active_dir_set = hash_initialize (INITIAL_TABLE_SIZE, nullptr,
                                        dev_ino_hash,
                                        dev_ino_compare,
                                        dev_ino_free);
      if (active_dir_set == nullptr)
        xalloc_die ();

      obstack_init (&dev_ino_obstack);
    }

  localtz = tzalloc (getenv ("TZ"));

  format_needs_stat = sort_type == sort_time || sort_type == sort_size
    || format == long_format
    || print_scontext
    || print_block_size;
  format_needs_type = (! format_needs_stat
                       && (recursive
                           || print_with_color
                           || indicator_style != none
                           || directories_first));

  if (dired)
    {
      obstack_init (&dired_obstack);
      obstack_init (&subdired_obstack);
    }

  if (print_hyperlink)
    {
      file_escape_init ();

      hostname = xgethostname ();
       
      if (! hostname)
        hostname = "";
    }

  cwd_n_alloc = 100;
  cwd_file = xnmalloc (cwd_n_alloc, sizeof *cwd_file);
  cwd_n_used = 0;

  clear_files ();

  n_files = argc - i;

  if (n_files <= 0)
    {
      if (immediate_dirs)
        gobble_file (".", directory, NOT_AN_INODE_NUMBER, true, "");
      else
        queue_directory (".", nullptr, true);
    }
  else
    do
      gobble_file (argv[i++], unknown, NOT_AN_INODE_NUMBER, true, "");
    while (i < argc);

  if (cwd_n_used)
    {
      sort_files ();
      if (!immediate_dirs)
        extract_dirs_from_files (nullptr, true);
       
    }

   
  if (cwd_n_used)
    {
      print_current_files ();
      if (pending_dirs)
        dired_outbyte ('\n');
    }
  else if (n_files <= 1 && pending_dirs && pending_dirs->next == 0)
    print_dir_name = false;

  while (pending_dirs)
    {
      thispend = pending_dirs;
      pending_dirs = pending_dirs->next;

      if (LOOP_DETECT)
        {
          if (thispend->name == nullptr)
            {
               
              struct dev_ino di = dev_ino_pop ();
              struct dev_ino *found = hash_remove (active_dir_set, &di);
              if (false)
                assert_matching_dev_ino (thispend->realname, di);
              affirm (found);
              dev_ino_free (found);
              free_pending_ent (thispend);
              continue;
            }
        }

      print_dir (thispend->name, thispend->realname,
                 thispend->command_line_arg);

      free_pending_ent (thispend);
      print_dir_name = true;
    }

  if (print_with_color && used_color)
    {
      int j;

       
      if (!(color_indicator[C_LEFT].len == 2
            && memcmp (color_indicator[C_LEFT].string, "\033[", 2) == 0
            && color_indicator[C_RIGHT].len == 1
            && color_indicator[C_RIGHT].string[0] == 'm'))
        restore_default_color ();

      fflush (stdout);

      signal_restore ();

       
      for (j = stop_signal_count; j; j--)
        raise (SIGSTOP);
      j = interrupt_signal;
      if (j)
        raise (j);
    }

  if (dired)
    {
       
      dired_dump_obstack ("//DIRED//", &dired_obstack);
      dired_dump_obstack ("//SUBDIRED//", &subdired_obstack);
      printf ("//DIRED-OPTIONS// --quoting-style=%s\n",
              quoting_style_args[get_quoting_style (filename_quoting_options)]);
    }

  if (LOOP_DETECT)
    {
      assure (hash_get_n_entries (active_dir_set) == 0);
      hash_free (active_dir_set);
    }

  return exit_status;
}

 

static ptrdiff_t
decode_line_length (char const *spec)
{
  uintmax_t val;

   
  switch (xstrtoumax (spec, nullptr, 0, &val, ""))
    {
    case LONGINT_OK:
      return val <= MIN (PTRDIFF_MAX, SIZE_MAX) ? val : 0;

    case LONGINT_OVERFLOW:
      return 0;

    default:
      return -1;
    }
}

 

static bool
stdout_isatty (void)
{
  static signed char out_tty = -1;
  if (out_tty < 0)
    out_tty = isatty (STDOUT_FILENO);
  assume (out_tty == 0 || out_tty == 1);
  return out_tty;
}

 

static int
decode_switches (int argc, char **argv)
{
  char *time_style_option = nullptr;

   
  bool kibibytes_specified = false;
  int format_opt = -1;
  int hide_control_chars_opt = -1;
  int quoting_style_opt = -1;
  int sort_opt = -1;
  ptrdiff_t tabsize_opt = -1;
  ptrdiff_t width_opt = -1;

  while (true)
    {
      int oi = -1;
      int c = getopt_long (argc, argv,
                           "abcdfghiklmnopqrstuvw:xABCDFGHI:LNQRST:UXZ1",
                           long_options, &oi);
      if (c == -1)
        break;

      switch (c)
        {
        case 'a':
          ignore_mode = IGNORE_MINIMAL;
          break;

        case 'b':
          quoting_style_opt = escape_quoting_style;
          break;

        case 'c':
          time_type = time_ctime;
          break;

        case 'd':
          immediate_dirs = true;
          break;

        case 'f':
           
          ignore_mode = IGNORE_MINIMAL;
          sort_opt = sort_none;
          if (format_opt == long_format)
            format_opt = -1;
          print_with_color = false;
          print_hyperlink = false;
          print_block_size = false;
          break;

        case FILE_TYPE_INDICATOR_OPTION:  
          indicator_style = file_type;
          break;

        case 'g':
          format_opt = long_format;
          print_owner = false;
          break;

        case 'h':
          file_human_output_opts = human_output_opts =
            human_autoscale | human_SI | human_base_1024;
          file_output_block_size = output_block_size = 1;
          break;

        case 'i':
          print_inode = true;
          break;

        case 'k':
          kibibytes_specified = true;
          break;

        case 'l':
          format_opt = long_format;
          break;

        case 'm':
          format_opt = with_commas;
          break;

        case 'n':
          numeric_ids = true;
          format_opt = long_format;
          break;

        case 'o':   
          format_opt = long_format;
          print_group = false;
          break;

        case 'p':
          indicator_style = slash;
          break;

        case 'q':
          hide_control_chars_opt = true;
          break;

        case 'r':
          sort_reverse = true;
          break;

        case 's':
          print_block_size = true;
          break;

        case 't':
          sort_opt = sort_time;
          break;

        case 'u':
          time_type = time_atime;
          break;

        case 'v':
          sort_opt = sort_version;
          break;

        case 'w':
          width_opt = decode_line_length (optarg);
          if (width_opt < 0)
            error (LS_FAILURE, 0, "%s: %s", _("invalid line width"),
                   quote (optarg));
          break;

        case 'x':
          format_opt = horizontal;
          break;

        case 'A':
          ignore_mode = IGNORE_DOT_AND_DOTDOT;
          break;

        case 'B':
          add_ignore_pattern ("*~");
          add_ignore_pattern (".*~");
          break;

        case 'C':
          format_opt = many_per_line;
          break;

        case 'D':
          dired = true;
          break;

        case 'F':
          {
            int i;
            if (optarg)
              i = XARGMATCH ("--classify", optarg, when_args, when_types);
            else
               
              i = when_always;

            if (i == when_always || (i == when_if_tty && stdout_isatty ()))
              indicator_style = classify;
            break;
          }

        case 'G':		 
          print_group = false;
          break;

        case 'H':
          dereference = DEREF_COMMAND_LINE_ARGUMENTS;
          break;

        case DEREFERENCE_COMMAND_LINE_SYMLINK_TO_DIR_OPTION:
          dereference = DEREF_COMMAND_LINE_SYMLINK_TO_DIR;
          break;

        case 'I':
          add_ignore_pattern (optarg);
          break;

        case 'L':
          dereference = DEREF_ALWAYS;
          break;

        case 'N':
          quoting_style_opt = literal_quoting_style;
          break;

        case 'Q':
          quoting_style_opt = c_quoting_style;
          break;

        case 'R':
          recursive = true;
          break;

        case 'S':
          sort_opt = sort_size;
          break;

        case 'T':
          tabsize_opt = xnumtoumax (optarg, 0, 0, MIN (PTRDIFF_MAX, SIZE_MAX),
                                    "", _("invalid tab size"), LS_FAILURE);
          break;

        case 'U':
          sort_opt = sort_none;
          break;

        case 'X':
          sort_opt = sort_extension;
          break;

        case '1':
           
          if (format_opt != long_format)
            format_opt = one_per_line;
          break;

        case AUTHOR_OPTION:
          print_author = true;
          break;

        case HIDE_OPTION:
          {
            struct ignore_pattern *hide = xmalloc (sizeof *hide);
            hide->pattern = optarg;
            hide->next = hide_patterns;
            hide_patterns = hide;
          }
          break;

        case SORT_OPTION:
          sort_opt = XARGMATCH ("--sort", optarg, sort_args, sort_types);
          break;

        case GROUP_DIRECTORIES_FIRST_OPTION:
          directories_first = true;
          break;

        case TIME_OPTION:
          time_type = XARGMATCH ("--time", optarg, time_args, time_types);
          break;

        case FORMAT_OPTION:
          format_opt = XARGMATCH ("--format", optarg, format_args,
                                  format_types);
          break;

        case FULL_TIME_OPTION:
          format_opt = long_format;
          time_style_option = bad_cast ("full-iso");
          break;

        case COLOR_OPTION:
          {
            int i;
            if (optarg)
              i = XARGMATCH ("--color", optarg, when_args, when_types);
            else
               
              i = when_always;

            print_with_color = (i == when_always
                                || (i == when_if_tty && stdout_isatty ()));
            break;
          }

        case HYPERLINK_OPTION:
          {
            int i;
            if (optarg)
              i = XARGMATCH ("--hyperlink", optarg, when_args, when_types);
            else
               
              i = when_always;

            print_hyperlink = (i == when_always
                               || (i == when_if_tty && stdout_isatty ()));
            break;
          }

        case INDICATOR_STYLE_OPTION:
          indicator_style = XARGMATCH ("--indicator-style", optarg,
                                       indicator_style_args,
                                       indicator_style_types);
          break;

        case QUOTING_STYLE_OPTION:
          quoting_style_opt = XARGMATCH ("--quoting-style", optarg,
                                         quoting_style_args,
                                         quoting_style_vals);
          break;

        case TIME_STYLE_OPTION:
          time_style_option = optarg;
          break;

        case SHOW_CONTROL_CHARS_OPTION:
          hide_control_chars_opt = false;
          break;

        case BLOCK_SIZE_OPTION:
          {
            enum strtol_error e = human_options (optarg, &human_output_opts,
                                                 &output_block_size);
            if (e != LONGINT_OK)
              xstrtol_fatal (e, oi, 0, long_options, optarg);
            file_human_output_opts = human_output_opts;
            file_output_block_size = output_block_size;
          }
          break;

        case SI_OPTION:
          file_human_output_opts = human_output_opts =
            human_autoscale | human_SI;
          file_output_block_size = output_block_size = 1;
          break;

        case 'Z':
          print_scontext = true;
          break;

        case ZERO_OPTION:
          eolbyte = 0;
          hide_control_chars_opt = false;
          if (format_opt != long_format)
            format_opt = one_per_line;
          print_with_color = false;
          quoting_style_opt = literal_quoting_style;
          break;

        case_GETOPT_HELP_CHAR;

        case_GETOPT_VERSION_CHAR (PROGRAM_NAME, AUTHORS);

        default:
          usage (LS_FAILURE);
        }
    }

  if (! output_block_size)
    {
      char const *ls_block_size = getenv ("LS_BLOCK_SIZE");
      human_options (ls_block_size,
                     &human_output_opts, &output_block_size);
      if (ls_block_size || getenv ("BLOCK_SIZE"))
        {
          file_human_output_opts = human_output_opts;
          file_output_block_size = output_block_size;
        }
      if (kibibytes_specified)
        {
          human_output_opts = 0;
          output_block_size = 1024;
        }
    }

  format = (0 <= format_opt ? format_opt
            : ls_mode == LS_LS ? (stdout_isatty ()
                                  ? many_per_line : one_per_line)
            : ls_mode == LS_MULTI_COL ? many_per_line
            :   long_format);

   
  ptrdiff_t linelen = width_opt;
  if (format == many_per_line || format == horizontal || format == with_commas
      || print_with_color)
    {
#ifdef TIOCGWINSZ
      if (linelen < 0)
        {
           
# if 4 < __GNUC__ + (6 <= __GNUC_MINOR__)
#  pragma GCC diagnostic push
#  pragma GCC diagnostic ignored "-Wtype-limits"
# endif
          struct winsize ws;
          if (stdout_isatty ()
              && 0 <= ioctl (STDOUT_FILENO, TIOCGWINSZ, &ws)
              && 0 < ws.ws_col)
            linelen = ws.ws_col <= MIN (PTRDIFF_MAX, SIZE_MAX) ? ws.ws_col : 0;
# if 4 < __GNUC__ + (6 <= __GNUC_MINOR__)
#  pragma GCC diagnostic pop
# endif
        }
#endif
      if (linelen < 0)
        {
          char const *p = getenv ("COLUMNS");
          if (p && *p)
            {
              linelen = decode_line_length (p);
              if (linelen < 0)
                error (0, 0,
                       _("ignoring invalid width"
                         " in environment variable COLUMNS: %s"),
                       quote (p));
            }
        }
    }

  line_length = linelen < 0 ? 80 : linelen;

   
  max_idx = line_length / MIN_COLUMN_WIDTH;
   
  max_idx += line_length % MIN_COLUMN_WIDTH != 0;

  if (format == many_per_line || format == horizontal || format == with_commas)
    {
      if (0 <= tabsize_opt)
        tabsize = tabsize_opt;
      else
        {
          tabsize = 8;
          char const *p = getenv ("TABSIZE");
          if (p)
            {
              uintmax_t tmp;
              if (xstrtoumax (p, nullptr, 0, &tmp, "") == LONGINT_OK
                  && tmp <= SIZE_MAX)
                tabsize = tmp;
              else
                error (0, 0,
                       _("ignoring invalid tab size"
                         " in environment variable TABSIZE: %s"),
                       quote (p));
            }
        }
    }

  qmark_funny_chars = (hide_control_chars_opt < 0
                       ? ls_mode == LS_LS && stdout_isatty ()
                       : hide_control_chars_opt);

  int qs = quoting_style_opt;
  if (qs < 0)
    qs = getenv_quoting_style ();
  if (qs < 0)
    qs = (ls_mode == LS_LS
          ? (stdout_isatty () ? shell_escape_quoting_style : -1)
          : escape_quoting_style);
  if (0 <= qs)
    set_quoting_style (nullptr, qs);
  qs = get_quoting_style (nullptr);
  align_variable_outer_quotes
    = ((format == long_format
        || ((format == many_per_line || format == horizontal) && line_length))
       && (qs == shell_quoting_style
           || qs == shell_escape_quoting_style
           || qs == c_maybe_quoting_style));
  filename_quoting_options = clone_quoting_options (nullptr);
  if (qs == escape_quoting_style)
    set_char_quoting (filename_quoting_options, ' ', 1);
  if (file_type <= indicator_style)
    {
      char const *p;
      for (p = &"*=>@|"[indicator_style - file_type]; *p; p++)
        set_char_quoting (filename_quoting_options, *p, 1);
    }

  dirname_quoting_options = clone_quoting_options (nullptr);
  set_char_quoting (dirname_quoting_options, ':', 1);

   
  dired &= (format == long_format) & !print_hyperlink;

  if (eolbyte < dired)
    error (LS_FAILURE, 0, _("--dired and --zero are incompatible"));

   

  sort_type = (0 <= sort_opt ? sort_opt
               : (format != long_format
                  && (time_type == time_ctime || time_type == time_atime
                      || time_type == time_btime))
               ? sort_time : sort_name);

  if (format == long_format)
    {
      char *style = time_style_option;
      static char const posix_prefix[] = "posix-";

      if (! style)
        if (! (style = getenv ("TIME_STYLE")))
          style = bad_cast ("locale");

      while (STREQ_LEN (style, posix_prefix, sizeof posix_prefix - 1))
        {
          if (! hard_locale (LC_TIME))
            return optind;
          style += sizeof posix_prefix - 1;
        }

      if (*style == '+')
        {
          char *p0 = style + 1;
          char *p1 = strchr (p0, '\n');
          if (! p1)
            p1 = p0;
          else
            {
              if (strchr (p1 + 1, '\n'))
                error (LS_FAILURE, 0, _("invalid time style format %s"),
                       quote (p0));
              *p1++ = '\0';
            }
          long_time_format[0] = p0;
          long_time_format[1] = p1;
        }
      else
        {
          ptrdiff_t res = argmatch (style, time_style_args,
                                    (char const *) time_style_types,
                                    sizeof (*time_style_types));
          if (res < 0)
            {
               
              argmatch_invalid ("time style", style, res);

               
              fputs (_("Valid arguments are:\n"), stderr);
              char const *const *p = time_style_args;
              while (*p)
                fprintf (stderr, "  - [posix-]%s\n", *p++);
              fputs (_("  - +FORMAT (e.g., +%H:%M) for a 'date'-style"
                       " format\n"), stderr);
              usage (LS_FAILURE);
            }
          switch (res)
            {
            case full_iso_time_style:
              long_time_format[0] = long_time_format[1] =
                "%Y-%m-%d %H:%M:%S.%N %z";
              break;

            case long_iso_time_style:
              long_time_format[0] = long_time_format[1] = "%Y-%m-%d %H:%M";
              break;

            case iso_time_style:
              long_time_format[0] = "%Y-%m-%d ";
              long_time_format[1] = "%m-%d %H:%M";
              break;

            case locale_time_style:
              if (hard_locale (LC_TIME))
                {
                  for (int i = 0; i < 2; i++)
                    long_time_format[i] =
                      dcgettext (nullptr, long_time_format[i], LC_TIME);
                }
            }
        }

      abformat_init ();
    }

  return optind;
}

 

static bool
get_funky_string (char **dest, char const **src, bool equals_end,
                  size_t *output_count)
{
  char num;			 
  size_t count;			 
  enum {
    ST_GND, ST_BACKSLASH, ST_OCTAL, ST_HEX, ST_CARET, ST_END, ST_ERROR
  } state;
  char const *p;
  char *q;

  p = *src;			 
  q = *dest;			 

  count = 0;			 
  num = 0;

  state = ST_GND;		 
  while (state < ST_END)
    {
      switch (state)
        {
        case ST_GND:		 
          switch (*p)
            {
            case ':':
            case '\0':
              state = ST_END;	 
              break;
            case '\\':
              state = ST_BACKSLASH;  
              ++p;
              break;
            case '^':
              state = ST_CARET;  
              ++p;
              break;
            case '=':
              if (equals_end)
                {
                  state = ST_END;  
                  break;
                }
              FALLTHROUGH;
            default:
              *(q++) = *(p++);
              ++count;
              break;
            }
          break;

        case ST_BACKSLASH:	 
          switch (*p)
            {
            case '0':
            case '1':
            case '2':
            case '3':
            case '4':
            case '5':
            case '6':
            case '7':
              state = ST_OCTAL;	 
              num = *p - '0';
              break;
            case 'x':
            case 'X':
              state = ST_HEX;	 
              num = 0;
              break;
            case 'a':		 
              num = '\a';
              break;
            case 'b':		 
              num = '\b';
              break;
            case 'e':		 
              num = 27;
              break;
            case 'f':		 
              num = '\f';
              break;
            case 'n':		 
              num = '\n';
              break;
            case 'r':		 
              num = '\r';
              break;
            case 't':		 
              num = '\t';
              break;
            case 'v':		 
              num = '\v';
              break;
            case '?':		 
              num = 127;
              break;
            case '_':		 
              num = ' ';
              break;
            case '\0':		 
              state = ST_ERROR;	 
              break;
            default:		 
              num = *p;
              break;
            }
          if (state == ST_BACKSLASH)
            {
              *(q++) = num;
              ++count;
              state = ST_GND;
            }
          ++p;
          break;

        case ST_OCTAL:		 
          if (*p < '0' || *p > '7')
            {
              *(q++) = num;
              ++count;
              state = ST_GND;
            }
          else
            num = (num << 3) + (*(p++) - '0');
          break;

        case ST_HEX:		 
          switch (*p)
            {
            case '0':
            case '1':
            case '2':
            case '3':
            case '4':
            case '5':
            case '6':
            case '7':
            case '8':
            case '9':
              num = (num << 4) + (*(p++) - '0');
              break;
            case 'a':
            case 'b':
            case 'c':
            case 'd':
            case 'e':
            case 'f':
              num = (num << 4) + (*(p++) - 'a') + 10;
              break;
            case 'A':
            case 'B':
            case 'C':
            case 'D':
            case 'E':
            case 'F':
              num = (num << 4) + (*(p++) - 'A') + 10;
              break;
            default:
              *(q++) = num;
              ++count;
              state = ST_GND;
              break;
            }
          break;

        case ST_CARET:		 
          state = ST_GND;	 
          if (*p >= '@' && *p <= '~')
            {
              *(q++) = *(p++) & 037;
              ++count;
            }
          else if (*p == '?')
            {
              *(q++) = 127;
              ++count;
            }
          else
            state = ST_ERROR;
          break;

        default:
          unreachable ();
        }
    }

  *dest = q;
  *src = p;
  *output_count = count;

  return state != ST_ERROR;
}

enum parse_state
  {
    PS_START = 1,
    PS_2,
    PS_3,
    PS_4,
    PS_DONE,
    PS_FAIL
  };


 

static bool
known_term_type (void)
{
  char const *term = getenv ("TERM");
  if (! term || ! *term)
    return false;

  char const *line = G_line;
  while (line - G_line < sizeof (G_line))
    {
      if (STRNCMP_LIT (line, "TERM ") == 0)
        {
          if (fnmatch (line + 5, term, 0) == 0)
            return true;
        }
      line += strlen (line) + 1;
    }

  return false;
}

static void
parse_ls_color (void)
{
  char const *p;		 
  char *buf;			 
  int ind_no;			 
  char label[3];		 
  struct color_ext_type *ext;	 

  if ((p = getenv ("LS_COLORS")) == nullptr || *p == '\0')
    {
       
      char const *colorterm = getenv ("COLORTERM");
      if (! (colorterm && *colorterm) && ! known_term_type ())
        print_with_color = false;
      return;
    }

  ext = nullptr;
  strcpy (label, "??");

   
  buf = color_buf = xstrdup (p);

  enum parse_state state = PS_START;
  while (true)
    {
      switch (state)
        {
        case PS_START:		 
          switch (*p)
            {
            case ':':
              ++p;
              break;

            case '*':
               

              ext = xmalloc (sizeof *ext);
              ext->next = color_ext_list;
              color_ext_list = ext;
              ext->exact_match = false;

              ++p;
              ext->ext.string = buf;

              state = (get_funky_string (&buf, &p, true, &ext->ext.len)
                       ? PS_4 : PS_FAIL);
              break;

            case '\0':
              state = PS_DONE;	 
              goto done;

            default:	 
              label[0] = *(p++);
              state = PS_2;
              break;
            }
          break;

        case PS_2:		 
          if (*p)
            {
              label[1] = *(p++);
              state = PS_3;
            }
          else
            state = PS_FAIL;	 
          break;

        case PS_3:		 
          state = PS_FAIL;	 
          if (*(p++) == '=') 
            {
              for (ind_no = 0; indicator_name[ind_no] != nullptr; ++ind_no)
                {
                  if (STREQ (label, indicator_name[ind_no]))
                    {
                      color_indicator[ind_no].string = buf;
                      state = (get_funky_string (&buf, &p, false,
                                                 &color_indicator[ind_no].len)
                               ? PS_START : PS_FAIL);
                      break;
                    }
                }
              if (state == PS_FAIL)
                error (0, 0, _("unrecognized prefix: %s"), quote (label));
            }
          break;

        case PS_4:		 
          if (*(p++) == '=')
            {
              ext->seq.string = buf;
              state = (get_funky_string (&buf, &p, false, &ext->seq.len)
                       ? PS_START : PS_FAIL);
            }
          else
            state = PS_FAIL;
          break;

        case PS_FAIL:
          goto done;

        default:
          affirm (false);
        }
    }
 done:

  if (state == PS_FAIL)
    {
      struct color_ext_type *e;
      struct color_ext_type *e2;

      error (0, 0,
             _("unparsable value for LS_COLORS environment variable"));
      free (color_buf);
      for (e = color_ext_list; e != nullptr;  )
        {
          e2 = e;
          e = e->next;
          free (e2);
        }
      print_with_color = false;
    }
  else
    {
       
      struct color_ext_type *e1;

      for (e1 = color_ext_list; e1 != nullptr; e1 = e1->next)
        {
          struct color_ext_type *e2;
          bool case_ignored = false;

          for (e2 = e1->next; e2 != nullptr; e2 = e2->next)
            {
              if (e2->ext.len < SIZE_MAX && e1->ext.len == e2->ext.len)
                {
                  if (memcmp (e1->ext.string, e2->ext.string, e1->ext.len) == 0)
                    e2->ext.len = SIZE_MAX;  
                  else if (c_strncasecmp (e1->ext.string, e2->ext.string,
                                          e1->ext.len) == 0)
                    {
                      if (case_ignored)
                        {
                          e2->ext.len = SIZE_MAX;  
                        }
                      else if (e1->seq.len == e2->seq.len
                               && memcmp (e1->seq.string, e2->seq.string,
                                          e1->seq.len) == 0)
                        {
                          e2->ext.len = SIZE_MAX;  
                          case_ignored = true;     
                        }
                      else
                        {
                          e1->exact_match = true;
                          e2->exact_match = true;
                        }
                    }
                }
            }
        }
    }

  if (color_indicator[C_LINK].len == 6
      && !STRNCMP_LIT (color_indicator[C_LINK].string, "target"))
    color_symlink_as_referent = true;
}

 

static int
getenv_quoting_style (void)
{
  char const *q_style = getenv ("QUOTING_STYLE");
  if (!q_style)
    return -1;
  int i = ARGMATCH (q_style, quoting_style_args, quoting_style_vals);
  if (i < 0)
    {
      error (0, 0,
             _("ignoring invalid value"
               " of environment variable QUOTING_STYLE: %s"),
             quote (q_style));
      return -1;
    }
  return quoting_style_vals[i];
}

 

static void
set_exit_status (bool serious)
{
  if (serious)
    exit_status = LS_FAILURE;
  else if (exit_status == EXIT_SUCCESS)
    exit_status = LS_MINOR_PROBLEM;
}

 

static void
file_failure (bool serious, char const *message, char const *file)
{
  error (0, errno, message, quoteaf (file));
  set_exit_status (serious);
}

 

static void
queue_directory (char const *name, char const *realname, bool command_line_arg)
{
  struct pending *new = xmalloc (sizeof *new);
  new->realname = realname ? xstrdup (realname) : nullptr;
  new->name = name ? xstrdup (name) : nullptr;
  new->command_line_arg = command_line_arg;
  new->next = pending_dirs;
  pending_dirs = new;
}

 

static void
print_dir (char const *name, char const *realname, bool command_line_arg)
{
  DIR *dirp;
  struct dirent *next;
  uintmax_t total_blocks = 0;
  static bool first = true;

  errno = 0;
  dirp = opendir (name);
  if (!dirp)
    {
      file_failure (command_line_arg, _("cannot open directory %s"), name);
      return;
    }

  if (LOOP_DETECT)
    {
      struct stat dir_stat;
      int fd = dirfd (dirp);

       
      if ((0 <= fd
           ? fstat_for_ino (fd, &dir_stat)
           : stat_for_ino (name, &dir_stat)) < 0)
        {
          file_failure (command_line_arg,
                        _("cannot determine device and inode of %s"), name);
          closedir (dirp);
          return;
        }

       
      if (visit_dir (dir_stat.st_dev, dir_stat.st_ino))
        {
          error (0, 0, _("%s: not listing already-listed directory"),
                 quotef (name));
          closedir (dirp);
          set_exit_status (true);
          return;
        }

      dev_ino_push (dir_stat.st_dev, dir_stat.st_ino);
    }

  clear_files ();

  if (recursive || print_dir_name)
    {
      if (!first)
        dired_outbyte ('\n');
      first = false;
      dired_indent ();

      char *absolute_name = nullptr;
      if (print_hyperlink)
        {
          absolute_name = canonicalize_filename_mode (name, CAN_MISSING);
          if (! absolute_name)
            file_failure (command_line_arg,
                          _("error canonicalizing %s"), name);
        }
      quote_name (realname ? realname : name, dirname_quoting_options, -1,
                  nullptr, true, &subdired_obstack, absolute_name);

      free (absolute_name);

      dired_outstring (":\n");
    }

   

  while (true)
    {
       
      errno = 0;
      next = readdir (dirp);
      if (next)
        {
          if (! file_ignored (next->d_name))
            {
              enum filetype type = unknown;

#if HAVE_STRUCT_DIRENT_D_TYPE
              switch (next->d_type)
                {
                case DT_BLK:  type = blockdev;		break;
                case DT_CHR:  type = chardev;		break;
                case DT_DIR:  type = directory;		break;
                case DT_FIFO: type = fifo;		break;
                case DT_LNK:  type = symbolic_link;	break;
                case DT_REG:  type = normal;		break;
                case DT_SOCK: type = sock;		break;
# ifdef DT_WHT
                case DT_WHT:  type = whiteout;		break;
# endif
                }
#endif
              total_blocks += gobble_file (next->d_name, type,
                                           RELIABLE_D_INO (next),
                                           false, name);

               
              if (format == one_per_line && sort_type == sort_none
                      && !print_block_size && !recursive)
                {
                   
                  sort_files ();
                  print_current_files ();
                  clear_files ();
                }
            }
        }
      else if (errno != 0)
        {
          file_failure (command_line_arg, _("reading directory %s"), name);
          if (errno != EOVERFLOW)
            break;
        }
      else
        break;

       
      process_signals ();
    }

  if (closedir (dirp) != 0)
    {
      file_failure (command_line_arg, _("closing directory %s"), name);
       
    }

   
  sort_files ();

   

  if (recursive)
    extract_dirs_from_files (name, false);

  if (format == long_format || print_block_size)
    {
      char buf[LONGEST_HUMAN_READABLE + 3];
      char *p = human_readable (total_blocks, buf + 1, human_output_opts,
                                ST_NBLOCKSIZE, output_block_size);
      char *pend = p + strlen (p);
      *--p = ' ';
      *pend++ = eolbyte;
      dired_indent ();
      dired_outstring (_("total"));
      dired_outbuf (p, pend - p);
    }

  if (cwd_n_used)
    print_current_files ();
}

 

static void
add_ignore_pattern (char const *pattern)
{
  struct ignore_pattern *ignore;

  ignore = xmalloc (sizeof *ignore);
  ignore->pattern = pattern;
   
  ignore->next = ignore_patterns;
  ignore_patterns = ignore;
}

 

static bool
patterns_match (struct ignore_pattern const *patterns, char const *file)
{
  struct ignore_pattern const *p;
  for (p = patterns; p; p = p->next)
    if (fnmatch (p->pattern, file, FNM_PERIOD) == 0)
      return true;
  return false;
}

 

static bool
file_ignored (char const *name)
{
  return ((ignore_mode != IGNORE_MINIMAL
           && name[0] == '.'
           && (ignore_mode == IGNORE_DEFAULT || ! name[1 + (name[1] == '.')]))
          || (ignore_mode == IGNORE_DEFAULT
              && patterns_match (hide_patterns, name))
          || patterns_match (ignore_patterns, name));
}

 

static uintmax_t
unsigned_file_size (off_t size)
{
  return size + (size < 0) * ((uintmax_t) OFF_T_MAX - OFF_T_MIN + 1);
}

#ifdef HAVE_CAP
 
static bool
has_capability (char const *name)
{
  char *result;
  bool has_cap;

  cap_t cap_d = cap_get_file (name);
  if (cap_d == nullptr)
    return false;

  result = cap_to_text (cap_d, nullptr);
  cap_free (cap_d);
  if (!result)
    return false;

   
  has_cap = !!*result;

  cap_free (result);
  return has_cap;
}
#else
static bool
has_capability (MAYBE_UNUSED char const *name)
{
  errno = ENOTSUP;
  return false;
}
#endif

 

static void
free_ent (struct fileinfo *f)
{
  free (f->name);
  free (f->linkname);
  free (f->absolute_name);
  if (f->scontext != UNKNOWN_SECURITY_CONTEXT)
    {
      if (is_smack_enabled ())
        free (f->scontext);
      else
        freecon (f->scontext);
    }
}

 
static void
clear_files (void)
{
  for (size_t i = 0; i < cwd_n_used; i++)
    {
      struct fileinfo *f = sorted_file[i];
      free_ent (f);
    }

  cwd_n_used = 0;
  cwd_some_quoted = false;
  any_has_acl = false;
  inode_number_width = 0;
  block_size_width = 0;
  nlink_width = 0;
  owner_width = 0;
  group_width = 0;
  author_width = 0;
  scontext_width = 0;
  major_device_number_width = 0;
  minor_device_number_width = 0;
  file_size_width = 0;
}

 
static bool
errno_unsupported (int err)
{
  return (err == EINVAL || err == ENOSYS || is_ENOTSUP (err));
}

 
static int
getfilecon_cache (char const *file, struct fileinfo *f, bool deref)
{
   
  static dev_t unsupported_device;

  if (f->stat.st_dev == unsupported_device)
    {
      errno = ENOTSUP;
      return -1;
    }
  int r = 0;
#ifdef HAVE_SMACK
  if (is_smack_enabled ())
    r = smack_new_label_from_path (file, "security.SMACK64", deref,
                                   &f->scontext);
  else
#endif
    r = (deref
         ? getfilecon (file, &f->scontext)
         : lgetfilecon (file, &f->scontext));
  if (r < 0 && errno_unsupported (errno))
    unsupported_device = f->stat.st_dev;
  return r;
}

 
static int
file_has_acl_cache (char const *file, struct fileinfo *f)
{
   
  static dev_t unsupported_device;

  if (f->stat.st_dev == unsupported_device)
    {
      errno = ENOTSUP;
      return 0;
    }

   
  errno = 0;
  int n = file_has_acl (file, &f->stat);
  if (n <= 0 && errno_unsupported (errno))
    unsupported_device = f->stat.st_dev;
  return n;
}

 
static bool
has_capability_cache (char const *file, struct fileinfo *f)
{
   
  static dev_t unsupported_device;

  if (f->stat.st_dev == unsupported_device)
    {
      errno = ENOTSUP;
      return 0;
    }

  bool b = has_capability (file);
  if ( !b && errno_unsupported (errno))
    unsupported_device = f->stat.st_dev;
  return b;
}

static bool
needs_quoting (char const *name)
{
  char test[2];
  size_t len = quotearg_buffer (test, sizeof test , name, -1,
                                filename_quoting_options);
  return *name != *test || strlen (name) != len;
}

 
static uintmax_t
gobble_file (char const *name, enum filetype type, ino_t inode,
             bool command_line_arg, char const *dirname)
{
  uintmax_t blocks = 0;
  struct fileinfo *f;

   
  affirm (! command_line_arg || inode == NOT_AN_INODE_NUMBER);

  if (cwd_n_used == cwd_n_alloc)
    {
      cwd_file = xnrealloc (cwd_file, cwd_n_alloc, 2 * sizeof *cwd_file);
      cwd_n_alloc *= 2;
    }

  f = &cwd_file[cwd_n_used];
  memset (f, '\0', sizeof *f);
  f->stat.st_ino = inode;
  f->filetype = type;

  f->quoted = -1;
  if ((! cwd_some_quoted) && align_variable_outer_quotes)
    {
       
      f->quoted = needs_quoting (name);
      if (f->quoted)
        cwd_some_quoted = 1;
    }

  if (command_line_arg
      || print_hyperlink
      || format_needs_stat
       
      || (type == directory && print_with_color
          && (is_colored (C_OTHER_WRITABLE)
              || is_colored (C_STICKY)
              || is_colored (C_STICKY_OTHER_WRITABLE)))
       
      || ((print_inode || format_needs_type)
          && (type == symbolic_link || type == unknown)
          && (dereference == DEREF_ALWAYS
              || color_symlink_as_referent || check_symlink_mode))
       
      || (print_inode && inode == NOT_AN_INODE_NUMBER)
      || (format_needs_type
          && (type == unknown || command_line_arg
               
              || (type == normal && (indicator_style == classify
                                      
                                     || (print_with_color
                                         && (is_colored (C_EXEC)
                                             || is_colored (C_SETUID)
                                             || is_colored (C_SETGID)
                                             || is_colored (C_CAP)))
                                     )))))

    {
       
      char *full_name;
      bool do_deref;
      int err;

      if (name[0] == '/' || dirname[0] == 0)
        full_name = (char *) name;
      else
        {
          full_name = alloca (strlen (name) + strlen (dirname) + 2);
          attach (full_name, dirname, name);
        }

      if (print_hyperlink)
        {
          f->absolute_name = canonicalize_filename_mode (full_name,
                                                         CAN_MISSING);
          if (! f->absolute_name)
            file_failure (command_line_arg,
                          _("error canonicalizing %s"), full_name);
        }

      switch (dereference)
        {
        case DEREF_ALWAYS:
          err = do_stat (full_name, &f->stat);
          do_deref = true;
          break;

        case DEREF_COMMAND_LINE_ARGUMENTS:
        case DEREF_COMMAND_LINE_SYMLINK_TO_DIR:
          if (command_line_arg)
            {
              bool need_lstat;
              err = do_stat (full_name, &f->stat);
              do_deref = true;

              if (dereference == DEREF_COMMAND_LINE_ARGUMENTS)
                break;

              need_lstat = (err < 0
                            ? (errno == ENOENT || errno == ELOOP)
                            : ! S_ISDIR (f->stat.st_mode));
              if (!need_lstat)
                break;

               
            }
          FALLTHROUGH;

        default:  
          err = do_lstat (full_name, &f->stat);
          do_deref = false;
          break;
        }

      if (err != 0)
        {
           
          file_failure (command_line_arg,
                        _("cannot access %s"), full_name);

          f->scontext = UNKNOWN_SECURITY_CONTEXT;

          if (command_line_arg)
            return 0;

          f->name = xstrdup (name);
          cwd_n_used++;

          return 0;
        }

      f->stat_ok = true;

       
      if ((type == normal || S_ISREG (f->stat.st_mode))
          && print_with_color && is_colored (C_CAP))
        f->has_capability = has_capability_cache (full_name, f);

      if (format == long_format || print_scontext)
        {
          bool have_scontext = false;
          bool have_acl = false;
          int attr_len = getfilecon_cache (full_name, f, do_deref);
          err = (attr_len < 0);

          if (err == 0)
            {
              if (is_smack_enabled ())
                have_scontext = ! STREQ ("_", f->scontext);
              else
                have_scontext = ! STREQ ("unlabeled", f->scontext);
            }
          else
            {
              f->scontext = UNKNOWN_SECURITY_CONTEXT;

               
              if (is_ENOTSUP (errno) || errno == ENODATA)
                err = 0;
            }

          if (err == 0 && format == long_format)
            {
              int n = file_has_acl_cache (full_name, f);
              err = (n < 0);
              have_acl = (0 < n);
            }

          f->acl_type = (!have_scontext && !have_acl
                         ? ACL_T_NONE
                         : (have_scontext && !have_acl
                            ? ACL_T_LSM_CONTEXT_ONLY
                            : ACL_T_YES));
          any_has_acl |= f->acl_type != ACL_T_NONE;

          if (err)
            error (0, errno, "%s", quotef (full_name));
        }

      if (S_ISLNK (f->stat.st_mode)
          && (format == long_format || check_symlink_mode))
        {
          struct stat linkstats;

          get_link_name (full_name, f, command_line_arg);

           
          if (f->linkname && f->quoted == 0 && needs_quoting (f->linkname))
            f->quoted = -1;

           
          if (f->linkname
              && (file_type <= indicator_style || check_symlink_mode)
              && stat_for_mode (full_name, &linkstats) == 0)
            {
              f->linkok = true;
              f->linkmode = linkstats.st_mode;
            }
        }

      if (S_ISLNK (f->stat.st_mode))
        f->filetype = symbolic_link;
      else if (S_ISDIR (f->stat.st_mode))
        {
          if (command_line_arg && !immediate_dirs)
            f->filetype = arg_directory;
          else
            f->filetype = directory;
        }
      else
        f->filetype = normal;

      blocks = ST_NBLOCKS (f->stat);
      if (format == long_format || print_block_size)
        {
          char buf[LONGEST_HUMAN_READABLE + 1];
          int len = mbswidth (human_readable (blocks, buf, human_output_opts,
                                              ST_NBLOCKSIZE, output_block_size),
                              0);
          if (block_size_width < len)
            block_size_width = len;
        }

      if (format == long_format)
        {
          if (print_owner)
            {
              int len = format_user_width (f->stat.st_uid);
              if (owner_width < len)
                owner_width = len;
            }

          if (print_group)
            {
              int len = format_group_width (f->stat.st_gid);
              if (group_width < len)
                group_width = len;
            }

          if (print_author)
            {
              int len = format_user_width (f->stat.st_author);
              if (author_width < len)
                author_width = len;
            }
        }

      if (print_scontext)
        {
          int len = strlen (f->scontext);
          if (scontext_width < len)
            scontext_width = len;
        }

      if (format == long_format)
        {
          char b[INT_BUFSIZE_BOUND (uintmax_t)];
          int b_len = strlen (umaxtostr (f->stat.st_nlink, b));
          if (nlink_width < b_len)
            nlink_width = b_len;

          if (S_ISCHR (f->stat.st_mode) || S_ISBLK (f->stat.st_mode))
            {
              char buf[INT_BUFSIZE_BOUND (uintmax_t)];
              int len = strlen (umaxtostr (major (f->stat.st_rdev), buf));
              if (major_device_number_width < len)
                major_device_number_width = len;
              len = strlen (umaxtostr (minor (f->stat.st_rdev), buf));
              if (minor_device_number_width < len)
                minor_device_number_width = len;
              len = major_device_number_width + 2 + minor_device_number_width;
              if (file_size_width < len)
                file_size_width = len;
            }
          else
            {
              char buf[LONGEST_HUMAN_READABLE + 1];
              uintmax_t size = unsigned_file_size (f->stat.st_size);
              int len = mbswidth (human_readable (size, buf,
                                                  file_human_output_opts,
                                                  1, file_output_block_size),
                                  0);
              if (file_size_width < len)
                file_size_width = len;
            }
        }
    }

  if (print_inode)
    {
      char buf[INT_BUFSIZE_BOUND (uintmax_t)];
      int len = strlen (umaxtostr (f->stat.st_ino, buf));
      if (inode_number_width < len)
        inode_number_width = len;
    }

  f->name = xstrdup (name);
  cwd_n_used++;

  return blocks;
}

 
static bool
is_directory (const struct fileinfo *f)
{
  return f->filetype == directory || f->filetype == arg_directory;
}

 
static bool
is_linked_directory (const struct fileinfo *f)
{
  return f->filetype == directory || f->filetype == arg_directory
         || S_ISDIR (f->linkmode);
}

 

static void
get_link_name (char const *filename, struct fileinfo *f, bool command_line_arg)
{
  f->linkname = areadlink_with_size (filename, f->stat.st_size);
  if (f->linkname == nullptr)
    file_failure (command_line_arg, _("cannot read symbolic link %s"),
                  filename);
}

 

static bool
basename_is_dot_or_dotdot (char const *name)
{
  char const *base = last_component (name);
  return dot_or_dotdot (base);
}

 

static void
extract_dirs_from_files (char const *dirname, bool command_line_arg)
{
  size_t i;
  size_t j;
  bool ignore_dot_and_dot_dot = (dirname != nullptr);

  if (dirname && LOOP_DETECT)
    {
       
      queue_directory (nullptr, dirname, false);
    }

   
  for (i = cwd_n_used; i-- != 0; )
    {
      struct fileinfo *f = sorted_file[i];

      if (is_directory (f)
          && (! ignore_dot_and_dot_dot
              || ! basename_is_dot_or_dotdot (f->name)))
        {
          if (!dirname || f->name[0] == '/')
            queue_directory (f->name, f->linkname, command_line_arg);
          else
            {
              char *name = file_name_concat (dirname, f->name, nullptr);
              queue_directory (name, f->linkname, command_line_arg);
              free (name);
            }
          if (f->filetype == arg_directory)
            free_ent (f);
        }
    }

   

  for (i = 0, j = 0; i < cwd_n_used; i++)
    {
      struct fileinfo *f = sorted_file[i];
      sorted_file[j] = f;
      j += (f->filetype != arg_directory);
    }
  cwd_n_used = j;
}

 

static jmp_buf failed_strcoll;

static int
xstrcoll (char const *a, char const *b)
{
  int diff;
  errno = 0;
  diff = strcoll (a, b);
  if (errno)
    {
      error (0, errno, _("cannot compare file names %s and %s"),
             quote_n (0, a), quote_n (1, b));
      set_exit_status (false);
      longjmp (failed_strcoll, 1);
    }
  return diff;
}

 

typedef void const *V;
typedef int (*qsortFunc)(V a, V b);

 
static int
dirfirst_check (struct fileinfo const *a, struct fileinfo const *b,
                int (*cmp) (V, V))
{
  int diff = is_linked_directory (b) - is_linked_directory (a);
  return diff ? diff : cmp (a, b);
}

 
#define DEFINE_SORT_FUNCTIONS(key_name, key_cmp_func)			\
   					\
  static int xstrcoll_##key_name (V a, V b)				\
  { return key_cmp_func (a, b, xstrcoll); }				\
  ATTRIBUTE_PURE static int strcmp_##key_name (V a, V b)		\
  { return key_cmp_func (a, b, strcmp); }				\
                                                                        \
   					\
  static int rev_xstrcoll_##key_name (V a, V b)				\
  { return key_cmp_func (b, a, xstrcoll); }				\
  ATTRIBUTE_PURE static int rev_strcmp_##key_name (V a, V b)	\
  { return key_cmp_func (b, a, strcmp); }				\
                                                                        \
   					\
  static int xstrcoll_df_##key_name (V a, V b)				\
  { return dirfirst_check (a, b, xstrcoll_##key_name); }		\
  ATTRIBUTE_PURE static int strcmp_df_##key_name (V a, V b)		\
  { return dirfirst_check (a, b, strcmp_##key_name); }			\
                                                                        \
   					\
  static int rev_xstrcoll_df_##key_name (V a, V b)			\
  { return dirfirst_check (a, b, rev_xstrcoll_##key_name); }		\
  ATTRIBUTE_PURE static int rev_strcmp_df_##key_name (V a, V b)	\
  { return dirfirst_check (a, b, rev_strcmp_##key_name); }

static int
cmp_ctime (struct fileinfo const *a, struct fileinfo const *b,
           int (*cmp) (char const *, char const *))
{
  int diff = timespec_cmp (get_stat_ctime (&b->stat),
                           get_stat_ctime (&a->stat));
  return diff ? diff : cmp (a->name, b->name);
}

static int
cmp_mtime (struct fileinfo const *a, struct fileinfo const *b,
           int (*cmp) (char const *, char const *))
{
  int diff = timespec_cmp (get_stat_mtime (&b->stat),
                           get_stat_mtime (&a->stat));
  return diff ? diff : cmp (a->name, b->name);
}

static int
cmp_atime (struct fileinfo const *a, struct fileinfo const *b,
           int (*cmp) (char const *, char const *))
{
  int diff = timespec_cmp (get_stat_atime (&b->stat),
                           get_stat_atime (&a->stat));
  return diff ? diff : cmp (a->name, b->name);
}

static int
cmp_btime (struct fileinfo const *a, struct fileinfo const *b,
           int (*cmp) (char const *, char const *))
{
  int diff = timespec_cmp (get_stat_btime (&b->stat),
                           get_stat_btime (&a->stat));
  return diff ? diff : cmp (a->name, b->name);
}

static int
off_cmp (off_t a, off_t b)
{
  return (a > b) - (a < b);
}

static int
cmp_size (struct fileinfo const *a, struct fileinfo const *b,
          int (*cmp) (char const *, char const *))
{
  int diff = off_cmp (b->stat.st_size, a->stat.st_size);
  return diff ? diff : cmp (a->name, b->name);
}

static int
cmp_name (struct fileinfo const *a, struct fileinfo const *b,
          int (*cmp) (char const *, char const *))
{
  return cmp (a->name, b->name);
}

 

static int
cmp_extension (struct fileinfo const *a, struct fileinfo const *b,
               int (*cmp) (char const *, char const *))
{
  char const *base1 = strrchr (a->name, '.');
  char const *base2 = strrchr (b->name, '.');
  int diff = cmp (base1 ? base1 : "", base2 ? base2 : "");
  return diff ? diff : cmp (a->name, b->name);
}

 

static size_t
fileinfo_name_width (struct fileinfo const *f)
{
  return f->width
         ? f->width
         : quote_name_width (f->name, filename_quoting_options, f->quoted);
}

static int
cmp_width (struct fileinfo const *a, struct fileinfo const *b,
          int (*cmp) (char const *, char const *))
{
  int diff = fileinfo_name_width (a) - fileinfo_name_width (b);
  return diff ? diff : cmp (a->name, b->name);
}

DEFINE_SORT_FUNCTIONS (ctime, cmp_ctime)
DEFINE_SORT_FUNCTIONS (mtime, cmp_mtime)
DEFINE_SORT_FUNCTIONS (atime, cmp_atime)
DEFINE_SORT_FUNCTIONS (btime, cmp_btime)
DEFINE_SORT_FUNCTIONS (size, cmp_size)
DEFINE_SORT_FUNCTIONS (name, cmp_name)
DEFINE_SORT_FUNCTIONS (extension, cmp_extension)
DEFINE_SORT_FUNCTIONS (width, cmp_width)

 
static int
cmp_version (struct fileinfo const *a, struct fileinfo const *b)
{
  int diff = filevercmp (a->name, b->name);
  return diff ? diff : strcmp (a->name, b->name);
}

static int
xstrcoll_version (V a, V b)
{
  return cmp_version (a, b);
}
static int
rev_xstrcoll_version (V a, V b)
{
  return cmp_version (b, a);
}
static int
xstrcoll_df_version (V a, V b)
{
  return dirfirst_check (a, b, xstrcoll_version);
}
static int
rev_xstrcoll_df_version (V a, V b)
{
  return dirfirst_check (a, b, rev_xstrcoll_version);
}


 

#define LIST_SORTFUNCTION_VARIANTS(key_name)                        \
  {                                                                 \
    {                                                               \
      { xstrcoll_##key_name, xstrcoll_df_##key_name },              \
      { rev_xstrcoll_##key_name, rev_xstrcoll_df_##key_name },      \
    },                                                              \
    {                                                               \
      { strcmp_##key_name, strcmp_df_##key_name },                  \
      { rev_strcmp_##key_name, rev_strcmp_df_##key_name },          \
    }                                                               \
  }

static qsortFunc const sort_functions[][2][2][2] =
  {
    LIST_SORTFUNCTION_VARIANTS (name),
    LIST_SORTFUNCTION_VARIANTS (extension),
    LIST_SORTFUNCTION_VARIANTS (width),
    LIST_SORTFUNCTION_VARIANTS (size),

    {
      {
        { xstrcoll_version, xstrcoll_df_version },
        { rev_xstrcoll_version, rev_xstrcoll_df_version },
      },

       
      {
        { nullptr, nullptr },
        { nullptr, nullptr },
      }
    },

     
    LIST_SORTFUNCTION_VARIANTS (mtime),
    LIST_SORTFUNCTION_VARIANTS (ctime),
    LIST_SORTFUNCTION_VARIANTS (atime),
    LIST_SORTFUNCTION_VARIANTS (btime)
  };

 
static_assert (ARRAY_CARDINALITY (sort_functions)
               == sort_numtypes - 2 + time_numtypes);

 

static void
initialize_ordering_vector (void)
{
  for (size_t i = 0; i < cwd_n_used; i++)
    sorted_file[i] = &cwd_file[i];
}

 

static void
update_current_files_info (void)
{
   
  if (sort_type == sort_width
      || (line_length && (format == many_per_line || format == horizontal)))
    {
      size_t i;
      for (i = 0; i < cwd_n_used; i++)
        {
          struct fileinfo *f = sorted_file[i];
          f->width = fileinfo_name_width (f);
        }
    }
}

 

static void
sort_files (void)
{
  bool use_strcmp;

  if (sorted_file_alloc < cwd_n_used + cwd_n_used / 2)
    {
      free (sorted_file);
      sorted_file = xnmalloc (cwd_n_used, 3 * sizeof *sorted_file);
      sorted_file_alloc = 3 * cwd_n_used;
    }

  initialize_ordering_vector ();

  update_current_files_info ();

  if (sort_type == sort_none)
    return;

   

  if (! setjmp (failed_strcoll))
    use_strcmp = false;       
  else
    {
      use_strcmp = true;
      affirm (sort_type != sort_version);
      initialize_ordering_vector ();
    }

   
  mpsort ((void const **) sorted_file, cwd_n_used,
          sort_functions[sort_type + (sort_type == sort_time ? time_type : 0)]
                        [use_strcmp][sort_reverse]
                        [directories_first]);
}

 

static void
print_current_files (void)
{
  size_t i;

  switch (format)
    {
    case one_per_line:
      for (i = 0; i < cwd_n_used; i++)
        {
          print_file_name_and_frills (sorted_file[i], 0);
          putchar (eolbyte);
        }
      break;

    case many_per_line:
      if (! line_length)
        print_with_separator (' ');
      else
        print_many_per_line ();
      break;

    case horizontal:
      if (! line_length)
        print_with_separator (' ');
      else
        print_horizontal ();
      break;

    case with_commas:
      print_with_separator (',');
      break;

    case long_format:
      for (i = 0; i < cwd_n_used; i++)
        {
          set_normal_color ();
          print_long_format (sorted_file[i]);
          dired_outbyte (eolbyte);
        }
      break;
    }
}

 

static size_t
align_nstrftime (char *buf, size_t size, bool recent, struct tm const *tm,
                 timezone_t tz, int ns)
{
  char const *nfmt = (use_abformat
                      ? abformat[recent][tm->tm_mon]
                      : long_time_format[recent]);
  return nstrftime (buf, size, nfmt, tm, tz, ns);
}

 

static int
long_time_expected_width (void)
{
  static int width = -1;

  if (width < 0)
    {
      time_t epoch = 0;
      struct tm tm;
      char buf[TIME_STAMP_LEN_MAXIMUM + 1];

       
      if (localtime_rz (localtz, &epoch, &tm))
        {
          size_t len = align_nstrftime (buf, sizeof buf, false,
                                        &tm, localtz, 0);
          if (len != 0)
            width = mbsnwidth (buf, len, 0);
        }

      if (width < 0)
        width = 0;
    }

  return width;
}

 

static void
format_user_or_group (char const *name, uintmax_t id, int width)
{
  if (name)
    {
      int width_gap = width - mbswidth (name, 0);
      int pad = MAX (0, width_gap);
      dired_outstring (name);

      do
        dired_outbyte (' ');
      while (pad--);
    }
  else
    dired_pos += printf ("%*"PRIuMAX" ", width, id);
}

 

static void
format_user (uid_t u, int width, bool stat_ok)
{
  format_user_or_group (! stat_ok ? "?" :
                        (numeric_ids ? nullptr : getuser (u)), u, width);
}

 

static void
format_group (gid_t g, int width, bool stat_ok)
{
  format_user_or_group (! stat_ok ? "?" :
                        (numeric_ids ? nullptr : getgroup (g)), g, width);
}

 

static int
format_user_or_group_width (char const *name, uintmax_t id)
{
  if (name)
    {
      int len = mbswidth (name, 0);
      return MAX (0, len);
    }
  else
    return snprintf (nullptr, 0, "%"PRIuMAX, id);
}

 

static int
format_user_width (uid_t u)
{
  return format_user_or_group_width (numeric_ids ? nullptr : getuser (u), u);
}

 

static int
format_group_width (gid_t g)
{
  return format_user_or_group_width (numeric_ids ? nullptr : getgroup (g), g);
}

 
static char *
format_inode (char buf[INT_BUFSIZE_BOUND (uintmax_t)],
              const struct fileinfo *f)
{
  return (f->stat_ok && f->stat.st_ino != NOT_AN_INODE_NUMBER
          ? umaxtostr (f->stat.st_ino, buf)
          : (char *) "?");
}

 
static void
print_long_format (const struct fileinfo *f)
{
  char modebuf[12];
  char buf
    [LONGEST_HUMAN_READABLE + 1		 
     + LONGEST_HUMAN_READABLE + 1	 
     + sizeof (modebuf) - 1 + 1		 
     + INT_BUFSIZE_BOUND (uintmax_t)	 
     + LONGEST_HUMAN_READABLE + 2	 
     + LONGEST_HUMAN_READABLE + 1	 
     + TIME_STAMP_LEN_MAXIMUM + 1	 
     ];
  size_t s;
  char *p;
  struct timespec when_timespec;
  struct tm when_local;
  bool btime_ok = true;

   
  if (f->stat_ok)
    filemodestring (&f->stat, modebuf);
  else
    {
      modebuf[0] = filetype_letter[f->filetype];
      memset (modebuf + 1, '?', 10);
      modebuf[11] = '\0';
    }
  if (! any_has_acl)
    modebuf[10] = '\0';
  else if (f->acl_type == ACL_T_LSM_CONTEXT_ONLY)
    modebuf[10] = '.';
  else if (f->acl_type == ACL_T_YES)
    modebuf[10] = '+';

  switch (time_type)
    {
    case time_ctime:
      when_timespec = get_stat_ctime (&f->stat);
      break;
    case time_mtime:
      when_timespec = get_stat_mtime (&f->stat);
      break;
    case time_atime:
      when_timespec = get_stat_atime (&f->stat);
      break;
    case time_btime:
      when_timespec = get_stat_btime (&f->stat);
      if (when_timespec.tv_sec == -1 && when_timespec.tv_nsec == -1)
        btime_ok = false;
      break;
    default:
      unreachable ();
    }

  p = buf;

  if (print_inode)
    {
      char hbuf[INT_BUFSIZE_BOUND (uintmax_t)];
      p += sprintf (p, "%*s ", inode_number_width, format_inode (hbuf, f));
    }

  if (print_block_size)
    {
      char hbuf[LONGEST_HUMAN_READABLE + 1];
      char const *blocks =
        (! f->stat_ok
         ? "?"
         : human_readable (ST_NBLOCKS (f->stat), hbuf, human_output_opts,
                           ST_NBLOCKSIZE, output_block_size));
      int pad;
      for (pad = block_size_width - mbswidth (blocks, 0); 0 < pad; pad--)
        *p++ = ' ';
      while ((*p++ = *blocks++))
        continue;
      p[-1] = ' ';
    }

   
  {
    char hbuf[INT_BUFSIZE_BOUND (uintmax_t)];
    p += sprintf (p, "%s %*s ", modebuf, nlink_width,
                  ! f->stat_ok ? "?" : umaxtostr (f->stat.st_nlink, hbuf));
  }

  dired_indent ();

  if (print_owner || print_group || print_author || print_scontext)
    {
      dired_outbuf (buf, p - buf);

      if (print_owner)
        format_user (f->stat.st_uid, owner_width, f->stat_ok);

      if (print_group)
        format_group (f->stat.st_gid, group_width, f->stat_ok);

      if (print_author)
        format_user (f->stat.st_author, author_width, f->stat_ok);

      if (print_scontext)
        format_user_or_group (f->scontext, 0, scontext_width);

      p = buf;
    }

  if (f->stat_ok
      && (S_ISCHR (f->stat.st_mode) || S_ISBLK (f->stat.st_mode)))
    {
      char majorbuf[INT_BUFSIZE_BOUND (uintmax_t)];
      char minorbuf[INT_BUFSIZE_BOUND (uintmax_t)];
      int blanks_width = (file_size_width
                          - (major_device_number_width + 2
                             + minor_device_number_width));
      p += sprintf (p, "%*s, %*s ",
                    major_device_number_width + MAX (0, blanks_width),
                    umaxtostr (major (f->stat.st_rdev), majorbuf),
                    minor_device_number_width,
                    umaxtostr (minor (f->stat.st_rdev), minorbuf));
    }
  else
    {
      char hbuf[LONGEST_HUMAN_READABLE + 1];
      char const *size =
        (! f->stat_ok
         ? "?"
         : human_readable (unsigned_file_size (f->stat.st_size),
                           hbuf, file_human_output_opts, 1,
                           file_output_block_size));
      int pad;
      for (pad = file_size_width - mbswidth (size, 0); 0 < pad; pad--)
        *p++ = ' ';
      while ((*p++ = *size++))
        continue;
      p[-1] = ' ';
    }

  s = 0;
  *p = '\1';

  if (f->stat_ok && btime_ok
      && localtime_rz (localtz, &when_timespec.tv_sec, &when_local))
    {
      struct timespec six_months_ago;
      bool recent;

       
      if (timespec_cmp (current_time, when_timespec) < 0)
        gettime (&current_time);

       
      six_months_ago.tv_sec = current_time.tv_sec - 31556952 / 2;
      six_months_ago.tv_nsec = current_time.tv_nsec;

      recent = (timespec_cmp (six_months_ago, when_timespec) < 0
                && timespec_cmp (when_timespec, current_time) < 0);

       
      s = align_nstrftime (p, TIME_STAMP_LEN_MAXIMUM + 1, recent,
                           &when_local, localtz, when_timespec.tv_nsec);
    }

  if (s || !*p)
    {
      p += s;
      *p++ = ' ';
    }
  else
    {
       
      char hbuf[INT_BUFSIZE_BOUND (intmax_t)];
      p += sprintf (p, "%*s ", long_time_expected_width (),
                    (! f->stat_ok || ! btime_ok
                     ? "?"
                     : timetostr (when_timespec.tv_sec, hbuf)));
       
    }

  dired_outbuf (buf, p - buf);
  size_t w = print_name_with_quoting (f, false, &dired_obstack, p - buf);

  if (f->filetype == symbolic_link)
    {
      if (f->linkname)
        {
          dired_outstring (" -> ");
          print_name_with_quoting (f, true, nullptr, (p - buf) + w + 4);
          if (indicator_style != none)
            print_type_indicator (true, f->linkmode, unknown);
        }
    }
  else if (indicator_style != none)
    print_type_indicator (f->stat_ok, f->stat.st_mode, f->filetype);
}

 

static size_t
quote_name_buf (char **inbuf, size_t bufsize, char *name,
                struct quoting_options const *options,
                int needs_general_quoting, size_t *width, bool *pad)
{
  char *buf = *inbuf;
  size_t displayed_width IF_LINT ( = 0);
  size_t len = 0;
  bool quoted;

  enum quoting_style qs = get_quoting_style (options);
  bool needs_further_quoting = qmark_funny_chars
                               && (qs == shell_quoting_style
                                   || qs == shell_always_quoting_style
                                   || qs == literal_quoting_style);

  if (needs_general_quoting != 0)
    {
      len = quotearg_buffer (buf, bufsize, name, -1, options);
      if (bufsize <= len)
        {
          buf = xmalloc (len + 1);
          quotearg_buffer (buf, len + 1, name, -1, options);
        }

      quoted = (*name != *buf) || strlen (name) != len;
    }
  else if (needs_further_quoting)
    {
      len = strlen (name);
      if (bufsize <= len)
        buf = xmalloc (len + 1);
      memcpy (buf, name, len + 1);

      quoted = false;
    }
  else
    {
      len = strlen (name);
      buf = name;
      quoted = false;
    }

  if (needs_further_quoting)
    {
      if (MB_CUR_MAX > 1)
        {
          char const *p = buf;
          char const *plimit = buf + len;
          char *q = buf;
          displayed_width = 0;

          while (p < plimit)
            switch (*p)
              {
                case ' ': case '!': case '"': case '#': case '%':
                case '&': case '\'': case '(': case ')': case '*':
                case '+': case ',': case '-': case '.': case '/':
                case '0': case '1': case '2': case '3': case '4':
                case '5': case '6': case '7': case '8': case '9':
                case ':': case ';': case '<': case '=': case '>':
                case '?':
                case 'A': case 'B': case 'C': case 'D': case 'E':
                case 'F': case 'G': case 'H': case 'I': case 'J':
                case 'K': case 'L': case 'M': case 'N': case 'O':
                case 'P': case 'Q': case 'R': case 'S': case 'T':
                case 'U': case 'V': case 'W': case 'X': case 'Y':
                case 'Z':
                case '[': case '\\': case ']': case '^': case '_':
                case 'a': case 'b': case 'c': case 'd': case 'e':
                case 'f': case 'g': case 'h': case 'i': case 'j':
                case 'k': case 'l': case 'm': case 'n': case 'o':
                case 'p': case 'q': case 'r': case 's': case 't':
                case 'u': case 'v': case 'w': case 'x': case 'y':
                case 'z': case '{': case '|': case '}': case '~':
                   
                  *q++ = *p++;
                  displayed_width += 1;
                  break;
                default:
                   
                  {
                    mbstate_t mbstate = { 0, };
                    do
                      {
                        wchar_t wc;
                        size_t bytes;
                        int w;

                        bytes = mbrtowc (&wc, p, plimit - p, &mbstate);

                        if (bytes == (size_t) -1)
                          {
                             
                            p++;
                            *q++ = '?';
                            displayed_width += 1;
                            break;
                          }

                        if (bytes == (size_t) -2)
                          {
                             
                            p = plimit;
                            *q++ = '?';
                            displayed_width += 1;
                            break;
                          }

                        if (bytes == 0)
                           
                          bytes = 1;

                        w = wcwidth (wc);
                        if (w >= 0)
                          {
                             
                            for (; bytes > 0; --bytes)
                              *q++ = *p++;
                            displayed_width += w;
                          }
                        else
                          {
                             
                            p += bytes;
                            *q++ = '?';
                            displayed_width += 1;
                          }
                      }
                    while (! mbsinit (&mbstate));
                  }
                  break;
              }

           
          len = q - buf;
        }
      else
        {
          char *p = buf;
          char const *plimit = buf + len;

          while (p < plimit)
            {
              if (! isprint (to_uchar (*p)))
                *p = '?';
              p++;
            }
          displayed_width = len;
        }
    }
  else if (width != nullptr)
    {
      if (MB_CUR_MAX > 1)
        displayed_width = mbsnwidth (buf, len, 0);
      else
        {
          char const *p = buf;
          char const *plimit = buf + len;

          displayed_width = 0;
          while (p < plimit)
            {
              if (isprint (to_uchar (*p)))
                displayed_width++;
              p++;
            }
        }
    }

   
  *pad = (align_variable_outer_quotes && cwd_some_quoted && ! quoted);

  if (width != nullptr)
    *width = displayed_width;

  *inbuf = buf;

  return len;
}

static size_t
quote_name_width (char const *name, struct quoting_options const *options,
                  int needs_general_quoting)
{
  char smallbuf[BUFSIZ];
  char *buf = smallbuf;
  size_t width;
  bool pad;

  quote_name_buf (&buf, sizeof smallbuf, (char *) name, options,
                  needs_general_quoting, &width, &pad);

  if (buf != smallbuf && buf != name)
    free (buf);

  width += pad;

  return width;
}

 
static char *
file_escape (char const *str, bool path)
{
  char *esc = xnmalloc (3, strlen (str) + 1);
  char *p = esc;
  while (*str)
    {
      if (path && ISSLASH (*str))
        {
          *p++ = '/';
          str++;
        }
      else if (RFC3986[to_uchar (*str)])
        *p++ = *str++;
      else
        p += sprintf (p, "%%%02x", to_uchar (*str++));
    }
  *p = '\0';
  return esc;
}

static size_t
quote_name (char const *name, struct quoting_options const *options,
            int needs_general_quoting, const struct bin_str *color,
            bool allow_pad, struct obstack *stack, char const *absolute_name)
{
  char smallbuf[BUFSIZ];
  char *buf = smallbuf;
  size_t len;
  bool pad;

  len = quote_name_buf (&buf, sizeof smallbuf, (char *) name, options,
                        needs_general_quoting, nullptr, &pad);

  if (pad && allow_pad)
    dired_outbyte (' ');

  if (color)
    print_color_indicator (color);

   
  bool skip_quotes = false;

  if (absolute_name)
    {
      if (align_variable_outer_quotes && cwd_some_quoted && ! pad)
        {
          skip_quotes = true;
          putchar (*buf);
        }
      char *h = file_escape (hostname,   false);
      char *n = file_escape (absolute_name,   true);
       
      printf ("\033]8;;file:
      free (h);
      free (n);
    }

  if (stack)
    push_current_dired_pos (stack);

  fwrite (buf + skip_quotes, 1, len - (skip_quotes * 2), stdout);

  dired_pos += len;

  if (stack)
    push_current_dired_pos (stack);

  if (absolute_name)
    {
      fputs ("\033]8;;\a", stdout);
      if (skip_quotes)
        putchar (*(buf + len - 1));
    }

  if (buf != smallbuf && buf != name)
    free (buf);

  return len + pad;
}

static size_t
print_name_with_quoting (const struct fileinfo *f,
                         bool symlink_target,
                         struct obstack *stack,
                         size_t start_col)
{
  char const *name = symlink_target ? f->linkname : f->name;

  const struct bin_str *color
    = print_with_color ? get_color_indicator (f, symlink_target) : nullptr;

  bool used_color_this_time = (print_with_color
                               && (color || is_colored (C_NORM)));

  size_t len = quote_name (name, filename_quoting_options, f->quoted,
                           color, !symlink_target, stack, f->absolute_name);

  process_signals ();
  if (used_color_this_time)
    {
      prep_non_filename_text ();

      /* We use the byte length rather than display width here as
         an optimization to avoid accurately calculating the width,
         because we only output the clear to EOL sequence if the name
         _might_ wrap to the next line.  This may output a sequence
         unnecessarily in multi-byte locales for example,
         but in that case it's inconsequential to the output.  */
      if (line_length
          && (start_col / line_length != (start_col + len - 1) / line_length))
        put_indicator (&color_indicator[C_CLR_TO_EOL]);
    }

  return len;
}

static void
prep_non_filename_text (void)
{
  if (color_indicator[C_END].string != nullptr)
    put_indicator (&color_indicator[C_END]);
  else
    {
      put_indicator (&color_indicator[C_LEFT]);
      put_indicator (&color_indicator[C_RESET]);
      put_indicator (&color_indicator[C_RIGHT]);
    }
}

/* Print the file name of 'f' with appropriate quoting.
   Also print file size, inode number, and filetype indicator character,
   as requested by switches.  */

static size_t
print_file_name_and_frills (const struct fileinfo *f, size_t start_col)
{
  char buf[MAX (LONGEST_HUMAN_READABLE + 1, INT_BUFSIZE_BOUND (uintmax_t))];

  set_normal_color ();

  if (print_inode)
    printf ("%*s ", format == with_commas ? 0 : inode_number_width,
            format_inode (buf, f));

  if (print_block_size)
    printf ("%*s ", format == with_commas ? 0 : block_size_width,
            ! f->stat_ok ? "?"
            : human_readable (ST_NBLOCKS (f->stat), buf, human_output_opts,
                              ST_NBLOCKSIZE, output_block_size));

  if (print_scontext)
    printf ("%*s ", format == with_commas ? 0 : scontext_width, f->scontext);

  size_t width = print_name_with_quoting (f, false, nullptr, start_col);

  if (indicator_style != none)
    width += print_type_indicator (f->stat_ok, f->stat.st_mode, f->filetype);

  return width;
}

/* Given these arguments describing a file, return the single-byte
   type indicator, or 0.  */
static char
get_type_indicator (bool stat_ok, mode_t mode, enum filetype type)
{
  char c;

  if (stat_ok ? S_ISREG (mode) : type == normal)
    {
      if (stat_ok && indicator_style == classify && (mode & S_IXUGO))
        c = '*';
      else
        c = 0;
    }
  else
    {
      if (stat_ok ? S_ISDIR (mode) : type == directory || type == arg_directory)
        c = '/';
      else if (indicator_style == slash)
        c = 0;
      else if (stat_ok ? S_ISLNK (mode) : type == symbolic_link)
        c = '@';
      else if (stat_ok ? S_ISFIFO (mode) : type == fifo)
        c = '|';
      else if (stat_ok ? S_ISSOCK (mode) : type == sock)
        c = '=';
      else if (stat_ok && S_ISDOOR (mode))
        c = '>';
      else
        c = 0;
    }
  return c;
}

static bool
print_type_indicator (bool stat_ok, mode_t mode, enum filetype type)
{
  char c = get_type_indicator (stat_ok, mode, type);
  if (c)
    dired_outbyte (c);
  return !!c;
}

/* Returns if color sequence was printed.  */
static bool
print_color_indicator (const struct bin_str *ind)
{
  if (ind)
    {
      /* Need to reset so not dealing with attribute combinations */
      if (is_colored (C_NORM))
        restore_default_color ();
      put_indicator (&color_indicator[C_LEFT]);
      put_indicator (ind);
      put_indicator (&color_indicator[C_RIGHT]);
    }

  return ind != nullptr;
}

/* Returns color indicator or nullptr if none.  */
ATTRIBUTE_PURE
static const struct bin_str*
get_color_indicator (const struct fileinfo *f, bool symlink_target)
{
  enum indicator_no type;
  struct color_ext_type *ext;	/* Color extension */
  size_t len;			/* Length of name */

  char const *name;
  mode_t mode;
  int linkok;
  if (symlink_target)
    {
      name = f->linkname;
      mode = f->linkmode;
      linkok = f->linkok ? 0 : -1;
    }
  else
    {
      name = f->name;
      mode = file_or_link_mode (f);
      linkok = f->linkok;
    }

  /* Is this a nonexistent file?  If so, linkok == -1.  */

  if (linkok == -1 && is_colored (C_MISSING))
    type = C_MISSING;
  else if (!f->stat_ok)
    {
      static enum indicator_no filetype_indicator[] = FILETYPE_INDICATORS;
      type = filetype_indicator[f->filetype];
    }
  else
    {
      if (S_ISREG (mode))
        {
          type = C_FILE;

          if ((mode & S_ISUID) != 0 && is_colored (C_SETUID))
            type = C_SETUID;
          else if ((mode & S_ISGID) != 0 && is_colored (C_SETGID))
            type = C_SETGID;
          else if (is_colored (C_CAP) && f->has_capability)
            type = C_CAP;
          else if ((mode & S_IXUGO) != 0 && is_colored (C_EXEC))
            type = C_EXEC;
          else if ((1 < f->stat.st_nlink) && is_colored (C_MULTIHARDLINK))
            type = C_MULTIHARDLINK;
        }
      else if (S_ISDIR (mode))
        {
          type = C_DIR;

          if ((mode & S_ISVTX) && (mode & S_IWOTH)
              && is_colored (C_STICKY_OTHER_WRITABLE))
            type = C_STICKY_OTHER_WRITABLE;
          else if ((mode & S_IWOTH) != 0 && is_colored (C_OTHER_WRITABLE))
            type = C_OTHER_WRITABLE;
          else if ((mode & S_ISVTX) != 0 && is_colored (C_STICKY))
            type = C_STICKY;
        }
      else if (S_ISLNK (mode))
        type = C_LINK;
      else if (S_ISFIFO (mode))
        type = C_FIFO;
      else if (S_ISSOCK (mode))
        type = C_SOCK;
      else if (S_ISBLK (mode))
        type = C_BLK;
      else if (S_ISCHR (mode))
        type = C_CHR;
      else if (S_ISDOOR (mode))
        type = C_DOOR;
      else
        {
          /* Classify a file of some other type as C_ORPHAN.  */
          type = C_ORPHAN;
        }
    }

  /* Check the file's suffix only if still classified as C_FILE.  */
  ext = nullptr;
  if (type == C_FILE)
    {
      /* Test if NAME has a recognized suffix.  */

      len = strlen (name);
      name += len;		/* Pointer to final \0.  */
      for (ext = color_ext_list; ext != nullptr; ext = ext->next)
        {
          if (ext->ext.len <= len)
            {
              if (ext->exact_match)
                {
                  if (STREQ_LEN (name - ext->ext.len, ext->ext.string,
                                 ext->ext.len))
                    break;
                }
              else
                {
                  if (c_strncasecmp (name - ext->ext.len, ext->ext.string,
                                     ext->ext.len) == 0)
                    break;
                }
            }
        }
    }

  /* Adjust the color for orphaned symlinks.  */
  if (type == C_LINK && !linkok)
    {
      if (color_symlink_as_referent || is_colored (C_ORPHAN))
        type = C_ORPHAN;
    }

  const struct bin_str *const s
    = ext ? &(ext->seq) : &color_indicator[type];

  return s->string ? s : nullptr;
}

/* Output a color indicator (which may contain nulls).  */
static void
put_indicator (const struct bin_str *ind)
{
  if (! used_color)
    {
      used_color = true;

      /* If the standard output is a controlling terminal, watch out
         for signals, so that the colors can be restored to the
         default state if "ls" is suspended or interrupted.  */

      if (0 <= tcgetpgrp (STDOUT_FILENO))
        signal_init ();

      prep_non_filename_text ();
    }

  fwrite (ind->string, ind->len, 1, stdout);
}

static size_t
length_of_file_name_and_frills (const struct fileinfo *f)
{
  size_t len = 0;
  char buf[MAX (LONGEST_HUMAN_READABLE + 1, INT_BUFSIZE_BOUND (uintmax_t))];

  if (print_inode)
    len += 1 + (format == with_commas
                ? strlen (umaxtostr (f->stat.st_ino, buf))
                : inode_number_width);

  if (print_block_size)
    len += 1 + (format == with_commas
                ? strlen (! f->stat_ok ? "?"
                          : human_readable (ST_NBLOCKS (f->stat), buf,
                                            human_output_opts, ST_NBLOCKSIZE,
                                            output_block_size))
                : block_size_width);

  if (print_scontext)
    len += 1 + (format == with_commas ? strlen (f->scontext) : scontext_width);

  len += fileinfo_name_width (f);

  if (indicator_style != none)
    {
      char c = get_type_indicator (f->stat_ok, f->stat.st_mode, f->filetype);
      len += (c != 0);
    }

  return len;
}

static void
print_many_per_line (void)
{
  size_t row;			/* Current row.  */
  size_t cols = calculate_columns (true);
  struct column_info const *line_fmt = &column_info[cols - 1];

  /* Calculate the number of rows that will be in each column except possibly
     for a short column on the right.  */
  size_t rows = cwd_n_used / cols + (cwd_n_used % cols != 0);

  for (row = 0; row < rows; row++)
    {
      size_t col = 0;
      size_t filesno = row;
      size_t pos = 0;

      /* Print the next row.  */
      while (true)
        {
          struct fileinfo const *f = sorted_file[filesno];
          size_t name_length = length_of_file_name_and_frills (f);
          size_t max_name_length = line_fmt->col_arr[col++];
          print_file_name_and_frills (f, pos);

          filesno += rows;
          if (filesno >= cwd_n_used)
            break;

          indent (pos + name_length, pos + max_name_length);
          pos += max_name_length;
        }
      putchar (eolbyte);
    }
}

static void
print_horizontal (void)
{
  size_t filesno;
  size_t pos = 0;
  size_t cols = calculate_columns (false);
  struct column_info const *line_fmt = &column_info[cols - 1];
  struct fileinfo const *f = sorted_file[0];
  size_t name_length = length_of_file_name_and_frills (f);
  size_t max_name_length = line_fmt->col_arr[0];

  /* Print first entry.  */
  print_file_name_and_frills (f, 0);

  /* Now the rest.  */
  for (filesno = 1; filesno < cwd_n_used; ++filesno)
    {
      size_t col = filesno % cols;

      if (col == 0)
        {
          putchar (eolbyte);
          pos = 0;
        }
      else
        {
          indent (pos + name_length, pos + max_name_length);
          pos += max_name_length;
        }

      f = sorted_file[filesno];
      print_file_name_and_frills (f, pos);

      name_length = length_of_file_name_and_frills (f);
      max_name_length = line_fmt->col_arr[col];
    }
  putchar (eolbyte);
}

/* Output name + SEP + ' '.  */

static void
print_with_separator (char sep)
{
  size_t filesno;
  size_t pos = 0;

  for (filesno = 0; filesno < cwd_n_used; filesno++)
    {
      struct fileinfo const *f = sorted_file[filesno];
      size_t len = line_length ? length_of_file_name_and_frills (f) : 0;

      if (filesno != 0)
        {
          char separator;

          if (! line_length
              || ((pos + len + 2 < line_length)
                  && (pos <= SIZE_MAX - len - 2)))
            {
              pos += 2;
              separator = ' ';
            }
          else
            {
              pos = 0;
              separator = eolbyte;
            }

          putchar (sep);
          putchar (separator);
        }

      print_file_name_and_frills (f, pos);
      pos += len;
    }
  putchar (eolbyte);
}

/* Assuming cursor is at position FROM, indent up to position TO.
   Use a TAB character instead of two or more spaces whenever possible.  */

static void
indent (size_t from, size_t to)
{
  while (from < to)
    {
      if (tabsize != 0 && to / tabsize > (from + 1) / tabsize)
        {
          putchar ('\t');
          from += tabsize - from % tabsize;
        }
      else
        {
          putchar (' ');
          from++;
        }
    }
}

/* Put DIRNAME/NAME into DEST, handling '.' and '/' properly.  */
/* FIXME: maybe remove this function someday.  See about using a
   non-malloc'ing version of file_name_concat.  */

static void
attach (char *dest, char const *dirname, char const *name)
{
  char const *dirnamep = dirname;

  /* Copy dirname if it is not ".".  */
  if (dirname[0] != '.' || dirname[1] != 0)
    {
      while (*dirnamep)
        *dest++ = *dirnamep++;
      /* Add '/' if 'dirname' doesn't already end with it.  */
      if (dirnamep > dirname && dirnamep[-1] != '/')
        *dest++ = '/';
    }
  while (*name)
    *dest++ = *name++;
  *dest = 0;
}

/* Allocate enough column info suitable for the current number of
   files and display columns, and initialize the info to represent the
   narrowest possible columns.  */

static void
init_column_info (size_t max_cols)
{
  size_t i;

  /* Currently allocated columns in column_info.  */
  static size_t column_info_alloc;

  if (column_info_alloc < max_cols)
    {
      size_t new_column_info_alloc;
      size_t *p;

      if (!max_idx || max_cols < max_idx / 2)
        {
          /* The number of columns is far less than the display width
             allows.  Grow the allocation, but only so that it's
             double the current requirements.  If the display is
             extremely wide, this avoids allocating a lot of memory
             that is never needed.  */
          column_info = xnrealloc (column_info, max_cols,
                                   2 * sizeof *column_info);
          new_column_info_alloc = 2 * max_cols;
        }
      else
        {
          column_info = xnrealloc (column_info, max_idx, sizeof *column_info);
          new_column_info_alloc = max_idx;
        }

      /* Allocate the new size_t objects by computing the triangle
         formula n * (n + 1) / 2, except that we don't need to
         allocate the part of the triangle that we've already
         allocated.  Check for address arithmetic overflow.  */
      {
        size_t column_info_growth = new_column_info_alloc - column_info_alloc;
        size_t s = column_info_alloc + 1 + new_column_info_alloc;
        size_t t = s * column_info_growth;
        if (s < new_column_info_alloc || t / column_info_growth != s)
          xalloc_die ();
        p = xnmalloc (t / 2, sizeof *p);
      }

      /* Grow the triangle by parceling out the cells just allocated.  */
      for (i = column_info_alloc; i < new_column_info_alloc; i++)
        {
          column_info[i].col_arr = p;
          p += i + 1;
        }

      column_info_alloc = new_column_info_alloc;
    }

  for (i = 0; i < max_cols; ++i)
    {
      size_t j;

      column_info[i].valid_len = true;
      column_info[i].line_len = (i + 1) * MIN_COLUMN_WIDTH;
      for (j = 0; j <= i; ++j)
        column_info[i].col_arr[j] = MIN_COLUMN_WIDTH;
    }
}

/* Calculate the number of columns needed to represent the current set
   of files in the current display width.  */

static size_t
calculate_columns (bool by_columns)
{
  size_t filesno;		/* Index into cwd_file.  */
  size_t cols;			/* Number of files across.  */

  /* Normally the maximum number of columns is determined by the
     screen width.  But if few files are available this might limit it
     as well.  */
  size_t max_cols = 0 < max_idx && max_idx < cwd_n_used ? max_idx : cwd_n_used;

  init_column_info (max_cols);

  /* Compute the maximum number of possible columns.  */
  for (filesno = 0; filesno < cwd_n_used; ++filesno)
    {
      struct fileinfo const *f = sorted_file[filesno];
      size_t name_length = length_of_file_name_and_frills (f);

      for (size_t i = 0; i < max_cols; ++i)
        {
          if (column_info[i].valid_len)
            {
              size_t idx = (by_columns
                            ? filesno / ((cwd_n_used + i) / (i + 1))
                            : filesno % (i + 1));
              size_t real_length = name_length + (idx == i ? 0 : 2);

              if (column_info[i].col_arr[idx] < real_length)
                {
                  column_info[i].line_len += (real_length
                                              - column_info[i].col_arr[idx]);
                  column_info[i].col_arr[idx] = real_length;
                  column_info[i].valid_len = (column_info[i].line_len
                                              < line_length);
                }
            }
        }
    }

  /* Find maximum allowed columns.  */
  for (cols = max_cols; 1 < cols; --cols)
    {
      if (column_info[cols - 1].valid_len)
        break;
    }

  return cols;
}

void
usage (int status)
{
  if (status != EXIT_SUCCESS)
    emit_try_help ();
  else
    {
      printf (_("Usage: %s [OPTION]... [FILE]...\n"), program_name);
      fputs (_("\
List information about the FILEs (the current directory by default).\n\
Sort entries alphabetically if none of -cftuvSUX nor --sort is specified.\n\
"), stdout);

      emit_mandatory_arg_note ();

      fputs (_("\
  -a, --all                  do not ignore entries starting with .\n\
  -A, --almost-all           do not list implied . and ..\n\
      --author               with -l, print the author of each file\n\
  -b, --escape               print C-style escapes for nongraphic characters\n\
"), stdout);
      fputs (_("\
      --block-size=SIZE      with -l, scale sizes by SIZE when printing them;\n\
                             e.g., '--block-size=M'; see SIZE format below\n\
\n\
"), stdout);
      fputs (_("\
  -B, --ignore-backups       do not list implied entries ending with ~\n\
"), stdout);
      fputs (_("\
  -c                         with -lt: sort by, and show, ctime (time of last\n\
                             change of file status information);\n\
                             with -l: show ctime and sort by name;\n\
                             otherwise: sort by ctime, newest first\n\
\n\
"), stdout);
      fputs (_("\
  -C                         list entries by columns\n\
      --color[=WHEN]         color the output WHEN; more info below\n\
  -d, --directory            list directories themselves, not their contents\n\
  -D, --dired                generate output designed for Emacs' dired mode\n\
"), stdout);
      fputs (_("\
  -f                         list all entries in directory order\n\
  -F, --classify[=WHEN]      append indicator (one of */=>@|) to entries WHEN\n\
      --file-type            likewise, except do not append '*'\n\
"), stdout);
      fputs (_("\
      --format=WORD          across -x, commas -m, horizontal -x, long -l,\n\
                             single-column -1, verbose -l, vertical -C\n\
\n\
"), stdout);
      fputs (_("\
      --full-time            like -l --time-style=full-iso\n\
"), stdout);
      fputs (_("\
  -g                         like -l, but do not list owner\n\
"), stdout);
      fputs (_("\
      --group-directories-first\n\
                             group directories before files;\n\
                             can be augmented with a --sort option, but any\n\
                             use of --sort=none (-U) disables grouping\n\
\n\
"), stdout);
      fputs (_("\
  -G, --no-group             in a long listing, don't print group names\n\
"), stdout);
      fputs (_("\
  -h, --human-readable       with -l and -s, print sizes like 1K 234M 2G etc.\n\
      --si                   likewise, but use powers of 1000 not 1024\n\
"), stdout);
      fputs (_("\
  -H, --dereference-command-line\n\
                             follow symbolic links listed on the command line\n\
"), stdout);
      fputs (_("\
      --dereference-command-line-symlink-to-dir\n\
                             follow each command line symbolic link\n\
                             that points to a directory\n\
\n\
"), stdout);
      fputs (_("\
      --hide=PATTERN         do not list implied entries matching shell PATTERN\
\n\
                             (overridden by -a or -A)\n\
\n\
"), stdout);
      fputs (_("\
      --hyperlink[=WHEN]     hyperlink file names WHEN\n\
"), stdout);
      fputs (_("\
      --indicator-style=WORD\n\
                             append indicator with style WORD to entry names:\n\
                             none (default), slash (-p),\n\
                             file-type (--file-type), classify (-F)\n\
\n\
"), stdout);
      fputs (_("\
  -i, --inode                print the index number of each file\n\
  -I, --ignore=PATTERN       do not list implied entries matching shell PATTERN\
\n\
"), stdout);
      fputs (_("\
  -k, --kibibytes            default to 1024-byte blocks for file system usage;\
\n\
                             used only with -s and per directory totals\n\
\n\
"), stdout);
      fputs (_("\
  -l                         use a long listing format\n\
"), stdout);
      fputs (_("\
  -L, --dereference          when showing file information for a symbolic\n\
                             link, show information for the file the link\n\
                             references rather than for the link itself\n\
\n\
"), stdout);
      fputs (_("\
  -m                         fill width with a comma separated list of entries\
\n\
"), stdout);
      fputs (_("\
  -n, --numeric-uid-gid      like -l, but list numeric user and group IDs\n\
  -N, --literal              print entry names without quoting\n\
  -o                         like -l, but do not list group information\n\
  -p, --indicator-style=slash\n\
                             append / indicator to directories\n\
"), stdout);
      fputs (_("\
  -q, --hide-control-chars   print ? instead of nongraphic characters\n\
"), stdout);
      fputs (_("\
      --show-control-chars   show nongraphic characters as-is (the default,\n\
                             unless program is 'ls' and output is a terminal)\
\n\
\n\
"), stdout);
      fputs (_("\
  -Q, --quote-name           enclose entry names in double quotes\n\
"), stdout);
      fputs (_("\
      --quoting-style=WORD   use quoting style WORD for entry names:\n\
                             literal, locale, shell, shell-always,\n\
                             shell-escape, shell-escape-always, c, escape\n\
                             (overrides QUOTING_STYLE environment variable)\n\
\n\
"), stdout);
      fputs (_("\
  -r, --reverse              reverse order while sorting\n\
  -R, --recursive            list subdirectories recursively\n\
  -s, --size                 print the allocated size of each file, in blocks\n\
"), stdout);
      fputs (_("\
  -S                         sort by file size, largest first\n\
"), stdout);
      fputs (_("\
      --sort=WORD            sort by WORD instead of name: none (-U), size (-S)\
,\n\
                             time (-t), version (-v), extension (-X), width\n\
\n\
"), stdout);
      fputs (_("\
      --time=WORD            select which timestamp used to display or sort;\n\
                               access time (-u): atime, access, use;\n\
                               metadata change time (-c): ctime, status;\n\
                               modified time (default): mtime, modification;\n\
                               birth time: birth, creation;\n\
                             with -l, WORD determines which time to show;\n\
                             with --sort=time, sort by WORD (newest first)\n\
\n\
"), stdout);
      fputs (_("\
      --time-style=TIME_STYLE\n\
                             time/date format with -l; see TIME_STYLE below\n\
"), stdout);
      fputs (_("\
  -t                         sort by time, newest first; see --time\n\
  -T, --tabsize=COLS         assume tab stops at each COLS instead of 8\n\
"), stdout);
      fputs (_("\
  -u                         with -lt: sort by, and show, access time;\n\
                             with -l: show access time and sort by name;\n\
                             otherwise: sort by access time, newest first\n\
\n\
"), stdout);
      fputs (_("\
  -U                         do not sort; list entries in directory order\n\
"), stdout);
      fputs (_("\
  -v                         natural sort of (version) numbers within text\n\
"), stdout);
      fputs (_("\
  -w, --width=COLS           set output width to COLS.  0 means no limit\n\
  -x                         list entries by lines instead of by columns\n\
  -X                         sort alphabetically by entry extension\n\
  -Z, --context              print any security context of each file\n\
      --zero                 end each output line with NUL, not newline\n\
  -1                         list one file per line\n\
"), stdout);
      fputs (HELP_OPTION_DESCRIPTION, stdout);
      fputs (VERSION_OPTION_DESCRIPTION, stdout);
      emit_size_note ();
      fputs (_("\
\n\
The TIME_STYLE argument can be full-iso, long-iso, iso, locale, or +FORMAT.\n\
FORMAT is interpreted like in date(1).  If FORMAT is FORMAT1<newline>FORMAT2,\n\
then FORMAT1 applies to non-recent files and FORMAT2 to recent files.\n\
TIME_STYLE prefixed with 'posix-' takes effect only outside the POSIX locale.\n\
Also the TIME_STYLE environment variable sets the default style to use.\n\
"), stdout);
      fputs (_("\
\n\
The WHEN argument defaults to 'always' and can also be 'auto' or 'never'.\n\
"), stdout);
      fputs (_("\
\n\
Using color to distinguish file types is disabled both by default and\n\
with --color=never.  With --color=auto, ls emits color codes only when\n\
standard output is connected to a terminal.  The LS_COLORS environment\n\
variable can change the settings.  Use the dircolors(1) command to set it.\n\
"), stdout);
      fputs (_("\
\n\
Exit status:\n\
 0  if OK,\n\
 1  if minor problems (e.g., cannot access subdirectory),\n\
 2  if serious trouble (e.g., cannot access command-line argument).\n\
"), stdout);
      emit_ancillary_info (PROGRAM_NAME);
    }
  exit (status);
}
