 
#include <config.h>

#include <stdckdint.h>
#include <stdio.h>
#include <getopt.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/wait.h>

#include "system.h"
#include "alignalloc.h"
#include "assure.h"
#include "fadvise.h"
#include "fd-reopen.h"
#include "fcntl--.h"
#include "full-write.h"
#include "ioblksize.h"
#include "quote.h"
#include "sig2str.h"
#include "sys-limits.h"
#include "temp-stream.h"
#include "xbinary-io.h"
#include "xdectoint.h"
#include "xstrtol.h"

 
#define PROGRAM_NAME "split"

#define AUTHORS \
  proper_name_lite ("Torbjorn Granlund", "Torbj\303\266rn Granlund"), \
  proper_name ("Richard M. Stallman")

 
static char const *filter_command;

 
static pid_t filter_pid;

 
static int *open_pipes;
static idx_t open_pipes_alloc;
static int n_open_pipes;

 
static bool default_SIGPIPE;

 
static char const *outbase;

 
static char *outfile;

 
static char *outfile_mid;

 
static bool suffix_auto = true;

 
static idx_t suffix_length;

 
static char const *suffix_alphabet = "abcdefghijklmnopqrstuvwxyz";

 
static char const *numeric_suffix_start;

 
static char const *additional_suffix;

 
static char *infile;

 
static struct stat in_stat_buf;

 
static int output_desc = -1;

 
static bool verbose;

 
static bool elide_empty_files;

 
static bool unbuffered;

 
static int eolchar = -1;

 
enum Split_type
{
  type_undef, type_bytes, type_byteslines, type_lines, type_digits,
  type_chunk_bytes, type_chunk_lines, type_rr
};

 
enum
{
  VERBOSE_OPTION = CHAR_MAX + 1,
  FILTER_OPTION,
  IO_BLKSIZE_OPTION,
  ADDITIONAL_SUFFIX_OPTION
};

static struct option const longopts[] =
{
  {"bytes", required_argument, nullptr, 'b'},
  {"lines", required_argument, nullptr, 'l'},
  {"line-bytes", required_argument, nullptr, 'C'},
  {"number", required_argument, nullptr, 'n'},
  {"elide-empty-files", no_argument, nullptr, 'e'},
  {"unbuffered", no_argument, nullptr, 'u'},
  {"suffix-length", required_argument, nullptr, 'a'},
  {"additional-suffix", required_argument, nullptr,
   ADDITIONAL_SUFFIX_OPTION},
  {"numeric-suffixes", optional_argument, nullptr, 'd'},
  {"hex-suffixes", optional_argument, nullptr, 'x'},
  {"filter", required_argument, nullptr, FILTER_OPTION},
  {"verbose", no_argument, nullptr, VERBOSE_OPTION},
  {"separator", required_argument, nullptr, 't'},
  {"-io-blksize", required_argument, nullptr,
   IO_BLKSIZE_OPTION},  
  {GETOPT_HELP_OPTION_DECL},
  {GETOPT_VERSION_OPTION_DECL},
  {nullptr, 0, nullptr, 0}
};

 
static inline bool
ignorable (int err)
{
  return filter_command && err == EPIPE;
}

static void
set_suffix_length (intmax_t n_units, enum Split_type split_type)
{
#define DEFAULT_SUFFIX_LENGTH 2

  int suffix_length_needed = 0;

   
  if (numeric_suffix_start)
    suffix_auto = false;

   
  if (split_type == type_chunk_bytes || split_type == type_chunk_lines
      || split_type == type_rr)
    {
      intmax_t n_units_end = n_units - 1;
      if (numeric_suffix_start)
        {
          intmax_t n_start;
          strtol_error e = xstrtoimax (numeric_suffix_start, nullptr, 10,
                                       &n_start, "");
          if (e == LONGINT_OK && n_start < n_units)
            {
               
              if (ckd_add (&n_units_end, n_units_end, n_start))
                n_units_end = INTMAX_MAX;
            }

        }
      idx_t alphabet_len = strlen (suffix_alphabet);
      do
        suffix_length_needed++;
      while (n_units_end /= alphabet_len);

      suffix_auto = false;
    }

  if (suffix_length)             
    {
      if (suffix_length < suffix_length_needed)
        error (EXIT_FAILURE, 0,
               _("the suffix length needs to be at least %d"),
               suffix_length_needed);
      suffix_auto = false;
      return;
    }
  else
    suffix_length = MAX (DEFAULT_SUFFIX_LENGTH, suffix_length_needed);
}

