 

#include <config.h>

#include <stdio.h>
#include <getopt.h>
#include <sys/types.h>
#include <signal.h>

#include "system.h"
#include "argmatch.h"
#include "assure.h"
#include "cl-strtod.h"
#include "fcntl--.h"
#include "iopoll.h"
#include "isapipe.h"
#include "posixver.h"
#include "quote.h"
#include "safe-read.h"
#include "stat-size.h"
#include "stat-time.h"
#include "xbinary-io.h"
#include "xdectoint.h"
#include "xnanosleep.h"
#include "xstrtol.h"
#include "xstrtod.h"

#if HAVE_INOTIFY
# include "hash.h"
# include <poll.h>
# include <sys/inotify.h>
#endif

 
#if defined __linux__ || defined __ANDROID__
# include "fs.h"
# include "fs-is-local.h"
# if HAVE_SYS_STATFS_H
#  include <sys/statfs.h>
# elif HAVE_SYS_VFS_H
#  include <sys/vfs.h>
# endif
#endif

 
#define PROGRAM_NAME "tail"

#define AUTHORS \
  proper_name ("Paul Rubin"), \
  proper_name ("David MacKenzie"), \
  proper_name ("Ian Lance Taylor"), \
  proper_name ("Jim Meyering")

 
#define DEFAULT_N_LINES 10

 
#define COPY_TO_EOF UINTMAX_MAX
#define COPY_A_BUFFER (UINTMAX_MAX - 1)

 
#define DEFAULT_FOLLOW_MODE Follow_descriptor

enum Follow_mode
{
   
  Follow_name = 1,

   
  Follow_descriptor = 2
};

 
#define IS_TAILABLE_FILE_TYPE(Mode) \
  (S_ISREG (Mode) || S_ISFIFO (Mode) || S_ISSOCK (Mode) || S_ISCHR (Mode))

static char const *const follow_mode_string[] =
{
  "descriptor", "name", nullptr
};

static enum Follow_mode const follow_mode_map[] =
{
  Follow_descriptor, Follow_name,
};

struct File_spec
{
   
  char *name;

   
  off_t size;
  struct timespec mtime;
  dev_t dev;
  ino_t ino;
  mode_t mode;

   
  bool ignore;

   
  bool remote;

   
  bool tailable;

   
  int fd;

   
  int errnum;

   
  int blocking;

#if HAVE_INOTIFY
   
  int wd;

   
  int parent_wd;

   
  size_t basename_start;
#endif

   
  uintmax_t n_unchanged_stats;
};

 
static bool reopen_inaccessible_files;

 
static bool count_lines;

 
static enum Follow_mode follow_mode = Follow_descriptor;

 
static bool forever;

 
static bool monitor_output;

 
static bool from_start;

 
static bool print_headers;

 
static char line_end;

 
enum header_mode
{
  multiple_files, always, never
};

 
#define DEFAULT_MAX_N_UNCHANGED_STATS_BETWEEN_OPENS 5
static uintmax_t max_n_unchanged_stats_between_opens =
  DEFAULT_MAX_N_UNCHANGED_STATS_BETWEEN_OPENS;

 
static pid_t pid;

 
static bool have_read_stdin;

 
static bool presume_input_pipe;

 
static bool disable_inotify;

 
enum
{
  RETRY_OPTION = CHAR_MAX + 1,
  MAX_UNCHANGED_STATS_OPTION,
  PID_OPTION,
  PRESUME_INPUT_PIPE_OPTION,
  LONG_FOLLOW_OPTION,
  DISABLE_INOTIFY_OPTION
};

static struct option const long_options[] =
{
  {"bytes", required_argument, nullptr, 'c'},
  {"follow", optional_argument, nullptr, LONG_FOLLOW_OPTION},
  {"lines", required_argument, nullptr, 'n'},
  {"max-unchanged-stats", required_argument, nullptr,
   MAX_UNCHANGED_STATS_OPTION},
  {"-disable-inotify", no_argument, nullptr,
   DISABLE_INOTIFY_OPTION},  
  {"pid", required_argument, nullptr, PID_OPTION},
  {"-presume-input-pipe", no_argument, nullptr,
   PRESUME_INPUT_PIPE_OPTION},  
  {"quiet", no_argument, nullptr, 'q'},
  {"retry", no_argument, nullptr, RETRY_OPTION},
  {"silent", no_argument, nullptr, 'q'},
  {"sleep-interval", required_argument, nullptr, 's'},
  {"verbose", no_argument, nullptr, 'v'},
  {"zero-terminated", no_argument, nullptr, 'z'},
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
      printf (_("\
Print the last %d lines of each FILE to standard output.\n\
With more than one FILE, precede each with a header giving the file name.\n\
"), DEFAULT_N_LINES);

      emit_stdin_note ();
      emit_mandatory_arg_note ();

     fputs (_("\
  -c, --bytes=[+]NUM       output the last NUM bytes; or use -c +NUM to\n\
                             output starting with byte NUM of each file\n\
"), stdout);
     fputs (_("\
  -f, --follow[={name|descriptor}]\n\
                           output appended data as the file grows;\n\
                             an absent option argument means 'descriptor'\n\
  -F                       same as --follow=name --retry\n\
"), stdout);
     printf (_("\
  -n, --lines=[+]NUM       output the last NUM lines, instead of the last %d;\n\
                             or use -n +NUM to skip NUM-1 lines at the start\n\
"),
             DEFAULT_N_LINES
             );
     printf (_("\
      --max-unchanged-stats=N\n\
                           with --follow=name, reopen a FILE which has not\n\
                             changed size after N (default %d) iterations\n\
                             to see if it has been unlinked or renamed\n\
                             (this is the usual case of rotated log files);\n\
                             with inotify, this option is rarely useful\n\
"),
             DEFAULT_MAX_N_UNCHANGED_STATS_BETWEEN_OPENS
             );
     fputs (_("\
      --pid=PID            with -f, terminate after process ID, PID dies\n\
  -q, --quiet, --silent    never output headers giving file names\n\
      --retry              keep trying to open a file if it is inaccessible\n\
"), stdout);
     fputs (_("\
  -s, --sleep-interval=N   with -f, sleep for approximately N seconds\n\
                             (default 1.0) between iterations;\n\
                             with inotify and --pid=P, check process P at\n\
                             least once every N seconds\n\
  -v, --verbose            always output headers giving file names\n\
"), stdout);
     fputs (_("\
  -z, --zero-terminated    line delimiter is NUL, not newline\n\
"), stdout);
     fputs (HELP_OPTION_DESCRIPTION, stdout);
     fputs (VERSION_OPTION_DESCRIPTION, stdout);
     fputs (_("\
\n\
NUM may have a multiplier suffix:\n\
b 512, kB 1000, K 1024, MB 1000*1000, M 1024*1024,\n\
GB 1000*1000*1000, G 1024*1024*1024, and so on for T, P, E, Z, Y, R, Q.\n\
Binary prefixes can be used, too: KiB=K, MiB=M, and so on.\n\
\n\
"), stdout);
     fputs (_("\
With --follow (-f), tail defaults to following the file descriptor, which\n\
means that even if a tail'ed file is renamed, tail will continue to track\n\
its end.  This default behavior is not desirable when you really want to\n\
track the actual name of the file, not the file descriptor (e.g., log\n\
rotation).  Use --follow=name in that case.  That causes tail to track the\n\
named file in a way that accommodates renaming, removal and creation.\n\
"), stdout);
      emit_ancillary_info (PROGRAM_NAME);
    }
  exit (status);
}

/* Ensure exit, either with SIGPIPE or EXIT_FAILURE status.  */
static void
die_pipe (void)
{
  raise (SIGPIPE);
  exit (EXIT_FAILURE);
}

/* If the output has gone away, then terminate
   as we would if we had written to this output.  */
static void
check_output_alive (void)
{
  if (! monitor_output)
    return;

  if (iopoll (-1, STDOUT_FILENO, false) == IOPOLL_BROKEN_OUTPUT)
    die_pipe ();
}

MAYBE_UNUSED static bool
valid_file_spec (struct File_spec const *f)
{
  /* Exactly one of the following subexpressions must be true. */
  return ((f->fd == -1) ^ (f->errnum == 0));
}

static char const *
pretty_name (struct File_spec const *f)
{
  return (STREQ (f->name, "-") ? _("standard input") : f->name);
}

/* Record a file F with descriptor FD, size SIZE, status ST, and
   blocking status BLOCKING.  */

static void
record_open_fd (struct File_spec *f, int fd,
                off_t size, struct stat const *st,
                int blocking)
{
  f->fd = fd;
  f->size = size;
  f->mtime = get_stat_mtime (st);
  f->dev = st->st_dev;
  f->ino = st->st_ino;
  f->mode = st->st_mode;
  f->blocking = blocking;
  f->n_unchanged_stats = 0;
  f->ignore = false;
}

/* Close the file with descriptor FD and name FILENAME.  */

static void
close_fd (int fd, char const *filename)
{
  if (fd != -1 && fd != STDIN_FILENO && close (fd))
    {
      error (0, errno, _("closing %s (fd=%d)"), quoteaf (filename), fd);
    }
}

static void
write_header (char const *pretty_filename)
{
  static bool first_file = true;

  printf ("%s==> %s <==\n", (first_file ? "" : "\n"), pretty_filename);
  first_file = false;
}

/* Write N_BYTES from BUFFER to stdout.
   Exit immediately on error with a single diagnostic.  */

static void
xwrite_stdout (char const *buffer, size_t n_bytes)
{
  if (n_bytes > 0 && fwrite (buffer, 1, n_bytes, stdout) < n_bytes)
    {
      clearerr (stdout); /* To avoid redundant close_stdout diagnostic.  */
      error (EXIT_FAILURE, errno, _("error writing %s"),
             quoteaf ("standard output"));
    }
}

/* Read and output N_BYTES of file PRETTY_FILENAME starting at the current
   position in FD.  If N_BYTES is COPY_TO_EOF, then copy until end of file.
   If N_BYTES is COPY_A_BUFFER, then copy at most one buffer's worth.
   Return the number of bytes read from the file.  */

static uintmax_t
dump_remainder (bool want_header, char const *pretty_filename, int fd,
                uintmax_t n_bytes)
{
  uintmax_t n_written;
  uintmax_t n_remaining = n_bytes;

  n_written = 0;
  while (true)
    {
      char buffer[BUFSIZ];
      size_t n = MIN (n_remaining, BUFSIZ);
      size_t bytes_read = safe_read (fd, buffer, n);
      if (bytes_read == SAFE_READ_ERROR)
        {
          if (errno != EAGAIN)
            error (EXIT_FAILURE, errno, _("error reading %s"),
                   quoteaf (pretty_filename));
          break;
        }
      if (bytes_read == 0)
        break;
      if (want_header)
        {
          write_header (pretty_filename);
          want_header = false;
        }
      xwrite_stdout (buffer, bytes_read);
      n_written += bytes_read;
      if (n_bytes != COPY_TO_EOF)
        {
          n_remaining -= bytes_read;
          if (n_remaining == 0 || n_bytes == COPY_A_BUFFER)
            break;
        }
    }

  return n_written;
}

/* Call lseek with the specified arguments, where file descriptor FD
   corresponds to the file, FILENAME.
   Give a diagnostic and exit nonzero if lseek fails.
   Otherwise, return the resulting offset.  */

static off_t
xlseek (int fd, off_t offset, int whence, char const *filename)
{
  off_t new_offset = lseek (fd, offset, whence);
  char buf[INT_BUFSIZE_BOUND (offset)];
  char *s;

  if (0 <= new_offset)
    return new_offset;

  s = offtostr (offset, buf);
  switch (whence)
    {
    case SEEK_SET:
      error (EXIT_FAILURE, errno, _("%s: cannot seek to offset %s"),
             quotef (filename), s);
      break;
    case SEEK_CUR:
      error (EXIT_FAILURE, errno, _("%s: cannot seek to relative offset %s"),
             quotef (filename), s);
      break;
    case SEEK_END:
      error (EXIT_FAILURE, errno,
             _("%s: cannot seek to end-relative offset %s"),
             quotef (filename), s);
      break;
    default:
      unreachable ();
    }
}

/* Print the last N_LINES lines from the end of file FD.
   Go backward through the file, reading 'BUFSIZ' bytes at a time (except
   probably the first), until we hit the start of the file or have
   read NUMBER newlines.
   START_POS is the starting position of the read pointer for the file
   associated with FD (may be nonzero).
   END_POS is the file offset of EOF (one larger than offset of last byte).
   Return true if successful.  */

static bool
file_lines (char const *pretty_filename, int fd, uintmax_t n_lines,
            off_t start_pos, off_t end_pos, uintmax_t *read_pos)
{
  char buffer[BUFSIZ];
  size_t bytes_read;
  off_t pos = end_pos;

  if (n_lines == 0)
    return true;

  /* Set 'bytes_read' to the size of the last, probably partial, buffer;
     0 < 'bytes_read' <= 'BUFSIZ'.  */
  bytes_read = (pos - start_pos) % BUFSIZ;
  if (bytes_read == 0)
    bytes_read = BUFSIZ;
  /* Make 'pos' a multiple of 'BUFSIZ' (0 if the file is short), so that all
     reads will be on block boundaries, which might increase efficiency.  */
  pos -= bytes_read;
  xlseek (fd, pos, SEEK_SET, pretty_filename);
  bytes_read = safe_read (fd, buffer, bytes_read);
  if (bytes_read == SAFE_READ_ERROR)
    {
      error (0, errno, _("error reading %s"), quoteaf (pretty_filename));
      return false;
    }
  *read_pos = pos + bytes_read;

  /* Count the incomplete line on files that don't end with a newline.  */
  if (bytes_read && buffer[bytes_read - 1] != line_end)
    --n_lines;

  do
    {
      /* Scan backward, counting the newlines in this bufferfull.  */

      size_t n = bytes_read;
      while (n)
        {
          char const *nl;
          nl = memrchr (buffer, line_end, n);
          if (nl == nullptr)
            break;
          n = nl - buffer;
          if (n_lines-- == 0)
            {
              /* If this newline isn't the last character in the buffer,
                 output the part that is after it.  */
              xwrite_stdout (nl + 1, bytes_read - (n + 1));
              *read_pos += dump_remainder (false, pretty_filename, fd,
                                           end_pos - (pos + bytes_read));
              return true;
            }
        }

      /* Not enough newlines in that bufferfull.  */
      if (pos == start_pos)
        {
          /* Not enough lines in the file; print everything from
             start_pos to the end.  */
          xlseek (fd, start_pos, SEEK_SET, pretty_filename);
          *read_pos = start_pos + dump_remainder (false, pretty_filename, fd,
                                                  end_pos);
          return true;
        }
      pos -= BUFSIZ;
      xlseek (fd, pos, SEEK_SET, pretty_filename);

      bytes_read = safe_read (fd, buffer, BUFSIZ);
      if (bytes_read == SAFE_READ_ERROR)
        {
          error (0, errno, _("error reading %s"), quoteaf (pretty_filename));
          return false;
        }

      *read_pos = pos + bytes_read;
    }
  while (bytes_read > 0);

  return true;
}