void
usage (int status)
{
  if (status != EXIT_SUCCESS)
    emit_try_help ();
  else
    {
      printf (_("\
Usage: %s [OPTION]... [FILE [PREFIX]]\n\
"),
              program_name);
      fputs (_("\
Output pieces of FILE to PREFIXaa, PREFIXab, ...;\n\
default size is 1000 lines, and default PREFIX is 'x'.\n\
"), stdout);

      emit_stdin_note ();
      emit_mandatory_arg_note ();

      fprintf (stdout, _("\
  -a, --suffix-length=N   generate suffixes of length N (default %d)\n\
      --additional-suffix=SUFFIX  append an additional SUFFIX to file names\n\
  -b, --bytes=SIZE        put SIZE bytes per output file\n\
  -C, --line-bytes=SIZE   put at most SIZE bytes of records per output file\n\
  -d                      use numeric suffixes starting at 0, not alphabetic\n\
      --numeric-suffixes[=FROM]  same as -d, but allow setting the start value\
\n\
  -x                      use hex suffixes starting at 0, not alphabetic\n\
      --hex-suffixes[=FROM]  same as -x, but allow setting the start value\n\
  -e, --elide-empty-files  do not generate empty output files with '-n'\n\
      --filter=COMMAND    write to shell COMMAND; file name is $FILE\n\
  -l, --lines=NUMBER      put NUMBER lines/records per output file\n\
  -n, --number=CHUNKS     generate CHUNKS output files; see explanation below\n\
  -t, --separator=SEP     use SEP instead of newline as the record separator;\n\
                            '\\0' (zero) specifies the NUL character\n\
  -u, --unbuffered        immediately copy input to output with '-n r/...'\n\
"), DEFAULT_SUFFIX_LENGTH);
      fputs (_("\
      --verbose           print a diagnostic just before each\n\
                            output file is opened\n\
"), stdout);
      fputs (HELP_OPTION_DESCRIPTION, stdout);
      fputs (VERSION_OPTION_DESCRIPTION, stdout);
      emit_size_note ();
      fputs (_("\n\
CHUNKS may be:\n\
  N       split into N files based on size of input\n\
  K/N     output Kth of N to stdout\n\
  l/N     split into N files without splitting lines/records\n\
  l/K/N   output Kth of N to stdout without splitting lines/records\n\
  r/N     like 'l' but use round robin distribution\n\
  r/K/N   likewise but only output Kth of N to stdout\n\
"), stdout);
      emit_ancillary_info (PROGRAM_NAME);
    }
  exit (status);
}

/* Copy the data in FD to a temporary file, then make that file FD.
   Use BUF, of size BUFSIZE, to copy.  Return the number of
   bytes copied, or -1 (setting errno) on error.  */
static off_t
copy_to_tmpfile (int fd, char *buf, idx_t bufsize)
{
  FILE *tmp;
  if (!temp_stream (&tmp, nullptr))
    return -1;
  off_t copied = 0;
  off_t r;

  while (0 < (r = read (fd, buf, bufsize)))
    {
      if (fwrite (buf, 1, r, tmp) != r)
        return -1;
      if (ckd_add (&copied, copied, r))
        {
          errno = EOVERFLOW;
          return -1;
        }
    }

  if (r < 0)
    return r;
  r = dup2 (fileno (tmp), fd);
  if (r < 0)
    return r;
  if (fclose (tmp) < 0)
    return -1;
  return copied;
}

/* Return the number of bytes that can be read from FD with status ST.
   Store up to the first BUFSIZE bytes of the file's data into BUF,
   and advance the file position by the number of bytes read.  On
   input error, set errno and return -1.  */

static off_t
input_file_size (int fd, struct stat const *st, char *buf, idx_t bufsize)
{
  off_t size = 0;
  do
    {
      ssize_t n_read = read (fd, buf + size, bufsize - size);
      if (n_read <= 0)
        return n_read < 0 ? n_read : size;
      size += n_read;
    }
  while (size < bufsize);

  off_t cur, end;
  if ((usable_st_size (st) && st->st_size < size)
      || (cur = lseek (fd, 0, SEEK_CUR)) < 0
      || cur < size /* E.g., /dev/zero on GNU/Linux.  */
      || (end = lseek (fd, 0, SEEK_END)) < 0)
    {
      char *tmpbuf = xmalloc (bufsize);
      end = copy_to_tmpfile (fd, tmpbuf, bufsize);
      free (tmpbuf);
      if (end < 0)
        return end;
      cur = 0;
    }

  if (end == OFF_T_MAX /* E.g., /dev/zero on GNU/Hurd.  */
      || (cur < end && ckd_add (&size, size, end - cur)))
    {
      errno = EOVERFLOW;
      return -1;
    }

  if (cur < end)
    {
      off_t r = lseek (fd, cur, SEEK_SET);
      if (r < 0)
        return r;
    }

  return size;
}

/* Compute the next sequential output file name and store it into the
   string 'outfile'.  */

static void
next_file_name (void)
{
  /* Index in suffix_alphabet of each character in the suffix.  */
  static idx_t *sufindex;
  static idx_t outbase_length;
  static idx_t outfile_length;
  static idx_t addsuf_length;

  if (! outfile)
    {
      bool overflow, widen;

new_name:
      widen = !! outfile_length;

      if (! widen)
        {
          /* Allocate and initialize the first file name.  */

          outbase_length = strlen (outbase);
          addsuf_length = additional_suffix ? strlen (additional_suffix) : 0;
          overflow = ckd_add (&outfile_length, outbase_length + addsuf_length,
                              suffix_length);
        }
      else
        {
          /* Reallocate and initialize a new wider file name.
             We do this by subsuming the unchanging part of
             the generated suffix into the prefix (base), and
             reinitializing the now one longer suffix.  */

          overflow = ckd_add (&outfile_length, outfile_length, 2);
          suffix_length++;
        }

      idx_t outfile_size;
      overflow |= ckd_add (&outfile_size, outfile_length, 1);
      if (overflow)
        xalloc_die ();
      outfile = xirealloc (outfile, outfile_size);

      if (! widen)
        memcpy (outfile, outbase, outbase_length);
      else
        {
          /* Append the last alphabet character to the file name prefix.  */
          outfile[outbase_length] = suffix_alphabet[sufindex[0]];
          outbase_length++;
        }

      outfile_mid = outfile + outbase_length;
      memset (outfile_mid, suffix_alphabet[0], suffix_length);
      if (additional_suffix)
        memcpy (outfile_mid + suffix_length, additional_suffix, addsuf_length);
      outfile[outfile_length] = 0;

      free (sufindex);
      sufindex = xicalloc (suffix_length, sizeof *sufindex);

      if (numeric_suffix_start)
        {
          affirm (! widen);

          /* Update the output file name.  */
          idx_t i = strlen (numeric_suffix_start);
          memcpy (outfile_mid + suffix_length - i, numeric_suffix_start, i);

          /* Update the suffix index.  */
          idx_t *sufindex_end = sufindex + suffix_length;
          while (i-- != 0)
            *--sufindex_end = numeric_suffix_start[i] - '0';
        }

#if ! _POSIX_NO_TRUNC && HAVE_PATHCONF && defined _PC_NAME_MAX
      /* POSIX requires that if the output file name is too long for
         its directory, 'split' must fail without creating any files.
         This must be checked for explicitly on operating systems that
         silently truncate file names.  */
      {
        char *dir = dir_name (outfile);
        long name_max = pathconf (dir, _PC_NAME_MAX);
        if (0 <= name_max && name_max < base_len (last_component (outfile)))
          error (EXIT_FAILURE, ENAMETOOLONG, "%s", quotef (outfile));
        free (dir);
      }
#endif
    }
  else
    {
      /* Increment the suffix in place, if possible.  */

      idx_t i = suffix_length;
      while (i-- != 0)
        {
          sufindex[i]++;
          if (suffix_auto && i == 0 && ! suffix_alphabet[sufindex[0] + 1])
            goto new_name;
          outfile_mid[i] = suffix_alphabet[sufindex[i]];
          if (outfile_mid[i])
            return;
          sufindex[i] = 0;
          outfile_mid[i] = suffix_alphabet[sufindex[i]];
        }
      error (EXIT_FAILURE, 0, _("output file suffixes exhausted"));
    }
}