/* Print the last N_LINES lines from the end of the standard input,
   open for reading as pipe FD.
   Buffer the text as a linked list of LBUFFERs, adding them as needed.
   Return true if successful.  */

static bool
pipe_lines (char const *pretty_filename, int fd, uintmax_t n_lines,
            uintmax_t *read_pos)
{
  struct linebuffer
  {
    char buffer[BUFSIZ];
    size_t nbytes;
    size_t nlines;
    struct linebuffer *next;
  };
  typedef struct linebuffer LBUFFER;
  LBUFFER *first, *last, *tmp;
  size_t total_lines = 0;	/* Total number of newlines in all buffers.  */
  bool ok = true;
  size_t n_read;		/* Size in bytes of most recent read */

  first = last = xmalloc (sizeof (LBUFFER));
  first->nbytes = first->nlines = 0;
  first->next = nullptr;
  tmp = xmalloc (sizeof (LBUFFER));

  /* Input is always read into a fresh buffer.  */
  while (true)
    {
      n_read = safe_read (fd, tmp->buffer, BUFSIZ);
      if (n_read == 0 || n_read == SAFE_READ_ERROR)
        break;
      tmp->nbytes = n_read;
      *read_pos += n_read;
      tmp->nlines = 0;
      tmp->next = nullptr;

      /* Count the number of newlines just read.  */
      {
        char const *buffer_end = tmp->buffer + n_read;
        char const *p = tmp->buffer;
        while ((p = memchr (p, line_end, buffer_end - p)))
          {
            ++p;
            ++tmp->nlines;
          }
      }
      total_lines += tmp->nlines;

      /* If there is enough room in the last buffer read, just append the new
         one to it.  This is because when reading from a pipe, 'n_read' can
         often be very small.  */
      if (tmp->nbytes + last->nbytes < BUFSIZ)
        {
          memcpy (&last->buffer[last->nbytes], tmp->buffer, tmp->nbytes);
          last->nbytes += tmp->nbytes;
          last->nlines += tmp->nlines;
        }
      else
        {
          /* If there's not enough room, link the new buffer onto the end of
             the list, then either free up the oldest buffer for the next
             read if that would leave enough lines, or else malloc a new one.
             Some compaction mechanism is possible but probably not
             worthwhile.  */
          last = last->next = tmp;
          if (total_lines - first->nlines > n_lines)
            {
              tmp = first;
              total_lines -= first->nlines;
              first = first->next;
            }
          else
            tmp = xmalloc (sizeof (LBUFFER));
        }
    }

  free (tmp);

  if (n_read == SAFE_READ_ERROR)
    {
      error (0, errno, _("error reading %s"), quoteaf (pretty_filename));
      ok = false;
      goto free_lbuffers;
    }

  /* If the file is empty, then bail out.  */
  if (last->nbytes == 0)
    goto free_lbuffers;

  /* This prevents a core dump when the pipe contains no newlines.  */
  if (n_lines == 0)
    goto free_lbuffers;

  /* Count the incomplete line on files that don't end with a newline.  */
  if (last->buffer[last->nbytes - 1] != line_end)
    {
      ++last->nlines;
      ++total_lines;
    }

  /* Run through the list, printing lines.  First, skip over unneeded
     buffers.  */
  for (tmp = first; total_lines - tmp->nlines > n_lines; tmp = tmp->next)
    total_lines -= tmp->nlines;

  /* Find the correct beginning, then print the rest of the file.  */
  {
    char const *beg = tmp->buffer;
    char const *buffer_end = tmp->buffer + tmp->nbytes;
    if (total_lines > n_lines)
      {
        /* Skip 'total_lines' - 'n_lines' newlines.  We made sure that
           'total_lines' - 'n_lines' <= 'tmp->nlines'.  */
        size_t j;
        for (j = total_lines - n_lines; j; --j)
          {
            beg = rawmemchr (beg, line_end);
            ++beg;
          }
      }

    xwrite_stdout (beg, buffer_end - beg);
  }

  for (tmp = tmp->next; tmp; tmp = tmp->next)
    xwrite_stdout (tmp->buffer, tmp->nbytes);

free_lbuffers:
  while (first)
    {
      tmp = first->next;
      free (first);
      first = tmp;
    }
  return ok;
}

/* Print the last N_BYTES characters from the end of pipe FD.
   This is a stripped down version of pipe_lines.
   Return true if successful.  */

static bool
pipe_bytes (char const *pretty_filename, int fd, uintmax_t n_bytes,
            uintmax_t *read_pos)
{
  struct charbuffer
  {
    char buffer[BUFSIZ];
    size_t nbytes;
    struct charbuffer *next;
  };
  typedef struct charbuffer CBUFFER;
  CBUFFER *first, *last, *tmp;
  size_t i;			/* Index into buffers.  */
  size_t total_bytes = 0;	/* Total characters in all buffers.  */
  bool ok = true;
  size_t n_read;

  first = last = xmalloc (sizeof (CBUFFER));
  first->nbytes = 0;
  first->next = nullptr;
  tmp = xmalloc (sizeof (CBUFFER));

  /* Input is always read into a fresh buffer.  */
  while (true)
    {
      n_read = safe_read (fd, tmp->buffer, BUFSIZ);
      if (n_read == 0 || n_read == SAFE_READ_ERROR)
        break;
      *read_pos += n_read;
      tmp->nbytes = n_read;
      tmp->next = nullptr;

      total_bytes += tmp->nbytes;
      /* If there is enough room in the last buffer read, just append the new
         one to it.  This is because when reading from a pipe, 'nbytes' can
         often be very small.  */
      if (tmp->nbytes + last->nbytes < BUFSIZ)
        {
          memcpy (&last->buffer[last->nbytes], tmp->buffer, tmp->nbytes);
          last->nbytes += tmp->nbytes;
        }
      else
        {
          /* If there's not enough room, link the new buffer onto the end of
             the list, then either free up the oldest buffer for the next
             read if that would leave enough characters, or else malloc a new
             one.  Some compaction mechanism is possible but probably not
             worthwhile.  */
          last = last->next = tmp;
          if (total_bytes - first->nbytes > n_bytes)
            {
              tmp = first;
              total_bytes -= first->nbytes;
              first = first->next;
            }
          else
            {
              tmp = xmalloc (sizeof (CBUFFER));
            }
        }
    }

  free (tmp);

  if (n_read == SAFE_READ_ERROR)
    {
      error (0, errno, _("error reading %s"), quoteaf (pretty_filename));
      ok = false;
      goto free_cbuffers;
    }

  /* Run through the list, printing characters.  First, skip over unneeded
     buffers.  */
  for (tmp = first; total_bytes - tmp->nbytes > n_bytes; tmp = tmp->next)
    total_bytes -= tmp->nbytes;

  /* Find the correct beginning, then print the rest of the file.
     We made sure that 'total_bytes' - 'n_bytes' <= 'tmp->nbytes'.  */
  if (total_bytes > n_bytes)
    i = total_bytes - n_bytes;
  else
    i = 0;
  xwrite_stdout (&tmp->buffer[i], tmp->nbytes - i);

  for (tmp = tmp->next; tmp; tmp = tmp->next)
    xwrite_stdout (tmp->buffer, tmp->nbytes);

free_cbuffers:
  while (first)
    {
      tmp = first->next;
      free (first);
      first = tmp;
    }
  return ok;
}

/* Skip N_BYTES characters from the start of pipe FD, and print
   any extra characters that were read beyond that.
   Return 1 on error, 0 if ok, -1 if EOF.  */

static int
start_bytes (char const *pretty_filename, int fd, uintmax_t n_bytes,
             uintmax_t *read_pos)
{
  char buffer[BUFSIZ];

  while (0 < n_bytes)
    {
      size_t bytes_read = safe_read (fd, buffer, BUFSIZ);
      if (bytes_read == 0)
        return -1;
      if (bytes_read == SAFE_READ_ERROR)
        {
          error (0, errno, _("error reading %s"), quoteaf (pretty_filename));
          return 1;
        }
      *read_pos += bytes_read;
      if (bytes_read <= n_bytes)
        n_bytes -= bytes_read;
      else
        {
          size_t n_remaining = bytes_read - n_bytes;
          /* Print extra characters if there are any.  */
          xwrite_stdout (&buffer[n_bytes], n_remaining);
          break;
        }
    }

  return 0;
}

/* Skip N_LINES lines at the start of file or pipe FD, and print
   any extra characters that were read beyond that.
   Return 1 on error, 0 if ok, -1 if EOF.  */

static int
start_lines (char const *pretty_filename, int fd, uintmax_t n_lines,
             uintmax_t *read_pos)
{
  if (n_lines == 0)
    return 0;

  while (true)
    {
      char buffer[BUFSIZ];
      size_t bytes_read = safe_read (fd, buffer, BUFSIZ);
      if (bytes_read == 0) /* EOF */
        return -1;
      if (bytes_read == SAFE_READ_ERROR) /* error */
        {
          error (0, errno, _("error reading %s"), quoteaf (pretty_filename));
          return 1;
        }

      char *buffer_end = buffer + bytes_read;

      *read_pos += bytes_read;

      char *p = buffer;
      while ((p = memchr (p, line_end, buffer_end - p)))
        {
          ++p;
          if (--n_lines == 0)
            {
              if (p < buffer_end)
                xwrite_stdout (p, buffer_end - p);
              return 0;
            }
        }
    }
}

 
static bool
fremote (int fd, char const *name)
{
  bool remote = true;            

#if HAVE_FSTATFS && HAVE_STRUCT_STATFS_F_TYPE \
 && (defined __linux__ || defined __ANDROID__)
  struct statfs buf;
  int err = fstatfs (fd, &buf);
  if (err != 0)
    {
       
      if (errno != ENOSYS)
        error (0, errno, _("cannot determine location of %s. "
                           "reverting to polling"), quoteaf (name));
    }
  else
    {
       
      remote = is_local_fs_type (buf.f_type) <= 0;
    }
#endif

  return remote;
}

 
static void
recheck (struct File_spec *f, bool blocking)
{
  struct stat new_stats;
  bool ok = true;
  bool is_stdin = (STREQ (f->name, "-"));
  bool was_tailable = f->tailable;
  int prev_errnum = f->errnum;
  bool new_file;
  int fd = (is_stdin
            ? STDIN_FILENO
            : open (f->name, O_RDONLY | (blocking ? 0 : O_NONBLOCK)));

  affirm (valid_file_spec (f));

   
  f->tailable = !(reopen_inaccessible_files && fd == -1);

  if (! disable_inotify && ! lstat (f->name, &new_stats)
      && S_ISLNK (new_stats.st_mode))
    {
       
      ok = false;
      f->errnum = -1;
      f->ignore = true;

      error (0, 0, _("%s has been replaced with an untailable symbolic link"),
             quoteaf (pretty_name (f)));
    }
  else if (fd == -1 || fstat (fd, &new_stats) < 0)
    {
      ok = false;
      f->errnum = errno;
      if (!f->tailable)
        {
          if (was_tailable)
            {
               
              error (0, f->errnum, _("%s has become inaccessible"),
                     quoteaf (pretty_name (f)));
            }
          else
            {
               
            }
        }
      else if (prev_errnum != errno)
        error (0, errno, "%s", quotef (pretty_name (f)));
    }
  else if (!IS_TAILABLE_FILE_TYPE (new_stats.st_mode))
    {
      ok = false;
      f->errnum = -1;
      f->tailable = false;
      f->ignore = ! (reopen_inaccessible_files && follow_mode == Follow_name);
      if (was_tailable || prev_errnum != f->errnum)
        error (0, 0, _("%s has been replaced with an untailable file%s"),
               quoteaf (pretty_name (f)),
               f->ignore ? _("; giving up on this name") : "");
    }
  else if ((f->remote = fremote (fd, pretty_name (f))) && ! disable_inotify)
    {
      ok = false;
      f->errnum = -1;
      error (0, 0, _("%s has been replaced with an untailable remote file"),
             quoteaf (pretty_name (f)));
      f->ignore = true;
      f->remote = true;
    }
  else
    {
      f->errnum = 0;
    }

  new_file = false;
  if (!ok)
    {
      close_fd (fd, pretty_name (f));
      close_fd (f->fd, pretty_name (f));
      f->fd = -1;
    }
  else if (prev_errnum && prev_errnum != ENOENT)
    {
      new_file = true;
      affirm (f->fd == -1);
      error (0, 0, _("%s has become accessible"), quoteaf (pretty_name (f)));
    }
  else if (f->fd == -1)
    {
       
      new_file = true;

      error (0, 0,
             _("%s has appeared;  following new file"),
             quoteaf (pretty_name (f)));
    }
  else if (f->ino != new_stats.st_ino || f->dev != new_stats.st_dev)
    {
       
      new_file = true;

      error (0, 0,
             _("%s has been replaced;  following new file"),
             quoteaf (pretty_name (f)));

       
      close_fd (f->fd, pretty_name (f));

    }
  else
    {
       
      close_fd (fd, pretty_name (f));
    }

   
  if (new_file)
    {
       
      record_open_fd (f, fd, 0, &new_stats, (is_stdin ? -1 : blocking));
      if (S_ISREG (new_stats.st_mode))
        xlseek (fd, 0, SEEK_SET, pretty_name (f));
    }
}

 

static bool
any_live_files (const struct File_spec *f, size_t n_files)
{
   
  if (reopen_inaccessible_files && follow_mode == Follow_name)
    return true;

  for (size_t i = 0; i < n_files; i++)
    {
      if (0 <= f[i].fd)
        return true;
      else
        {
          if (! f[i].ignore && reopen_inaccessible_files)
            return true;
        }
    }

  return false;
}

 