/* Create or truncate a file.  */

static int
create (char const *name)
{
  if (!filter_command)
    {
      if (verbose)
        fprintf (stdout, _("creating file %s\n"), quoteaf (name));

      int oflags = O_WRONLY | O_CREAT | O_BINARY;
      int fd = open (name, oflags | O_EXCL, MODE_RW_UGO);
      if (0 <= fd || errno != EEXIST)
        return fd;
      fd = open (name, oflags, MODE_RW_UGO);
      if (fd < 0)
        return fd;
      struct stat out_stat_buf;
      if (fstat (fd, &out_stat_buf) != 0)
        error (EXIT_FAILURE, errno, _("failed to stat %s"), quoteaf (name));
      if (SAME_INODE (in_stat_buf, out_stat_buf))
        error (EXIT_FAILURE, 0, _("%s would overwrite input; aborting"),
               quoteaf (name));
      bool regularish
        = S_ISREG (out_stat_buf.st_mode) || S_TYPEISSHM (&out_stat_buf);
      if (! (regularish && out_stat_buf.st_size == 0)
          && ftruncate (fd, 0) < 0 && regularish)
        error (EXIT_FAILURE, errno, _("%s: error truncating"), quotef (name));

      return fd;
    }
  else
    {
      int fd_pair[2];
      pid_t child_pid;
      char const *shell_prog = getenv ("SHELL");
      if (shell_prog == nullptr)
        shell_prog = "/bin/sh";
      if (setenv ("FILE", name, 1) != 0)
        error (EXIT_FAILURE, errno,
               _("failed to set FILE environment variable"));
      if (verbose)
        fprintf (stdout, _("executing with FILE=%s\n"), quotef (name));
      if (pipe (fd_pair) != 0)
        error (EXIT_FAILURE, errno, _("failed to create pipe"));
      child_pid = fork ();
      if (child_pid == 0)
        {
          /* This is the child process.  If an error occurs here, the
             parent will eventually learn about it after doing a wait,
             at which time it will emit its own error message.  */
          int j;
          /* We have to close any pipes that were opened during an
             earlier call, otherwise this process will be holding a
             write-pipe that will prevent the earlier process from
             reading an EOF on the corresponding read-pipe.  */
          for (j = 0; j < n_open_pipes; ++j)
            if (close (open_pipes[j]) != 0)
              error (EXIT_FAILURE, errno, _("closing prior pipe"));
          if (close (fd_pair[1]))
            error (EXIT_FAILURE, errno, _("closing output pipe"));
          if (fd_pair[0] != STDIN_FILENO)
            {
              if (dup2 (fd_pair[0], STDIN_FILENO) != STDIN_FILENO)
                error (EXIT_FAILURE, errno, _("moving input pipe"));
              if (close (fd_pair[0]) != 0)
                error (EXIT_FAILURE, errno, _("closing input pipe"));
            }
          if (default_SIGPIPE)
            signal (SIGPIPE, SIG_DFL);
          execl (shell_prog, last_component (shell_prog), "-c",
                 filter_command, (char *) nullptr);
          error (EXIT_FAILURE, errno, _("failed to run command: \"%s -c %s\""),
                 shell_prog, filter_command);
        }
      if (child_pid < 0)
        error (EXIT_FAILURE, errno, _("fork system call failed"));
      if (close (fd_pair[0]) != 0)
        error (EXIT_FAILURE, errno, _("failed to close input pipe"));
      filter_pid = child_pid;
      if (n_open_pipes == open_pipes_alloc)
        open_pipes = xpalloc (open_pipes, &open_pipes_alloc, 1,
                              MIN (INT_MAX, IDX_MAX), sizeof *open_pipes);
      open_pipes[n_open_pipes++] = fd_pair[1];
      return fd_pair[1];
    }
}

 
static void
closeout (FILE *fp, int fd, pid_t pid, char const *name)
{
  if (fp != nullptr && fclose (fp) != 0 && ! ignorable (errno))
    error (EXIT_FAILURE, errno, "%s", quotef (name));
  if (fd >= 0)
    {
      if (fp == nullptr && close (fd) < 0)
        error (EXIT_FAILURE, errno, "%s", quotef (name));
      int j;
      for (j = 0; j < n_open_pipes; ++j)
        {
          if (open_pipes[j] == fd)
            {
              open_pipes[j] = open_pipes[--n_open_pipes];
              break;
            }
        }
    }
  if (pid > 0)
    {
      int wstatus;
      if (waitpid (pid, &wstatus, 0) < 0)
        error (EXIT_FAILURE, errno, _("waiting for child process"));
      else if (WIFSIGNALED (wstatus))
        {
          int sig = WTERMSIG (wstatus);
          if (sig != SIGPIPE)
            {
              char signame[MAX (SIG2STR_MAX, INT_BUFSIZE_BOUND (int))];
              if (sig2str (sig, signame) != 0)
                sprintf (signame, "%d", sig);
              error (sig + 128, 0,
                     _("with FILE=%s, signal %s from command: %s"),
                     quotef (name), signame, filter_command);
            }
        }
      else if (WIFEXITED (wstatus))
        {
          int ex = WEXITSTATUS (wstatus);
          if (ex != 0)
            error (ex, 0, _("with FILE=%s, exit %d from command: %s"),
                   quotef (name), ex, filter_command);
        }
      else
        {
           
          error (EXIT_FAILURE, 0,
                 _("unknown status from command (0x%X)"), wstatus + 0u);
        }
    }
}

 