static void
tail_forever (struct File_spec *f, size_t n_files, double sleep_interval)
{
   
  bool blocking = (pid == 0 && follow_mode == Follow_descriptor
                   && n_files == 1 && f[0].fd != -1 && ! S_ISREG (f[0].mode));
  size_t last;
  bool writer_is_dead = false;

  last = n_files - 1;

  while (true)
    {
      size_t i;
      bool any_input = false;

      for (i = 0; i < n_files; i++)
        {
          int fd;
          char const *name;
          mode_t mode;
          struct stat stats;
          uintmax_t bytes_read;

          if (f[i].ignore)
            continue;

          if (f[i].fd < 0)
            {
              recheck (&f[i], blocking);
              continue;
            }

          fd = f[i].fd;
          name = pretty_name (&f[i]);
          mode = f[i].mode;

          if (f[i].blocking != blocking)
            {
              int old_flags = fcntl (fd, F_GETFL);
              int new_flags = old_flags | (blocking ? 0 : O_NONBLOCK);
              if (old_flags < 0
                  || (new_flags != old_flags
                      && fcntl (fd, F_SETFL, new_flags) == -1))
                {
                   
                  if (S_ISREG (f[i].mode) && errno == EPERM)
                    {
                       
                    }
                  else
                    error (EXIT_FAILURE, errno,
                           _("%s: cannot change nonblocking mode"),
                           quotef (name));
                }
              else
                f[i].blocking = blocking;
            }

          bool read_unchanged = false;
          if (!f[i].blocking)
            {
              if (fstat (fd, &stats) != 0)
                {
                  f[i].fd = -1;
                  f[i].errnum = errno;
                  error (0, errno, "%s", quotef (name));
                  close (fd);  
                  continue;
                }

              if (f[i].mode == stats.st_mode
                  && (! S_ISREG (stats.st_mode) || f[i].size == stats.st_size)
                  && timespec_cmp (f[i].mtime, get_stat_mtime (&stats)) == 0)
                {
                  if ((max_n_unchanged_stats_between_opens
                       <= f[i].n_unchanged_stats++)
                      && follow_mode == Follow_name)
                    {
                      recheck (&f[i], f[i].blocking);
                      f[i].n_unchanged_stats = 0;
                    }
                  if (fd != f[i].fd || S_ISREG (stats.st_mode) || 1 < n_files)
                    continue;
                  else
                    read_unchanged = true;
                }

              affirm (fd == f[i].fd);

               

              f[i].mtime = get_stat_mtime (&stats);
              f[i].mode = stats.st_mode;

               
              if (! read_unchanged)
                f[i].n_unchanged_stats = 0;

               
              if (S_ISREG (mode) && stats.st_size < f[i].size)
                {
                  error (0, 0, _("%s: file truncated"), quotef (name));
                   
                  xlseek (fd, 0, SEEK_SET, name);
                  f[i].size = 0;
                }

              if (i != last)
                {
                  if (print_headers)
                    write_header (name);
                  last = i;
                }
            }

           
          uintmax_t bytes_to_read;
          if (f[i].blocking)
            bytes_to_read = COPY_A_BUFFER;
          else if (S_ISREG (mode) && f[i].remote)
            bytes_to_read = stats.st_size - f[i].size;
          else
            bytes_to_read = COPY_TO_EOF;

          bytes_read = dump_remainder (false, name, fd, bytes_to_read);

          if (read_unchanged && bytes_read)
            f[i].n_unchanged_stats = 0;

          any_input |= (bytes_read != 0);
          f[i].size += bytes_read;
        }

      if (! any_live_files (f, n_files))
        {
          error (0, 0, _("no files remaining"));
          break;
        }

      if ((!any_input || blocking) && fflush (stdout) != 0)
        write_error ();

      check_output_alive ();

       
      if (!any_input)
        {
          if (writer_is_dead)
            break;

           
          writer_is_dead = (pid != 0
                            && kill (pid, 0) != 0
                             
                            && errno != EPERM);

          if (!writer_is_dead && xnanosleep (sleep_interval))
            error (EXIT_FAILURE, errno, _("cannot read realtime clock"));

        }
    }
}

#if HAVE_INOTIFY

 

static bool
any_remote_file (const struct File_spec *f, size_t n_files)
{
  for (size_t i = 0; i < n_files; i++)
    if (0 <= f[i].fd && f[i].remote)
      return true;
  return false;
}

 

static bool
any_non_remote_file (const struct File_spec *f, size_t n_files)
{
  for (size_t i = 0; i < n_files; i++)
    if (0 <= f[i].fd && ! f[i].remote)
      return true;
  return false;
}

 

static bool
any_symlinks (const struct File_spec *f, size_t n_files)
{
  struct stat st;
  for (size_t i = 0; i < n_files; i++)
    if (lstat (f[i].name, &st) == 0 && S_ISLNK (st.st_mode))
      return true;
  return false;
}

 

static bool
any_non_regular_fifo (const struct File_spec *f, size_t n_files)
{
  for (size_t i = 0; i < n_files; i++)
    if (0 <= f[i].fd && ! S_ISREG (f[i].mode) && ! S_ISFIFO (f[i].mode))
      return true;
  return false;
}

 

static bool
tailable_stdin (const struct File_spec *f, size_t n_files)
{
  for (size_t i = 0; i < n_files; i++)
    if (!f[i].ignore && STREQ (f[i].name, "-"))
      return true;
  return false;
}

static size_t
wd_hasher (const void *entry, size_t tabsize)
{
  const struct File_spec *spec = entry;
  return spec->wd % tabsize;
}

static bool
wd_comparator (const void *e1, const void *e2)
{
  const struct File_spec *spec1 = e1;
  const struct File_spec *spec2 = e2;
  return spec1->wd == spec2->wd;
}

 
static void
check_fspec (struct File_spec *fspec, struct File_spec **prev_fspec)
{
  struct stat stats;
  char const *name;

  if (fspec->fd == -1)
    return;

  name = pretty_name (fspec);

  if (fstat (fspec->fd, &stats) != 0)
    {
      fspec->errnum = errno;
      close_fd (fspec->fd, name);
      fspec->fd = -1;
      return;
    }

   
  if (S_ISREG (fspec->mode) && stats.st_size < fspec->size)
    {
      error (0, 0, _("%s: file truncated"), quotef (name));
      xlseek (fspec->fd, 0, SEEK_SET, name);
      fspec->size = 0;
    }
  else if (S_ISREG (fspec->mode) && stats.st_size == fspec->size
           && timespec_cmp (fspec->mtime, get_stat_mtime (&stats)) == 0)
    return;

  bool want_header = print_headers && (fspec != *prev_fspec);

  uintmax_t bytes_read = dump_remainder (want_header, name, fspec->fd,
                                         COPY_TO_EOF);
  fspec->size += bytes_read;

  if (bytes_read)
    {
      *prev_fspec = fspec;
      if (fflush (stdout) != 0)
        write_error ();
    }
}

 
static void
tail_forever_inotify (int wd, struct File_spec *f, size_t n_files,
                      double sleep_interval, Hash_table **wd_to_namep)
{
# if TAIL_TEST_SLEEP
   
  xnanosleep (1000000);
# endif
  unsigned int max_realloc = 3;

   
  Hash_table *wd_to_name;

  bool found_watchable_file = false;
  bool tailed_but_unwatchable = false;
  bool found_unwatchable_dir = false;
  bool no_inotify_resources = false;
  bool writer_is_dead = false;
  struct File_spec *prev_fspec;
  size_t evlen = 0;
  char *evbuf;
  size_t evbuf_off = 0;
  size_t len = 0;

  wd_to_name = hash_initialize (n_files, nullptr, wd_hasher, wd_comparator,
                                nullptr);
  if (! wd_to_name)
    xalloc_die ();
  *wd_to_namep = wd_to_name;

   
  uint32_t inotify_wd_mask = IN_MODIFY;
   
  if (follow_mode == Follow_name)
    inotify_wd_mask |= (IN_ATTRIB | IN_DELETE_SELF | IN_MOVE_SELF);

   
  size_t i;
  for (i = 0; i < n_files; i++)
    {
      if (!f[i].ignore)
        {
          size_t fnlen = strlen (f[i].name);
          if (evlen < fnlen)
            evlen = fnlen;

          f[i].wd = -1;

          if (follow_mode == Follow_name)
            {
              size_t dirlen = dir_len (f[i].name);
              char prev = f[i].name[dirlen];
              f[i].basename_start = last_component (f[i].name) - f[i].name;

              f[i].name[dirlen] = '\0';

                
              f[i].parent_wd = inotify_add_watch (wd, dirlen ? f[i].name : ".",
                                                  (IN_CREATE | IN_DELETE
                                                   | IN_MOVED_TO | IN_ATTRIB
                                                   | IN_DELETE_SELF));

              f[i].name[dirlen] = prev;

              if (f[i].parent_wd < 0)
                {
                  if (errno != ENOSPC)  
                    error (0, errno, _("cannot watch parent directory of %s"),
                           quoteaf (f[i].name));
                  else
                    error (0, 0, _("inotify resources exhausted"));
                  found_unwatchable_dir = true;
                   
                  break;
                }
            }

          f[i].wd = inotify_add_watch (wd, f[i].name, inotify_wd_mask);

          if (f[i].wd < 0)
            {
              if (f[i].fd != -1)   
                tailed_but_unwatchable = true;
              if (errno == ENOSPC || errno == ENOMEM)
                {
                  no_inotify_resources = true;
                  error (0, 0, _("inotify resources exhausted"));
                  break;
                }
              else if (errno != f[i].errnum)
                error (0, errno, _("cannot watch %s"), quoteaf (f[i].name));
              continue;
            }

          if (hash_insert (wd_to_name, &(f[i])) == nullptr)
            xalloc_die ();

          found_watchable_file = true;
        }
    }

   
  if (no_inotify_resources || found_unwatchable_dir
      || (follow_mode == Follow_descriptor && tailed_but_unwatchable))
    return;
  if (follow_mode == Follow_descriptor && !found_watchable_file)
    exit (EXIT_FAILURE);

  prev_fspec = &(f[n_files - 1]);

   
  for (i = 0; i < n_files; i++)
    {
      if (! f[i].ignore)
        {
           
          if (follow_mode == Follow_name)
            recheck (&(f[i]), false);
          else if (f[i].fd != -1)
            {
               
              struct stat stats;

              if (stat (f[i].name, &stats) == 0
                  && (f[i].dev != stats.st_dev || f[i].ino != stats.st_ino))
                {
                  error (0, errno, _("%s was replaced"),
                         quoteaf (pretty_name (&(f[i]))));
                  return;
                }
            }

           
          check_fspec (&f[i], &prev_fspec);
        }
    }

  evlen += sizeof (struct inotify_event) + 1;
  evbuf = xmalloc (evlen);

   
  while (true)
    {
      struct File_spec *fspec;
      struct inotify_event *ev;
      void *void_ev;

       
      if (follow_mode == Follow_name
          && ! reopen_inaccessible_files
          && hash_get_n_entries (wd_to_name) == 0)
        error (EXIT_FAILURE, 0, _("no files remaining"));

      if (len <= evbuf_off)
        {
           

          int file_change;
          struct pollfd pfd[2];
          do
            {
               
              int delay = -1;

              if (pid)
                {
                  if (writer_is_dead)
                    exit (EXIT_SUCCESS);

                  writer_is_dead = (kill (pid, 0) != 0 && errno != EPERM);

                  if (writer_is_dead || sleep_interval <= 0)
                    delay = 0;
                  else if (sleep_interval < INT_MAX / 1000 - 1)
                    {
                       
                      double ddelay = sleep_interval * 1000;
                      delay = ddelay;
                      delay += delay < ddelay;
                    }
                }

              pfd[0].fd = wd;
              pfd[0].events = POLLIN;
              pfd[1].fd = STDOUT_FILENO;
              pfd[1].events = pfd[1].revents = 0;
              file_change = poll (pfd, monitor_output + 1, delay);
            }
          while (file_change == 0);

          if (file_change < 0)
            error (EXIT_FAILURE, errno,
                   _("error waiting for inotify and output events"));
          if (pfd[1].revents)
            die_pipe ();

          len = safe_read (wd, evbuf, evlen);
          evbuf_off = 0;

           
          if ((len == 0 || (len == SAFE_READ_ERROR && errno == EINVAL))
              && max_realloc--)
            {
              len = 0;
              evlen *= 2;
              evbuf = xrealloc (evbuf, evlen);
              continue;
            }

          if (len == 0 || len == SAFE_READ_ERROR)
            error (EXIT_FAILURE, errno, _("error reading inotify event"));
        }

      void_ev = evbuf + evbuf_off;
      ev = void_ev;
      evbuf_off += sizeof (*ev) + ev->len;

       
      if ((ev->mask & IN_DELETE_SELF) && ! ev->len)
        {
          for (i = 0; i < n_files; i++)
            {
              if (ev->wd == f[i].parent_wd)
                {
                  error (0, 0,
                      _("directory containing watched file was removed"));
                  return;
                }
            }
        }

      if (ev->len)  
        {
          size_t j;
          for (j = 0; j < n_files; j++)
            {
               
              if (f[j].parent_wd == ev->wd
                  && STREQ (ev->name, f[j].name + f[j].basename_start))
                break;
            }

           
          if (j == n_files)
            continue;

          fspec = &(f[j]);

          int new_wd = -1;
          bool deleting = !! (ev->mask & IN_DELETE);

          if (! deleting)
            {
               
              new_wd = inotify_add_watch (wd, f[j].name, inotify_wd_mask);
            }

          if (! deleting && new_wd < 0)
            {
              if (errno == ENOSPC || errno == ENOMEM)
                {
                  error (0, 0, _("inotify resources exhausted"));
                  return;  
                }
              else
                {
                   
                  error (0, errno, _("cannot watch %s"), quoteaf (f[j].name));
                }
               
            }

           
          bool new_watch;
          new_watch = (! deleting) && (fspec->wd < 0 || new_wd != fspec->wd);

          if (new_watch)
            {
              if (0 <= fspec->wd)
                {
                  inotify_rm_watch (wd, fspec->wd);
                  hash_remove (wd_to_name, fspec);
                }

              fspec->wd = new_wd;

              if (new_wd == -1)
                continue;

               
              struct File_spec *prev = hash_remove (wd_to_name, fspec);
              if (prev && prev != fspec)
                {
                  if (follow_mode == Follow_name)
                    recheck (prev, false);
                  prev->wd = -1;
                  close_fd (prev->fd, pretty_name (prev));
                }

              if (hash_insert (wd_to_name, fspec) == nullptr)
                xalloc_die ();
            }

          if (follow_mode == Follow_name)
            recheck (fspec, false);
        }
      else
        {
          struct File_spec key;
          key.wd = ev->wd;
          fspec = hash_lookup (wd_to_name, &key);
        }

      if (! fspec)
        continue;

      if (ev->mask & (IN_ATTRIB | IN_DELETE | IN_DELETE_SELF | IN_MOVE_SELF))
        {
           
          if (ev->mask & IN_DELETE_SELF)
            {
              inotify_rm_watch (wd, fspec->wd);
              hash_remove (wd_to_name, fspec);
            }

           

          recheck (fspec, false);

          continue;
        }
      check_fspec (fspec, &prev_fspec);
    }
}
#endif

 