static bool
cwrite (bool new_file_flag, char const *bp, idx_t bytes)
{
  if (new_file_flag)
    {
      if (!bp && bytes == 0 && elide_empty_files)
        return true;
      closeout (nullptr, output_desc, filter_pid, outfile);
      next_file_name ();
      output_desc = create (outfile);
      if (output_desc < 0)
        error (EXIT_FAILURE, errno, "%s", quotef (outfile));
    }

  if (full_write (output_desc, bp, bytes) == bytes)
    return true;
  else
    {
      if (! ignorable (errno))
        error (EXIT_FAILURE, errno, "%s", quotef (outfile));
      return false;
    }
}

 

static void
bytes_split (intmax_t n_bytes, intmax_t rem_bytes,
             char *buf, idx_t bufsize, ssize_t initial_read,
             intmax_t max_files)
{
  bool new_file_flag = true;
  bool filter_ok = true;
  intmax_t opened = 0;
  intmax_t to_write = n_bytes + (0 < rem_bytes);
  bool eof = ! to_write;

  while (! eof)
    {
      ssize_t n_read;
      if (0 <= initial_read)
        {
          n_read = initial_read;
          initial_read = -1;
          eof = n_read < bufsize;
        }
      else
        {
          if (! filter_ok
              && 0 <= lseek (STDIN_FILENO, to_write, SEEK_CUR))
            {
              to_write = n_bytes + (opened + 1 < rem_bytes);
              new_file_flag = true;
            }

          n_read = read (STDIN_FILENO, buf, bufsize);
          if (n_read < 0)
            error (EXIT_FAILURE, errno, "%s", quotef (infile));
          eof = n_read == 0;
        }
      char *bp_out = buf;
      while (0 < to_write && to_write <= n_read)
        {
          if (filter_ok || new_file_flag)
            filter_ok = cwrite (new_file_flag, bp_out, to_write);
          opened += new_file_flag;
          new_file_flag = !max_files || (opened < max_files);
          if (! filter_ok && ! new_file_flag)
            {
               
              n_read = 0;
              eof = true;
              break;
            }
          bp_out += to_write;
          n_read -= to_write;
          to_write = n_bytes + (opened < rem_bytes);
        }
      if (0 < n_read)
        {
          if (filter_ok || new_file_flag)
            filter_ok = cwrite (new_file_flag, bp_out, n_read);
          opened += new_file_flag;
          new_file_flag = false;
          if (! filter_ok && opened == max_files)
            {
               
              break;
            }
          to_write -= n_read;
        }
    }

   
  while (opened++ < max_files)
    cwrite (true, nullptr, 0);
}

 

static void
lines_split (intmax_t n_lines, char *buf, idx_t bufsize)
{
  ssize_t n_read;
  char *bp, *bp_out, *eob;
  bool new_file_flag = true;
  intmax_t n = 0;

  do
    {
      n_read = read (STDIN_FILENO, buf, bufsize);
      if (n_read < 0)
        error (EXIT_FAILURE, errno, "%s", quotef (infile));
      bp = bp_out = buf;
      eob = bp + n_read;
      *eob = eolchar;
      while (true)
        {
          bp = rawmemchr (bp, eolchar);
          if (bp == eob)
            {
              if (eob != bp_out)  
                {
                  idx_t len = eob - bp_out;
                  cwrite (new_file_flag, bp_out, len);
                  new_file_flag = false;
                }
              break;
            }

          ++bp;
          if (++n >= n_lines)
            {
              cwrite (new_file_flag, bp_out, bp - bp_out);
              bp_out = bp;
              new_file_flag = true;
              n = 0;
            }
        }
    }
  while (n_read);
}

 