static bool
tail_bytes (char const *pretty_filename, int fd, uintmax_t n_bytes,
            uintmax_t *read_pos)
{
  struct stat stats;

  if (fstat (fd, &stats))
    {
      error (0, errno, _("cannot fstat %s"), quoteaf (pretty_filename));
      return false;
    }

  if (from_start)
    {
      if (! presume_input_pipe && n_bytes <= OFF_T_MAX
          && ((S_ISREG (stats.st_mode)
               && xlseek (fd, n_bytes, SEEK_CUR, pretty_filename) >= 0)
              || lseek (fd, n_bytes, SEEK_CUR) != -1))
        *read_pos += n_bytes;
      else
        {
          int t = start_bytes (pretty_filename, fd, n_bytes, read_pos);
          if (t)
            return t < 0;
        }
      n_bytes = COPY_TO_EOF;
    }
  else
    {
      off_t end_pos = -1;
      off_t current_pos = -1;

      if (! presume_input_pipe && n_bytes <= OFF_T_MAX)
        {
          if (usable_st_size (&stats))
            end_pos = stats.st_size;
          else if ((current_pos = lseek (fd, -n_bytes, SEEK_END)) != -1)
            end_pos = current_pos + n_bytes;
        }
      if (end_pos <= (off_t) ST_BLKSIZE (stats))
        return pipe_bytes (pretty_filename, fd, n_bytes, read_pos);
      if (current_pos == -1)
        current_pos = xlseek (fd, 0, SEEK_CUR, pretty_filename);
      if (current_pos < end_pos)
        {
          off_t bytes_remaining = end_pos - current_pos;

          if (n_bytes < bytes_remaining)
            {
              current_pos = end_pos - n_bytes;
              xlseek (fd, current_pos, SEEK_SET, pretty_filename);
            }
        }
      *read_pos = current_pos;
    }

  *read_pos += dump_remainder (false, pretty_filename, fd, n_bytes);
  return true;
}

 

static bool
tail_lines (char const *pretty_filename, int fd, uintmax_t n_lines,
            uintmax_t *read_pos)
{
  struct stat stats;

  if (fstat (fd, &stats))
    {
      error (0, errno, _("cannot fstat %s"), quoteaf (pretty_filename));
      return false;
    }

  if (from_start)
    {
      int t = start_lines (pretty_filename, fd, n_lines, read_pos);
      if (t)
        return t < 0;
      *read_pos += dump_remainder (false, pretty_filename, fd, COPY_TO_EOF);
    }
  else
    {
      off_t start_pos = -1;
      off_t end_pos;

       
      if ( ! presume_input_pipe
           && S_ISREG (stats.st_mode)
           && (start_pos = lseek (fd, 0, SEEK_CUR)) != -1
           && start_pos < (end_pos = lseek (fd, 0, SEEK_END)))
        {
          *read_pos = end_pos;
          if (end_pos != 0
              && ! file_lines (pretty_filename, fd, n_lines,
                               start_pos, end_pos, read_pos))
            return false;
        }
      else
        {
           
          if (start_pos != -1)
            xlseek (fd, start_pos, SEEK_SET, pretty_filename);

          return pipe_lines (pretty_filename, fd, n_lines, read_pos);
        }
    }
  return true;
}

 

static bool
tail (char const *filename, int fd, uintmax_t n_units,
      uintmax_t *read_pos)
{
  *read_pos = 0;
  if (count_lines)
    return tail_lines (filename, fd, n_units, read_pos);
  else
    return tail_bytes (filename, fd, n_units, read_pos);
}

 

static bool
tail_file (struct File_spec *f, uintmax_t n_units)
{
  int fd;
  bool ok;

  bool is_stdin = (STREQ (f->name, "-"));

  if (is_stdin)
    {
      have_read_stdin = true;
      fd = STDIN_FILENO;
      xset_binary_mode (STDIN_FILENO, O_BINARY);
    }
  else
    fd = open (f->name, O_RDONLY | O_BINARY);

  f->tailable = !(reopen_inaccessible_files && fd == -1);

  if (fd == -1)
    {
      if (forever)
        {
          f->fd = -1;
          f->errnum = errno;
          f->ignore = ! reopen_inaccessible_files;
          f->ino = 0;
          f->dev = 0;
        }
      error (0, errno, _("cannot open %s for reading"),
             quoteaf (pretty_name (f)));
      ok = false;
    }
  else
    {
      uintmax_t read_pos;

      if (print_headers)
        write_header (pretty_name (f));
      ok = tail (pretty_name (f), fd, n_units, &read_pos);
      if (forever)
        {
          struct stat stats;

#if TAIL_TEST_SLEEP
           
          xnanosleep (1);
#endif
          f->errnum = ok - 1;
          if (fstat (fd, &stats) < 0)
            {
              ok = false;
              f->errnum = errno;
              error (0, errno, _("error reading %s"),
                     quoteaf (pretty_name (f)));
            }
          else if (!IS_TAILABLE_FILE_TYPE (stats.st_mode))
            {
              ok = false;
              f->errnum = -1;
              f->tailable = false;
              f->ignore = ! reopen_inaccessible_files;
              error (0, 0, _("%s: cannot follow end of this type of file%s"),
                     quotef (pretty_name (f)),
                     f->ignore ? _("; giving up on this name") : "");
            }

          if (!ok)
            {
              f->ignore = ! reopen_inaccessible_files;
              close_fd (fd, pretty_name (f));
              f->fd = -1;
            }
          else
            {
               
              record_open_fd (f, fd, read_pos, &stats, (is_stdin ? -1 : 1));
              f->remote = fremote (fd, pretty_name (f));
            }
        }
      else
        {
          if (!is_stdin && close (fd))
            {
              error (0, errno, _("error reading %s"),
                     quoteaf (pretty_name (f)));
              ok = false;
            }
        }
    }

  return ok;
}

 

static bool
parse_obsolete_option (int argc, char * const *argv, uintmax_t *n_units)
{
  char const *p;
  char const *n_string;
  char const *n_string_end;
  int default_count = DEFAULT_N_LINES;
  bool t_from_start;
  bool t_count_lines = true;
  bool t_forever = false;

   
  if (! (argc == 2
         || (argc == 3 && ! (argv[2][0] == '-' && argv[2][1]))
         || (3 <= argc && argc <= 4 && STREQ (argv[2], "--"))))
    return false;

  int posix_ver = posix2_version ();
  bool obsolete_usage = posix_ver < 200112;
  bool traditional_usage = obsolete_usage || 200809 <= posix_ver;
  p = argv[1];

  switch (*p++)
    {
    default:
      return false;

    case '+':
       
      if (!traditional_usage)
        return false;

      t_from_start = true;
      break;

    case '-':
       
      if (!obsolete_usage && !p[p[0] == 'c'])
        return false;

      t_from_start = false;
      break;
    }

  n_string = p;
  while (ISDIGIT (*p))
    p++;
  n_string_end = p;

  switch (*p)
    {
    case 'b': default_count *= 512; FALLTHROUGH;
    case 'c': t_count_lines = false; FALLTHROUGH;
    case 'l': p++; break;
    }

  if (*p == 'f')
    {
      t_forever = true;
      ++p;
    }

  if (*p)
    return false;

  if (n_string == n_string_end)
    *n_units = default_count;
  else if ((xstrtoumax (n_string, nullptr, 10, n_units, "b")
            & ~LONGINT_INVALID_SUFFIX_CHAR)
           != LONGINT_OK)
    error (EXIT_FAILURE, errno, "%s: %s", _("invalid number"),
           quote (argv[1]));

   
  from_start = t_from_start;
  count_lines = t_count_lines;
  forever = t_forever;

  return true;
}