static void
line_bytes_split (intmax_t n_bytes, char *buf, idx_t bufsize)
{
  ssize_t n_read;
  intmax_t n_out = 0;       
  idx_t n_hold = 0;
  char *hold = nullptr;         
  idx_t hold_size = 0;
  bool split_line = false;   

  do
    {
      n_read = read (STDIN_FILENO, buf, bufsize);
      if (n_read < 0)
        error (EXIT_FAILURE, errno, "%s", quotef (infile));
      idx_t n_left = n_read;
      char *sob = buf;
      while (n_left)
        {
          idx_t split_rest = 0;
          char *eoc = nullptr;
          char *eol;

           
          if (n_bytes - n_out - n_hold <= n_left)
            {
               
              split_rest = n_bytes - n_out - n_hold;
              eoc = sob + split_rest - 1;
              eol = memrchr (sob, eolchar, split_rest);
            }
          else
            eol = memrchr (sob, eolchar, n_left);

           
          if (n_hold && !(!eol && n_out))
            {
              cwrite (n_out == 0, hold, n_hold);
              n_out += n_hold;
              if (n_hold > bufsize)
                hold = xirealloc (hold, bufsize);
              n_hold = 0;
              hold_size = bufsize;
            }

           
          if (eol)
            {
              split_line = true;
              idx_t n_write = eol - sob + 1;
              cwrite (n_out == 0, sob, n_write);
              n_out += n_write;
              n_left -= n_write;
              sob += n_write;
              if (eoc)
                split_rest -= n_write;
            }

           
          if (n_left && !split_line)
            {
              idx_t n_write = eoc ? split_rest : n_left;
              cwrite (n_out == 0, sob, n_write);
              n_out += n_write;
              n_left -= n_write;
              sob += n_write;
              if (eoc)
                split_rest -= n_write;
            }

           
          if ((eoc && split_rest) || (!eoc && n_left))
            {
              idx_t n_buf = eoc ? split_rest : n_left;
              if (hold_size - n_hold < n_buf)
                hold = xpalloc (hold, &hold_size, n_buf - (hold_size - n_hold),
                                -1, sizeof *hold);
              memcpy (hold + n_hold, sob, n_buf);
              n_hold += n_buf;
              n_left -= n_buf;
              sob += n_buf;
            }

           
          if (eoc)
            {
              n_out = 0;
              split_line = false;
            }
        }
    }
  while (n_read);

   
  if (n_hold)
    cwrite (n_out == 0, hold, n_hold);

  free (hold);
}

 

static void
lines_chunk_split (intmax_t k, intmax_t n, char *buf, idx_t bufsize,
                   ssize_t initial_read, off_t file_size)
{
  affirm (n && k <= n);

  intmax_t rem_bytes = file_size % n;
  off_t chunk_size = file_size / n;
  intmax_t chunk_no = 1;
  off_t chunk_end = chunk_size + (0 < rem_bytes);
  off_t n_written = 0;
  bool new_file_flag = true;
  bool chunk_truncated = false;

  if (k > 1 && 0 < file_size)
    {
       
      off_t start = (k - 1) * chunk_size + MIN (k - 1, rem_bytes) - 1;
      if (start < initial_read)
        {
          memmove (buf, buf + start, initial_read - start);
          initial_read -= start;
        }
      else
        {
          if (initial_read < start
              && lseek (STDIN_FILENO, start - initial_read, SEEK_CUR) < 0)
            error (EXIT_FAILURE, errno, "%s", quotef (infile));
          initial_read = -1;
        }
      n_written = start;
      chunk_no = k - 1;
      chunk_end = start + 1;
    }

  while (n_written < file_size)
    {
      char *bp = buf, *eob;
      ssize_t n_read;
      if (0 <= initial_read)
        {
          n_read = initial_read;
          initial_read = -1;
        }
      else
        {
          n_read = read (STDIN_FILENO, buf,
                         MIN (bufsize, file_size - n_written));
          if (n_read < 0)
            error (EXIT_FAILURE, errno, "%s", quotef (infile));
        }
      if (n_read == 0)
        break;  
      chunk_truncated = false;
      eob = buf + n_read;

      while (bp != eob)
        {
          idx_t to_write;
          bool next = false;

           
          off_t skip = MIN (n_read, MAX (0, chunk_end - 1 - n_written));
          char *bp_out = memchr (bp + skip, eolchar, n_read - skip);
          if (bp_out)
            {
              bp_out++;
              next = true;
            }
          else
            bp_out = eob;
          to_write = bp_out - bp;

          if (k == chunk_no)
            {
               
              if (full_write (STDOUT_FILENO, bp, to_write) != to_write)
                write_error ();
            }
          else if (! k)
            cwrite (new_file_flag, bp, to_write);
          n_written += to_write;
          bp += to_write;
          n_read -= to_write;
          new_file_flag = next;

           
          while (next || chunk_end <= n_written)
            {
              if (!next && bp == eob)
                {
                   
                  chunk_truncated = true;
                  break;
                }
              if (k == chunk_no)
                return;
              chunk_end += chunk_size + (chunk_no < rem_bytes);
              chunk_no++;
              if (chunk_end <= n_written)
                {
                  if (! k)
                    cwrite (true, nullptr, 0);
                }
              else
                next = false;
            }
        }
    }

  if (chunk_truncated)
    chunk_no++;

   
  if (!k)
    while (chunk_no++ <= n)
      cwrite (true, nullptr, 0);
}

 

static void
bytes_chunk_extract (intmax_t k, intmax_t n, char *buf, idx_t bufsize,
                     ssize_t initial_read, off_t file_size)
{
  off_t start;
  off_t end;

  assert (0 < k && k <= n);

  start = (k - 1) * (file_size / n) + MIN (k - 1, file_size % n);
  end = k == n ? file_size : k * (file_size / n) + MIN (k, file_size % n);

  if (start < initial_read)
    {
      memmove (buf, buf + start, initial_read - start);
      initial_read -= start;
    }
  else
    {
      if (initial_read < start
          && lseek (STDIN_FILENO, start - initial_read, SEEK_CUR) < 0)
        error (EXIT_FAILURE, errno, "%s", quotef (infile));
      initial_read = -1;
    }

  while (start < end)
    {
      ssize_t n_read;
      if (0 <= initial_read)
        {
          n_read = initial_read;
          initial_read = -1;
        }
      else
        {
          n_read = read (STDIN_FILENO, buf, bufsize);
          if (n_read < 0)
            error (EXIT_FAILURE, errno, "%s", quotef (infile));
        }
      if (n_read == 0)
        break;  
      n_read = MIN (n_read, end - start);
      if (full_write (STDOUT_FILENO, buf, n_read) != n_read
          && ! ignorable (errno))
        error (EXIT_FAILURE, errno, "%s", quotef ("-"));
      start += n_read;
    }
}

typedef struct of_info
{
  char *of_name;
  int ofd;
  FILE *ofile;
  pid_t opid;
} of_t;

enum
{
  OFD_NEW = -1,
  OFD_APPEND = -2
};

 

static bool
ofile_open (of_t *files, idx_t i_check, idx_t nfiles)
{
  bool file_limit = false;

  if (files[i_check].ofd <= OFD_NEW)
    {
      int fd;
      idx_t i_reopen = i_check ? i_check - 1 : nfiles - 1;

       
      while (true)
        {
          if (files[i_check].ofd == OFD_NEW)
            fd = create (files[i_check].of_name);
          else  
            {
               
              fd = open (files[i_check].of_name,
                         O_WRONLY | O_BINARY | O_APPEND | O_NONBLOCK);
            }

          if (0 <= fd)
            break;

          if (!(errno == EMFILE || errno == ENFILE))
            error (EXIT_FAILURE, errno, "%s", quotef (files[i_check].of_name));

          file_limit = true;

           
          while (files[i_reopen].ofd < 0)
            {
              i_reopen = i_reopen ? i_reopen - 1 : nfiles - 1;
               
              if (i_reopen == i_check)
                error (EXIT_FAILURE, errno, "%s",
                       quotef (files[i_check].of_name));
            }

          if (fclose (files[i_reopen].ofile) != 0)
            error (EXIT_FAILURE, errno, "%s", quotef (files[i_reopen].of_name));
          files[i_reopen].ofile = nullptr;
          files[i_reopen].ofd = OFD_APPEND;
        }

      files[i_check].ofd = fd;
      FILE *ofile = fdopen (fd, "a");
      if (!ofile)
        error (EXIT_FAILURE, errno, "%s", quotef (files[i_check].of_name));
      files[i_check].ofile = ofile;
      files[i_check].opid = filter_pid;
      filter_pid = 0;
    }

  return file_limit;
}

 

static void
lines_rr (intmax_t k, intmax_t n, char *buf, idx_t bufsize, of_t **filesp)
{
  bool wrapped = false;
  bool wrote = false;
  bool file_limit;
  idx_t i_file;
  of_t *files IF_LINT (= nullptr);
  intmax_t line_no;

  if (k)
    line_no = 1;
  else
    {
      if (IDX_MAX < n)
        xalloc_die ();
      files = *filesp = xinmalloc (n, sizeof *files);

       
      for (i_file = 0; i_file < n; i_file++)
        {
          next_file_name ();
          files[i_file].of_name = xstrdup (outfile);
          files[i_file].ofd = OFD_NEW;
          files[i_file].ofile = nullptr;
          files[i_file].opid = 0;
        }
      i_file = 0;
      file_limit = false;
    }

  while (true)
    {
      char *bp = buf, *eob;
      ssize_t n_read = read (STDIN_FILENO, buf, bufsize);
      if (n_read < 0)
        error (EXIT_FAILURE, errno, "%s", quotef (infile));
      else if (n_read == 0)
        break;  
      eob = buf + n_read;

      while (bp != eob)
        {
          idx_t to_write;
          bool next = false;

           
          char *bp_out = memchr (bp, eolchar, eob - bp);
          if (bp_out)
            {
              bp_out++;
              next = true;
            }
          else
            bp_out = eob;
          to_write = bp_out - bp;

          if (k)
            {
              if (line_no == k && unbuffered)
                {
                  if (full_write (STDOUT_FILENO, bp, to_write) != to_write)
                    write_error ();
                }
              else if (line_no == k && fwrite (bp, to_write, 1, stdout) != 1)
                {
                  write_error ();
                }
              if (next)
                line_no = (line_no == n) ? 1 : line_no + 1;
            }
          else
            {
               
              file_limit |= ofile_open (files, i_file, n);
              if (unbuffered)
                {
                   
                  if (full_write (files[i_file].ofd, bp, to_write) != to_write
                      && ! ignorable (errno))
                    error (EXIT_FAILURE, errno, "%s",
                           quotef (files[i_file].of_name));
                }
              else if (fwrite (bp, to_write, 1, files[i_file].ofile) != 1
                       && ! ignorable (errno))
                error (EXIT_FAILURE, errno, "%s",
                       quotef (files[i_file].of_name));

              if (! ignorable (errno))
                wrote = true;

              if (file_limit)
                {
                  if (fclose (files[i_file].ofile) != 0)
                    error (EXIT_FAILURE, errno, "%s",
                           quotef (files[i_file].of_name));
                  files[i_file].ofile = nullptr;
                  files[i_file].ofd = OFD_APPEND;
                }
              if (next && ++i_file == n)
                {
                  wrapped = true;
                   
                  if (! wrote)
                    goto no_filters;
                  wrote = false;
                  i_file = 0;
                }
            }

          bp = bp_out;
        }
    }

no_filters:
   
  if (!k)
    {
      idx_t ceiling = wrapped ? n : i_file;
      for (i_file = 0; i_file < n; i_file++)
        {
          if (i_file >= ceiling && !elide_empty_files)
            file_limit |= ofile_open (files, i_file, n);
          if (files[i_file].ofd >= 0)
            closeout (files[i_file].ofile, files[i_file].ofd,
                      files[i_file].opid, files[i_file].of_name);
          files[i_file].ofd = OFD_APPEND;
        }
    }
}