static void
parse_options (int argc, char **argv,
               uintmax_t *n_units, enum header_mode *header_mode,
               double *sleep_interval)
{
  int c;

  while ((c = getopt_long (argc, argv, "c:n:fFqs:vz0123456789",
                           long_options, nullptr))
         != -1)
    {
      switch (c)
        {
        case 'F':
          forever = true;
          follow_mode = Follow_name;
          reopen_inaccessible_files = true;
          break;

        case 'c':
        case 'n':
          count_lines = (c == 'n');
          if (*optarg == '+')
            from_start = true;
          else if (*optarg == '-')
            ++optarg;

          *n_units = xdectoumax (optarg, 0, UINTMAX_MAX, "bkKmMGTPEZYRQ0",
                                 count_lines
                                 ? _("invalid number of lines")
                                 : _("invalid number of bytes"), 0);
          break;

        case 'f':
        case LONG_FOLLOW_OPTION:
          forever = true;
          if (optarg == nullptr)
            follow_mode = DEFAULT_FOLLOW_MODE;
          else
            follow_mode = XARGMATCH ("--follow", optarg,
                                     follow_mode_string, follow_mode_map);
          break;

        case RETRY_OPTION:
          reopen_inaccessible_files = true;
          break;

        case MAX_UNCHANGED_STATS_OPTION:
           
          max_n_unchanged_stats_between_opens =
            xdectoumax (optarg, 0, UINTMAX_MAX, "",
              _("invalid maximum number of unchanged stats between opens"), 0);
          break;

        case DISABLE_INOTIFY_OPTION:
          disable_inotify = true;
          break;

        case PID_OPTION:
          pid = xdectoumax (optarg, 0, PID_T_MAX, "", _("invalid PID"), 0);
          break;

        case PRESUME_INPUT_PIPE_OPTION:
          presume_input_pipe = true;
          break;

        case 'q':
          *header_mode = never;
          break;

        case 's':
          {
            double s;
            if (! (xstrtod (optarg, nullptr, &s, cl_strtod) && 0 <= s))
              error (EXIT_FAILURE, 0,
                     _("invalid number of seconds: %s"), quote (optarg));
            *sleep_interval = s;
          }
          break;

        case 'v':
          *header_mode = always;
          break;

        case 'z':
          line_end = '\0';
          break;

        case_GETOPT_HELP_CHAR;

        case_GETOPT_VERSION_CHAR (PROGRAM_NAME, AUTHORS);

        case '0': case '1': case '2': case '3': case '4':
        case '5': case '6': case '7': case '8': case '9':
          error (EXIT_FAILURE, 0, _("option used in invalid context -- %c"), c);

        default:
          usage (EXIT_FAILURE);
        }
    }

  if (reopen_inaccessible_files)
    {
      if (!forever)
        {
          reopen_inaccessible_files = false;
          error (0, 0, _("warning: --retry ignored; --retry is useful"
                         " only when following"));
        }
      else if (follow_mode == Follow_descriptor)
        error (0, 0, _("warning: --retry only effective for the initial open"));
    }

  if (pid && !forever)
    error (0, 0,
           _("warning: PID ignored; --pid=PID is useful only when following"));
  else if (pid && kill (pid, 0) != 0 && errno == ENOSYS)
    {
      error (0, 0, _("warning: --pid=PID is not supported on this system"));
      pid = 0;
    }
}

 
static size_t
ignore_fifo_and_pipe (struct File_spec *f, size_t n_files)
{
   
  size_t n_viable = 0;

  for (size_t i = 0; i < n_files; i++)
    {
      bool is_a_fifo_or_pipe =
        (STREQ (f[i].name, "-")
         && !f[i].ignore
         && 0 <= f[i].fd
         && (S_ISFIFO (f[i].mode)
             || (HAVE_FIFO_PIPES != 1 && isapipe (f[i].fd))));
      if (is_a_fifo_or_pipe)
        {
          f[i].fd = -1;
          f[i].ignore = true;
        }
      else
        ++n_viable;
    }

  return n_viable;
}

int
main (int argc, char **argv)
{
  enum header_mode header_mode = multiple_files;
  bool ok = true;
   
  uintmax_t n_units = DEFAULT_N_LINES;
  size_t n_files;
  char **file;
  struct File_spec *F;
  size_t i;
  bool obsolete_option;

   
  double sleep_interval = 1.0;

  initialize_main (&argc, &argv);
  set_program_name (argv[0]);
  setlocale (LC_ALL, "");
  bindtextdomain (PACKAGE, LOCALEDIR);
  textdomain (PACKAGE);

  atexit (close_stdout);

  have_read_stdin = false;

  count_lines = true;
  forever = from_start = print_headers = false;
  line_end = '\n';
  obsolete_option = parse_obsolete_option (argc, argv, &n_units);
  argc -= obsolete_option;
  argv += obsolete_option;
  parse_options (argc, argv, &n_units, &header_mode, &sleep_interval);

   
  if (from_start)
    {
      if (n_units)
        --n_units;
    }

  if (optind < argc)
    {
      n_files = argc - optind;
      file = argv + optind;
    }
  else
    {
      static char *dummy_stdin = (char *) "-";
      n_files = 1;
      file = &dummy_stdin;
    }

  {
    bool found_hyphen = false;

    for (i = 0; i < n_files; i++)
      if (STREQ (file[i], "-"))
        found_hyphen = true;

     
    if (found_hyphen && follow_mode == Follow_name)
      error (EXIT_FAILURE, 0, _("cannot follow %s by name"), quoteaf ("-"));

     
    if (forever && found_hyphen)
      {
        struct stat in_stat;
        bool blocking_stdin;
        blocking_stdin = (pid == 0 && follow_mode == Follow_descriptor
                          && n_files == 1 && ! fstat (STDIN_FILENO, &in_stat)
                          && ! S_ISREG (in_stat.st_mode));

        if (! blocking_stdin && isatty (STDIN_FILENO))
          error (0, 0, _("warning: following standard input"
                         " indefinitely is ineffective"));
      }
  }

   
  if (! n_units && ! forever && ! from_start)
    return EXIT_SUCCESS;

  F = xnmalloc (n_files, sizeof *F);
  for (i = 0; i < n_files; i++)
    F[i].name = file[i];

  if (header_mode == always
      || (header_mode == multiple_files && n_files > 1))
    print_headers = true;

  xset_binary_mode (STDOUT_FILENO, O_BINARY);

  for (i = 0; i < n_files; i++)
    ok &= tail_file (&F[i], n_units);

  if (forever && ignore_fifo_and_pipe (F, n_files))
    {
       
      struct stat out_stat;
      if (fstat (STDOUT_FILENO, &out_stat) < 0)
        error (EXIT_FAILURE, errno, _("standard output"));
      monitor_output = (S_ISFIFO (out_stat.st_mode)
                        || (HAVE_FIFO_PIPES != 1 && isapipe (STDOUT_FILENO)));

#if HAVE_INOTIFY
       
      if (!disable_inotify && (tailable_stdin (F, n_files)
                               || any_remote_file (F, n_files)
                               || ! any_non_remote_file (F, n_files)
                               || any_symlinks (F, n_files)
                               || any_non_regular_fifo (F, n_files)
                               || (!ok && follow_mode == Follow_descriptor)))
        disable_inotify = true;

      if (!disable_inotify)
        {
          int wd = inotify_init ();
          if (0 <= wd)
            {
               
              if (fflush (stdout) != 0)
                write_error ();

              Hash_table *ht;
              tail_forever_inotify (wd, F, n_files, sleep_interval, &ht);
              hash_free (ht);
              close (wd);
              errno = 0;
            }
          error (0, errno, _("inotify cannot be used, reverting to polling"));
        }
#endif
      disable_inotify = true;
      tail_forever (F, n_files, sleep_interval);
    }

  if (have_read_stdin && close (STDIN_FILENO) < 0)
    error (EXIT_FAILURE, errno, "-");
  main_exit (ok ? EXIT_SUCCESS : EXIT_FAILURE);
}