#define FAIL_ONLY_ONE_WAY()					\
  do								\
    {								\
      error (0, 0, _("cannot split in more than one way"));	\
      usage (EXIT_FAILURE);					\
    }								\
  while (0)

 

static _Noreturn void
strtoint_die (char const *msgid, char const *arg)
{
  error (EXIT_FAILURE, errno == EINVAL ? 0 : errno, "%s: %s",
         gettext (msgid), quote (arg));
}

 
#define OVERFLOW_OK LONGINT_OVERFLOW

 

static intmax_t
parse_n_units (char const *arg, char const *multipliers, char const *msgid)
{
  intmax_t n;
  if (OVERFLOW_OK < xstrtoimax (arg, nullptr, 10, &n, multipliers) || n < 1)
    strtoint_die (msgid, arg);
  return n;
}

 

static void
parse_chunk (intmax_t *k_units, intmax_t *n_units, char const *arg)
{
  char *argend;
  strtol_error e = xstrtoimax (arg, &argend, 10, n_units, "");
  if (e == LONGINT_INVALID_SUFFIX_CHAR && *argend == '/')
    {
      *k_units = *n_units;
      *n_units = parse_n_units (argend + 1, "",
                                N_("invalid number of chunks"));
      if (! (0 < *k_units && *k_units <= *n_units))
        error (EXIT_FAILURE, 0, "%s: %s", _("invalid chunk number"),
               quote_mem (arg, argend - arg));
    }
  else if (! (e <= OVERFLOW_OK && 0 < *n_units))
    strtoint_die (N_("invalid number of chunks"), arg);
}


int
main (int argc, char **argv)
{
  enum Split_type split_type = type_undef;
  idx_t in_blk_size = 0;	 
  idx_t page_size = getpagesize ();
  intmax_t k_units = 0;
  intmax_t n_units = 0;

  static char const multipliers[] = "bEGKkMmPQRTYZ0";
  int c;
  int digits_optind = 0;
  off_t file_size = OFF_T_MAX;

  initialize_main (&argc, &argv);
  set_program_name (argv[0]);
  setlocale (LC_ALL, "");
  bindtextdomain (PACKAGE, LOCALEDIR);
  textdomain (PACKAGE);

  atexit (close_stdout);

   

  infile = bad_cast ("-");
  outbase = bad_cast ("x");

  while (true)
    {
       
      int this_optind = optind ? optind : 1;

      c = getopt_long (argc, argv, "0123456789C:a:b:del:n:t:ux",
                       longopts, nullptr);
      if (c == -1)
        break;

      switch (c)
        {
        case 'a':
          suffix_length = xdectoimax (optarg, 0, IDX_MAX,
                                      "", _("invalid suffix length"), 0);
          break;

        case ADDITIONAL_SUFFIX_OPTION:
          {
            int suffix_len = strlen (optarg);
            if (last_component (optarg) != optarg
                || (suffix_len && ISSLASH (optarg[suffix_len - 1])))
              {
                error (0, 0,
                       _("invalid suffix %s, contains directory separator"),
                       quote (optarg));
                usage (EXIT_FAILURE);
              }
          }
          additional_suffix = optarg;
          break;

        case 'b':
          if (split_type != type_undef)
            FAIL_ONLY_ONE_WAY ();
          split_type = type_bytes;
          n_units = parse_n_units (optarg, multipliers,
                                   N_("invalid number of bytes"));
          break;

        case 'l':
          if (split_type != type_undef)
            FAIL_ONLY_ONE_WAY ();
          split_type = type_lines;
          n_units = parse_n_units (optarg, "", N_("invalid number of lines"));
          break;

        case 'C':
          if (split_type != type_undef)
            FAIL_ONLY_ONE_WAY ();
          split_type = type_byteslines;
          n_units = parse_n_units (optarg, multipliers,
                                   N_("invalid number of lines"));
          break;

        case 'n':
          if (split_type != type_undef)
            FAIL_ONLY_ONE_WAY ();
           
          while (isspace (to_uchar (*optarg)))
            optarg++;
          if (STRNCMP_LIT (optarg, "r/") == 0)
            {
              split_type = type_rr;
              optarg += 2;
            }
          else if (STRNCMP_LIT (optarg, "l/") == 0)
            {
              split_type = type_chunk_lines;
              optarg += 2;
            }
          else
            split_type = type_chunk_bytes;
          parse_chunk (&k_units, &n_units, optarg);
          break;

        case 'u':
          unbuffered = true;
          break;

        case 't':
          {
            char neweol = optarg[0];
            if (! neweol)
              error (EXIT_FAILURE, 0, _("empty record separator"));
            if (optarg[1])
              {
                if (STREQ (optarg, "\\0"))
                  neweol = '\0';
                else
                  {
                     
                    error (EXIT_FAILURE, 0, _("multi-character separator %s"),
                           quote (optarg));
                  }
              }
             
            if (0 <= eolchar && neweol != eolchar)
              {
                error (EXIT_FAILURE, 0,
                       _("multiple separator characters specified"));
              }

            eolchar = neweol;
          }
          break;

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
          if (split_type == type_undef)
            {
              split_type = type_digits;
              n_units = 0;
            }
          if (split_type != type_undef && split_type != type_digits)
            FAIL_ONLY_ONE_WAY ();
          if (digits_optind != 0 && digits_optind != this_optind)
            n_units = 0;	 
          digits_optind = this_optind;
          if (ckd_mul (&n_units, n_units, 10)
              || ckd_add (&n_units, n_units, c - '0'))
            n_units = INTMAX_MAX;
          break;

        case 'd':
        case 'x':
          if (c == 'd')
            suffix_alphabet = "0123456789";
          else
            suffix_alphabet = "0123456789abcdef";
          if (optarg)
            {
              if (strlen (optarg) != strspn (optarg, suffix_alphabet))
                {
                  error (0, 0,
                         (c == 'd') ?
                           _("%s: invalid start value for numerical suffix") :
                           _("%s: invalid start value for hexadecimal suffix"),
                         quote (optarg));
                  usage (EXIT_FAILURE);
                }
              else
                {
                   
                  while (*optarg == '0' && *(optarg + 1) != '\0')
                    optarg++;
                  numeric_suffix_start = optarg;
                }
            }
          break;

        case 'e':
          elide_empty_files = true;
          break;

        case FILTER_OPTION:
          filter_command = optarg;
          break;

        case IO_BLKSIZE_OPTION:
          in_blk_size = xdectoumax (optarg, 1,
                                    MIN (SYS_BUFSIZE_MAX,
                                         MIN (IDX_MAX, SIZE_MAX) - 1),
                                    multipliers, _("invalid IO block size"), 0);
          break;

        case VERBOSE_OPTION:
          verbose = true;
          break;

        case_GETOPT_HELP_CHAR;

        case_GETOPT_VERSION_CHAR (PROGRAM_NAME, AUTHORS);

        default:
          usage (EXIT_FAILURE);
        }
    }

  if (k_units != 0 && filter_command)
    {
      error (0, 0, _("--filter does not process a chunk extracted to stdout"));
      usage (EXIT_FAILURE);
    }

   
  if (split_type == type_undef)
    {
      split_type = type_lines;
      n_units = 1000;
    }

  if (n_units == 0)
    {
      error (0, 0, _("invalid number of lines: %s"), quote ("0"));
      usage (EXIT_FAILURE);
    }

  if (eolchar < 0)
    eolchar = '\n';

  set_suffix_length (n_units, split_type);

   

  if (optind < argc)
    infile = argv[optind++];

  if (optind < argc)
    outbase = argv[optind++];

  if (optind < argc)
    {
      error (0, 0, _("extra operand %s"), quote (argv[optind]));
      usage (EXIT_FAILURE);
    }

   
  if (numeric_suffix_start && strlen (numeric_suffix_start) > suffix_length)
    {
      error (0, 0, _("numerical suffix start value is too large "
                     "for the suffix length"));
      usage (EXIT_FAILURE);
    }

   
  if (! STREQ (infile, "-")
      && fd_reopen (STDIN_FILENO, infile, O_RDONLY, 0) < 0)
    error (EXIT_FAILURE, errno, _("cannot open %s for reading"),
           quoteaf (infile));

   
  xset_binary_mode (STDIN_FILENO, O_BINARY);

   
  fdadvise (STDIN_FILENO, 0, 0, FADVISE_SEQUENTIAL);

   

  if (fstat (STDIN_FILENO, &in_stat_buf) != 0)
    error (EXIT_FAILURE, errno, "%s", quotef (infile));

  if (in_blk_size == 0)
    {
      in_blk_size = io_blksize (in_stat_buf);
      if (SYS_BUFSIZE_MAX < in_blk_size)
        in_blk_size = SYS_BUFSIZE_MAX;
    }

  char *buf = xalignalloc (page_size, in_blk_size + 1);
  ssize_t initial_read = -1;

  if (split_type == type_chunk_bytes || split_type == type_chunk_lines)
    {
      file_size = input_file_size (STDIN_FILENO, &in_stat_buf,
                                   buf, in_blk_size);
      if (file_size < 0)
        error (EXIT_FAILURE, errno, _("%s: cannot determine file size"),
               quotef (infile));
      initial_read = MIN (file_size, in_blk_size);
    }

   
  if (filter_command)
    default_SIGPIPE = signal (SIGPIPE, SIG_IGN) == SIG_DFL;

  switch (split_type)
    {
    case type_digits:
    case type_lines:
      lines_split (n_units, buf, in_blk_size);
      break;

    case type_bytes:
      bytes_split (n_units, 0, buf, in_blk_size, -1, 0);
      break;

    case type_byteslines:
      line_bytes_split (n_units, buf, in_blk_size);
      break;

    case type_chunk_bytes:
      if (k_units == 0)
        bytes_split (file_size / n_units, file_size % n_units,
                     buf, in_blk_size, initial_read, n_units);
      else
        bytes_chunk_extract (k_units, n_units, buf, in_blk_size, initial_read,
                             file_size);
      break;

    case type_chunk_lines:
      lines_chunk_split (k_units, n_units, buf, in_blk_size, initial_read,
                         file_size);
      break;

    case type_rr:
       
      {
        of_t *files;
        lines_rr (k_units, n_units, buf, in_blk_size, &files);
      }
      break;

    default:
      affirm (false);
    }

  if (close (STDIN_FILENO) != 0)
    error (EXIT_FAILURE, errno, "%s", quotef (infile));
  closeout (nullptr, output_desc, filter_pid, outfile);

  main_exit (EXIT_SUCCESS);
}
