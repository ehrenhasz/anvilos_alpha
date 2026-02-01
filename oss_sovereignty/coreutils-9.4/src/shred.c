 

 

 
#define PROGRAM_NAME "shred"

#define AUTHORS proper_name ("Colin Plumb")

#include <config.h>

#include <getopt.h>
#include <stdio.h>
#include <setjmp.h>
#include <sys/types.h>
#if defined __linux__ && HAVE_SYS_MTIO_H
# include <sys/mtio.h>
#endif

#include "system.h"
#include "alignalloc.h"
#include "argmatch.h"
#include "assure.h"
#include "xdectoint.h"
#include "fcntl--.h"
#include "human.h"
#include "randint.h"
#include "randread.h"
#include "renameatu.h"
#include "stat-size.h"

 
enum { DEFAULT_PASSES = 3 };

 
enum { VERBOSE_UPDATE = 5 };

 
enum { SECTOR_SIZE = 512 };
enum { SECTOR_MASK = SECTOR_SIZE - 1 };
static_assert (0 < SECTOR_SIZE && (SECTOR_SIZE & SECTOR_MASK) == 0);

enum remove_method
{
  remove_none = 0,       
  remove_unlink,         
  remove_wipe,           
  remove_wipesync        
};

static char const *const remove_args[] =
{
  "unlink", "wipe", "wipesync", nullptr
};

static enum remove_method const remove_methods[] =
{
  remove_unlink, remove_wipe, remove_wipesync
};

struct Options
{
  bool force;		 
  size_t n_iterations;	 
  off_t size;		 
  enum remove_method remove_file;  
  bool verbose;		 
  bool exact;		 
  bool zero_fill;	 
};

 
enum
{
  RANDOM_SOURCE_OPTION = CHAR_MAX + 1
};

static struct option const long_opts[] =
{
  {"exact", no_argument, nullptr, 'x'},
  {"force", no_argument, nullptr, 'f'},
  {"iterations", required_argument, nullptr, 'n'},
  {"size", required_argument, nullptr, 's'},
  {"random-source", required_argument, nullptr, RANDOM_SOURCE_OPTION},
  {"remove", optional_argument, nullptr, 'u'},
  {"verbose", no_argument, nullptr, 'v'},
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
      printf (_("Usage: %s [OPTION]... FILE...\n"), program_name);
      fputs (_("\
Overwrite the specified FILE(s) repeatedly, in order to make it harder\n\
for even very expensive hardware probing to recover the data.\n\
"), stdout);
      fputs (_("\
\n\
If FILE is -, shred standard output.\n\
"), stdout);

      emit_mandatory_arg_note ();

      printf (_("\
  -f, --force    change permissions to allow writing if necessary\n\
  -n, --iterations=N  overwrite N times instead of the default (%d)\n\
      --random-source=FILE  get random bytes from FILE\n\
  -s, --size=N   shred this many bytes (suffixes like K, M, G accepted)\n\
"), DEFAULT_PASSES);
      fputs (_("\
  -u             deallocate and remove file after overwriting\n\
      --remove[=HOW]  like -u but give control on HOW to delete;  See below\n\
  -v, --verbose  show progress\n\
  -x, --exact    do not round file sizes up to the next full block;\n\
                   this is the default for non-regular files\n\
  -z, --zero     add a final overwrite with zeros to hide shredding\n\
"), stdout);
      fputs (HELP_OPTION_DESCRIPTION, stdout);
      fputs (VERSION_OPTION_DESCRIPTION, stdout);
      fputs (_("\
\n\
Delete FILE(s) if --remove (-u) is specified.  The default is not to remove\n\
the files because it is common to operate on device files like /dev/hda,\n\
and those files usually should not be removed.\n\
The optional HOW parameter indicates how to remove a directory entry:\n\
'unlink' => use a standard unlink call.\n\
'wipe' => also first obfuscate bytes in the name.\n\
'wipesync' => also sync each obfuscated byte to the device.\n\
The default mode is 'wipesync', but note it can be expensive.\n\
\n\
"), stdout);
      fputs (_("\
CAUTION: shred assumes the file system and hardware overwrite data in place.\n\
Although this is common, many platforms operate otherwise.  Also, backups\n\
and mirrors may contain unremovable copies that will let a shredded file\n\
be recovered later.  See the GNU coreutils manual for details.\n\
"), stdout);
      emit_ancillary_info (PROGRAM_NAME);
    }
  exit (status);
}

/*
 * Determine if pattern type is periodic or not.
 */
static bool
periodic_pattern (int type)
{
  if (type <= 0)
    return false;

  unsigned char r[3];
  unsigned int bits = type & 0xfff;

  bits |= bits << 12;
  r[0] = (bits >> 4) & 255;
  r[1] = (bits >> 8) & 255;
  r[2] = bits & 255;

  return (r[0] != r[1]) || (r[0] != r[2]);
}

/*
 * Fill a buffer with a fixed pattern.
 *
 * The buffer must be at least 3 bytes long, even if
 * size is less.  Larger sizes are filled exactly.
 */
static void
fillpattern (int type, unsigned char *r, size_t size)
{
  size_t i;
  unsigned int bits = type & 0xfff;

  bits |= bits << 12;
  r[0] = (bits >> 4) & 255;
  r[1] = (bits >> 8) & 255;
  r[2] = bits & 255;
  for (i = 3; i <= size / 2; i *= 2)
    memcpy (r + i, r, i);
  if (i < size)
    memcpy (r + i, r, size - i);

  /* Invert the first bit of every sector. */
  if (type & 0x1000)
    for (i = 0; i < size; i += SECTOR_SIZE)
      r[i] ^= 0x80;
}

/*
 * Generate a 6-character (+ nul) pass name string
 * FIXME: allow translation of "random".
 */
#define PASS_NAME_SIZE 7
static void
passname (unsigned char const *data, char name[PASS_NAME_SIZE])
{
  if (data)
    sprintf (name, "%02x%02x%02x", data[0], data[1], data[2]);
  else
    memcpy (name, "random", PASS_NAME_SIZE);
}

/* Return true when it's ok to ignore an fsync or fdatasync
   failure that set errno to ERRNO_VAL.  */
static bool
ignorable_sync_errno (int errno_val)
{
  return (errno_val == EINVAL
          || errno_val == EBADF
          /* HP-UX does this */
          || errno_val == EISDIR);
}

/* Request that all data for FD be transferred to the corresponding
   storage device.  QNAME is the file name (quoted for colons).
   Report any errors found.  Return 0 on success, -1
   (setting errno) on failure.  It is not an error if fdatasync and/or
   fsync is not supported for this file, or if the file is not a
   writable file descriptor.  */
static int
dosync (int fd, char const *qname)
{
  int err;

#if HAVE_FDATASYNC
  if (fdatasync (fd) == 0)
    return 0;
  err = errno;
  if ( ! ignorable_sync_errno (err))
    {
      error (0, err, _("%s: fdatasync failed"), qname);
      errno = err;
      return -1;
    }
#endif

  if (fsync (fd) == 0)
    return 0;
  err = errno;
  if ( ! ignorable_sync_errno (err))
    {
      error (0, err, _("%s: fsync failed"), qname);
      errno = err;
      return -1;
    }

  sync ();
  return 0;
}

/* Turn on or off direct I/O mode for file descriptor FD, if possible.
   Try to turn it on if ENABLE is true.  Otherwise, try to turn it off.  */
static void
direct_mode (int fd, bool enable)
{
  if (O_DIRECT)
    {
      int fd_flags = fcntl (fd, F_GETFL);
      if (0 < fd_flags)
        {
          int new_flags = (enable
                           ? (fd_flags | O_DIRECT)
                           : (fd_flags & ~O_DIRECT));
          if (new_flags != fd_flags)
            fcntl (fd, F_SETFL, new_flags);
        }
    }

#if HAVE_DIRECTIO && defined DIRECTIO_ON && defined DIRECTIO_OFF
  /* This is Solaris-specific.  */
  directio (fd, enable ? DIRECTIO_ON : DIRECTIO_OFF);
#endif
}

/* Rewind FD; its status is ST.  */
static bool
dorewind (int fd, struct stat const *st)
{
  if (S_ISCHR (st->st_mode))
    {
#if defined __linux__ && HAVE_SYS_MTIO_H
      /* In the Linux kernel, lseek does not work on tape devices; it
         returns a randomish value instead.  Try the low-level tape
         rewind operation first.  */
      struct mtop op;
      op.mt_op = MTREW;
      op.mt_count = 1;
      if (ioctl (fd, MTIOCTOP, &op) == 0)
        return true;
#endif
    }
  off_t offset = lseek (fd, 0, SEEK_SET);
  if (0 < offset)
    errno = EINVAL;
  return offset == 0;
}

/* By convention, negative sizes represent unknown values.  */

static bool
known (off_t size)
{
  return 0 <= size;
}

/*
 * Do pass number K of N, writing *SIZEP bytes of the given pattern TYPE
 * to the file descriptor FD.  K and N are passed in only for verbose
 * progress message purposes.  If N == 0, no progress messages are printed.
 *
 * If *SIZEP == -1, the size is unknown, and it will be filled in as soon
 * as writing fails with ENOSPC.
 *
 * Return 1 on write error, -1 on other error, 0 on success.
 */
static int
dopass (int fd, struct stat const *st, char const *qname, off_t *sizep,
        int type, struct randread_source *s,
        unsigned long int k, unsigned long int n)
{
  off_t size = *sizep;
  off_t offset;			/* Current file position */
  time_t thresh IF_LINT ( = 0);	/* Time to maybe print next status update */
  time_t now = 0;		/* Current time */
  size_t lim;			/* Amount of data to try writing */
  size_t soff;			/* Offset into buffer for next write */
  ssize_t ssize;		/* Return value from write */

  /* Fill pattern buffer.  Aligning it to a page so we can do direct I/O.  */
  size_t page_size = getpagesize ();
#define PERIODIC_OUTPUT_SIZE (60 * 1024)
#define NONPERIODIC_OUTPUT_SIZE (64 * 1024)
  static_assert (PERIODIC_OUTPUT_SIZE % 3 == 0);
  size_t output_size = periodic_pattern (type)
                       ? PERIODIC_OUTPUT_SIZE : NONPERIODIC_OUTPUT_SIZE;
#define FILLPATTERN_SIZE (((output_size + 2) / 3) * 3) /* Multiple of 3 */
  unsigned char *pbuf = xalignalloc (page_size, FILLPATTERN_SIZE);

  char pass_string[PASS_NAME_SIZE];	/* Name of current pass */
  bool write_error = false;
  bool other_error = false;

  /* Printable previous offset into the file */
  char previous_offset_buf[LONGEST_HUMAN_READABLE + 1];
  char const *previous_human_offset;

  /* As a performance tweak, avoid direct I/O for small sizes,
     as it's just a performance rather then security consideration,
     and direct I/O can often be unsupported for small non aligned sizes.  */
  bool try_without_directio = 0 < size && size < output_size;
  if (! try_without_directio)
    direct_mode (fd, true);

  if (! dorewind (fd, st))
    {
      error (0, errno, _("%s: cannot rewind"), qname);
      other_error = true;
      goto free_pattern_mem;
    }

  /* Constant fill patterns need only be set up once. */
  if (type >= 0)
    {
      lim = known (size) && size < FILLPATTERN_SIZE ? size : FILLPATTERN_SIZE;
      fillpattern (type, pbuf, lim);
      passname (pbuf, pass_string);
    }
  else
    {
      passname (0, pass_string);
    }

  /* Set position if first status update */
  if (n)
    {
      error (0, 0, _("%s: pass %lu/%lu (%s)..."), qname, k, n, pass_string);
      thresh = time (nullptr) + VERBOSE_UPDATE;
      previous_human_offset = "";
    }

  offset = 0;
  while (true)
    {
      /* How much to write this time? */
      lim = output_size;
      if (known (size) && size - offset < output_size)
        {
          if (size < offset)
            break;
          lim = size - offset;
          if (!lim)
            break;
        }
      if (type < 0)
        randread (s, pbuf, lim);
      /* Loop to retry partial writes. */
      for (soff = 0; soff < lim; soff += ssize)
        {
          ssize = write (fd, pbuf + soff, lim - soff);
          if (ssize <= 0)
            {
              if (! known (size) && (ssize == 0 || errno == ENOSPC))
                {
                  /* We have found the end of the file.  */
                  if (soff <= OFF_T_MAX - offset)
                    *sizep = size = offset + soff;
                  break;
                }
              else
                {
                  int errnum = errno;
                  char buf[INT_BUFSIZE_BOUND (uintmax_t)];

                  /* Retry without direct I/O since this may not be supported
                     at all on some (file) systems, or with the current size.
                     I.e., a specified --size that is not aligned, or when
                     dealing with slop at the end of a file with --exact.  */
                  if (! try_without_directio && errno == EINVAL)
                    {
                      direct_mode (fd, false);
                      ssize = 0;
                      try_without_directio = true;
                      continue;
                    }
                  error (0, errnum, _("%s: error writing at offset %s"),
                         qname, umaxtostr (offset + soff, buf));

                  /* 'shred' is often used on bad media, before throwing it
                     out.  Thus, it shouldn't give up on bad blocks.  This
                     code works because lim is always a multiple of
                     SECTOR_SIZE, except at the end.  This size constraint
                     also enables direct I/O on some (file) systems.  */
                  static_assert (PERIODIC_OUTPUT_SIZE % SECTOR_SIZE == 0);
                  static_assert (NONPERIODIC_OUTPUT_SIZE % SECTOR_SIZE == 0);
                  if (errnum == EIO && known (size)
                      && (soff | SECTOR_MASK) < lim)
                    {
                      size_t soff1 = (soff | SECTOR_MASK) + 1;
                      if (lseek (fd, offset + soff1, SEEK_SET) != -1)
                        {
                          /* Arrange to skip this block. */
                          ssize = soff1 - soff;
                          write_error = true;
                          continue;
                        }
                      error (0, errno, _("%s: lseek failed"), qname);
                    }
                  other_error = true;
                  goto free_pattern_mem;
                }
            }
        }

      /* Okay, we have written "soff" bytes. */

      if (OFF_T_MAX - offset < soff)
        {
          error (0, 0, _("%s: file too large"), qname);
          other_error = true;
          goto free_pattern_mem;
        }

      offset += soff;

      bool done = offset == size;

      /* Time to print progress? */
      if (n && ((done && *previous_human_offset)
                || thresh <= (now = time (nullptr))))
        {
          char offset_buf[LONGEST_HUMAN_READABLE + 1];
          char size_buf[LONGEST_HUMAN_READABLE + 1];
          int human_progress_opts = (human_autoscale | human_SI
                                     | human_base_1024 | human_B);
          char const *human_offset
            = human_readable (offset, offset_buf,
                              human_floor | human_progress_opts, 1, 1);

          if (done || !STREQ (previous_human_offset, human_offset))
            {
              if (! known (size))
                error (0, 0, _("%s: pass %lu/%lu (%s)...%s"),
                       qname, k, n, pass_string, human_offset);
              else
                {
                  uintmax_t off = offset;
                  int percent = (size == 0
                                 ? 100
                                 : (off <= TYPE_MAXIMUM (uintmax_t) / 100
                                    ? off * 100 / size
                                    : off / (size / 100)));
                  char const *human_size
                    = human_readable (size, size_buf,
                                      human_ceiling | human_progress_opts,
                                      1, 1);
                  if (done)
                    human_offset = human_size;
                  error (0, 0, _("%s: pass %lu/%lu (%s)...%s/%s %d%%"),
                         qname, k, n, pass_string, human_offset, human_size,
                         percent);
                }

              strcpy (previous_offset_buf, human_offset);
              previous_human_offset = previous_offset_buf;
              thresh = now + VERBOSE_UPDATE;

              /*
               * Force periodic syncs to keep displayed progress accurate
               * FIXME: Should these be present even if -v is not enabled,
               * to keep the buffer cache from filling with dirty pages?
               * It's a common problem with programs that do lots of writes,
               * like mkfs.
               */
              if (dosync (fd, qname) != 0)
                {
                  if (errno != EIO)
                    {
                      other_error = true;
                      goto free_pattern_mem;
                    }
                  write_error = true;
                }
            }
        }
    }

  /* Force what we just wrote to hit the media. */
  if (dosync (fd, qname) != 0)
    {
      if (errno != EIO)
        {
          other_error = true;
          goto free_pattern_mem;
        }
      write_error = true;
    }

free_pattern_mem:
  alignfree (pbuf);

  return other_error ? -1 : write_error;
}

/*
 * The passes start and end with a random pass, and the passes in between
 * are done in random order.  The idea is to deprive someone trying to
 * reverse the process of knowledge of the overwrite patterns, so they
 * have the additional step of figuring out what was done to the device
 * before they can try to reverse or cancel it.
 *
 * First, all possible 1-bit patterns.  There are two of them.
 * Then, all possible 2-bit patterns.  There are four, but the two
 * which are also 1-bit patterns can be omitted.
 * Then, all possible 3-bit patterns.  Likewise, 8-2 = 6.
 * Then, all possible 4-bit patterns.  16-4 = 12.
 *
 * The basic passes are:
 * 1-bit: 0x000, 0xFFF
 * 2-bit: 0x555, 0xAAA
 * 3-bit: 0x249, 0x492, 0x924, 0x6DB, 0xB6D, 0xDB6 (+ 1-bit)
 *        100100100100         110110110110
 *           9   2   4            D   B   6
 * 4-bit: 0x111, 0x222, 0x333, 0x444, 0x666, 0x777,
 *        0x888, 0x999, 0xBBB, 0xCCC, 0xDDD, 0xEEE (+ 1-bit, 2-bit)
 * Adding three random passes at the beginning, middle and end
 * produces the default 25-pass structure.
 *
 * The next extension would be to 5-bit and 6-bit patterns.
 * There are 30 uncovered 5-bit patterns and 64-8-2 = 46 uncovered
 * 6-bit patterns, so they would increase the time required
 * significantly.  4-bit patterns are enough for most purposes.
 *
 * The main gotcha is that this would require a trickier encoding,
 * since lcm(2,3,4) = 12 bits is easy to fit into an int, but
 * lcm(2,3,4,5) = 60 bits is not.
 *
 * One extension that is included is to complement the first bit in each
 * 512-byte block, to alter the phase of the encoded data in the more
 * complex encodings.  This doesn't apply to MFM, so the 1-bit patterns
 * are considered part of the 3-bit ones and the 2-bit patterns are
 * considered part of the 4-bit patterns.
 *
 *
 * How does the generalization to variable numbers of passes work?
 *
 * Here's how...
 * Have an ordered list of groups of passes.  Each group is a set.
 * Take as many groups as will fit, plus a random subset of the
 * last partial group, and place them into the passes list.
 * Then shuffle the passes list into random order and use that.
 *
 * One extra detail: if we can't include a large enough fraction of the
 * last group to be interesting, then just substitute random passes.
 *
 * If you want more passes than the entire list of groups can
 * provide, just start repeating from the beginning of the list.
 */
static int const
  patterns[] =
{
  -2,				/* 2 random passes */
  2, 0x000, 0xFFF,		/* 1-bit */
  2, 0x555, 0xAAA,		/* 2-bit */
  -1,				/* 1 random pass */
  6, 0x249, 0x492, 0x6DB, 0x924, 0xB6D, 0xDB6,	/* 3-bit */
  12, 0x111, 0x222, 0x333, 0x444, 0x666, 0x777,
  0x888, 0x999, 0xBBB, 0xCCC, 0xDDD, 0xEEE,	/* 4-bit */
  -1,				/* 1 random pass */
        /* The following patterns have the first bit per block flipped */
  8, 0x1000, 0x1249, 0x1492, 0x16DB, 0x1924, 0x1B6D, 0x1DB6, 0x1FFF,
  14, 0x1111, 0x1222, 0x1333, 0x1444, 0x1555, 0x1666, 0x1777,
  0x1888, 0x1999, 0x1AAA, 0x1BBB, 0x1CCC, 0x1DDD, 0x1EEE,
  -1,				/* 1 random pass */
  0				/* End */
};

/*
 * Generate a random wiping pass pattern with num passes.
 * This is a two-stage process.  First, the passes to include
 * are chosen, and then they are shuffled into the desired
 * order.
 */
static void
genpattern (int *dest, size_t num, struct randint_source *s)
{
  size_t randpasses;
  int const *p;
  int *d;
  size_t n;
  size_t accum, top, swap;
  int k;

  if (!num)
    return;

  /* Stage 1: choose the passes to use */
  p = patterns;
  randpasses = 0;
  d = dest;			/* Destination for generated pass list */
  n = num;			/* Passes remaining to fill */

  while (true)
    {
      k = *p++;			/* Block descriptor word */
      if (!k)
        {			/* Loop back to the beginning */
          p = patterns;
        }
      else if (k < 0)
        {			/* -k random passes */
          k = -k;
          if ((size_t) k >= n)
            {
              randpasses += n;
              break;
            }
          randpasses += k;
          n -= k;
        }
      else if ((size_t) k <= n)
        {			/* Full block of patterns */
          memcpy (d, p, k * sizeof (int));
          p += k;
          d += k;
          n -= k;
        }
      else if (n < 2 || 3 * n < (size_t) k)
        {			/* Finish with random */
          randpasses += n;
          break;
        }
      else
        {			/* Pad out with n of the k available */
          do
            {
              if (n == (size_t) k || randint_choose (s, k) < n)
                {
                  *d++ = *p;
                  n--;
                }
              p++;
              k--;
            }
          while (n);
          break;
        }
    }
  top = num - randpasses;	/* Top of initialized data */
  /* affirm (d == dest + top); */

  /*
   * We now have fixed patterns in the dest buffer up to
   * "top", and we need to scramble them, with "randpasses"
   * random passes evenly spaced among them.
   *
   * We want one at the beginning, one at the end, and
   * evenly spaced in between.  To do this, we basically
   * use Bresenham's line draw (a.k.a DDA) algorithm
   * to draw a line with slope (randpasses-1)/(num-1).
   * (We use a positive accumulator and count down to
   * do this.)
   *
   * So for each desired output value, we do the following:
   * - If it should be a random pass, copy the pass type
   *   to top++, out of the way of the other passes, and
   *   set the current pass to -1 (random).
   * - If it should be a normal pattern pass, choose an
   *   entry at random between here and top-1 (inclusive)
   *   and swap the current entry with that one.
   */
  randpasses--;			/* To speed up later math */
  accum = randpasses;		/* Bresenham DDA accumulator */
  for (n = 0; n < num; n++)
    {
      if (accum <= randpasses)
        {
          accum += num - 1;
          dest[top++] = dest[n];
          dest[n] = -1;
        }
      else
        {
          swap = n + randint_choose (s, top - n);
          k = dest[n];
          dest[n] = dest[swap];
          dest[swap] = k;
        }
      accum -= randpasses;
    }
  /* affirm (top == num); */
}

/*
 * The core routine to actually do the work.  This overwrites the first
 * size bytes of the given fd.  Return true if successful.
 */
static bool
do_wipefd (int fd, char const *qname, struct randint_source *s,
           struct Options const *flags)
{
  size_t i;
  struct stat st;
  off_t size;		/* Size to write, size to read */
  off_t i_size = 0;	/* For small files, initial size to overwrite inode */
  unsigned long int n;	/* Number of passes for printing purposes */
  int *passarray;
  bool ok = true;
  struct randread_source *rs;

  n = 0;		/* dopass takes n == 0 to mean "don't print progress" */
  if (flags->verbose)
    n = flags->n_iterations + flags->zero_fill;

  if (fstat (fd, &st))
    {
      error (0, errno, _("%s: fstat failed"), qname);
      return false;
    }

  /* If we know that we can't possibly shred the file, give up now.
     Otherwise, we may go into an infinite loop writing data before we
     find that we can't rewind the device.  */
  if ((S_ISCHR (st.st_mode) && isatty (fd))
      || S_ISFIFO (st.st_mode)
      || S_ISSOCK (st.st_mode))
    {
      error (0, 0, _("%s: invalid file type"), qname);
      return false;
    }
  else if (S_ISREG (st.st_mode) && st.st_size < 0)
    {
      error (0, 0, _("%s: file has negative size"), qname);
      return false;
    }

  /* Allocate pass array */
  passarray = xnmalloc (flags->n_iterations, sizeof *passarray);

  size = flags->size;
  if (size == -1)
    {
      if (S_ISREG (st.st_mode))
        {
          size = st.st_size;

          if (! flags->exact)
            {
              /* Round up to the nearest block size to clear slack space.  */
              off_t remainder = size % ST_BLKSIZE (st);
              if (size && size < ST_BLKSIZE (st))
                i_size = size;
              if (remainder != 0)
                {
                  off_t size_incr = ST_BLKSIZE (st) - remainder;
                  size += MIN (size_incr, OFF_T_MAX - size);
                }
            }
        }
      else
        {
          /* The behavior of lseek is unspecified, but in practice if
             it returns a positive number that's the size of this
             device.  */
          size = lseek (fd, 0, SEEK_END);
          if (size <= 0)
            {
               
              size = -1;
            }
        }
    }
  else if (S_ISREG (st.st_mode)
           && st.st_size < MIN (ST_BLKSIZE (st), size))
    i_size = st.st_size;

   
  genpattern (passarray, flags->n_iterations, s);

  rs = randint_get_source (s);

  while (true)
    {
      off_t pass_size;
      unsigned long int pn = n;

      if (i_size)
        {
          pass_size = i_size;
          i_size = 0;
          pn = 0;
        }
      else if (size)
        {
          pass_size = size;
          size = 0;
        }
       
      else
        break;

      for (i = 0; i < flags->n_iterations + flags->zero_fill; i++)
        {
          int err = 0;
          int type = i < flags->n_iterations ? passarray[i] : 0;

          err = dopass (fd, &st, qname, &pass_size, type, rs, i + 1, pn);

          if (err)
            {
              ok = false;
              if (err < 0)
                goto wipefd_out;
            }
        }
    }

   

  if (flags->remove_file && ftruncate (fd, 0) != 0
      && (S_ISREG (st.st_mode) || S_TYPEISSHM (&st)))
    {
      error (0, errno, _("%s: error truncating"), qname);
      ok = false;
      goto wipefd_out;
    }

wipefd_out:
  free (passarray);
  return ok;
}

 
static bool
wipefd (int fd, char const *qname, struct randint_source *s,
        struct Options const *flags)
{
  int fd_flags = fcntl (fd, F_GETFL);

  if (fd_flags < 0)
    {
      error (0, errno, _("%s: fcntl failed"), qname);
      return false;
    }
  if (fd_flags & O_APPEND)
    {
      error (0, 0, _("%s: cannot shred append-only file descriptor"), qname);
      return false;
    }
  return do_wipefd (fd, qname, s, flags);
}

 

 
static char const nameset[] =
"0123456789abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ_.";

 

static bool
incname (char *name, size_t len)
{
  while (len--)
    {
      char const *p = strchr (nameset, name[len]);

       

       
      if (p[1])
        {
          name[len] = p[1];
          return true;
        }

       
      name[len] = nameset[0];
    }

  return false;
}

 
static bool
wipename (char *oldname, char const *qoldname, struct Options const *flags)
{
  char *newname = xstrdup (oldname);
  char *base = last_component (newname);
  char *dir = dir_name (newname);
  char *qdir = xstrdup (quotef (dir));
  bool first = true;
  bool ok = true;
  int dir_fd = -1;

  if (flags->remove_file == remove_wipesync)
    dir_fd = open (dir, O_RDONLY | O_DIRECTORY | O_NOCTTY | O_NONBLOCK);

  if (flags->verbose)
    error (0, 0, _("%s: removing"), qoldname);

  if (flags->remove_file != remove_unlink)
    for (size_t len = base_len (base); len != 0; len--)
      {
        memset (base, nameset[0], len);
        base[len] = 0;
        bool rename_ok;
        while (! (rename_ok = (renameatu (AT_FDCWD, oldname, AT_FDCWD, newname,
                                          RENAME_NOREPLACE)
                               == 0))
               && errno == EEXIST && incname (base, len))
          continue;
        if (rename_ok)
          {
            if (0 <= dir_fd && dosync (dir_fd, qdir) != 0)
              ok = false;
            if (flags->verbose)
              {
                 
                char const *old = first ? qoldname : oldname;
                error (0, 0,
                       _("%s: renamed to %s"), old, newname);
                first = false;
              }
            memcpy (oldname + (base - newname), base, len + 1);
          }
      }

  if (unlink (oldname) != 0)
    {
      error (0, errno, _("%s: failed to remove"), qoldname);
      ok = false;
    }
  else if (flags->verbose)
    error (0, 0, _("%s: removed"), qoldname);
  if (0 <= dir_fd)
    {
      if (dosync (dir_fd, qdir) != 0)
        ok = false;
      if (close (dir_fd) != 0)
        {
          error (0, errno, _("%s: failed to close"), qdir);
          ok = false;
        }
    }
  free (newname);
  free (dir);
  free (qdir);
  return ok;
}

 
static bool
wipefile (char *name, char const *qname,
          struct randint_source *s, struct Options const *flags)
{
  bool ok;
  int fd;

  fd = open (name, O_WRONLY | O_NOCTTY | O_BINARY);
  if (fd < 0
      && (errno == EACCES && flags->force)
      && chmod (name, S_IWUSR) == 0)
    fd = open (name, O_WRONLY | O_NOCTTY | O_BINARY);
  if (fd < 0)
    {
      error (0, errno, _("%s: failed to open for writing"), qname);
      return false;
    }

  ok = do_wipefd (fd, qname, s, flags);
  if (close (fd) != 0)
    {
      error (0, errno, _("%s: failed to close"), qname);
      ok = false;
    }
  if (ok && flags->remove_file)
    ok = wipename (name, qname, flags);
  return ok;
}


 
static struct randint_source *randint_source;

 
static void
clear_random_data (void)
{
  randint_all_free (randint_source);
}


int
main (int argc, char **argv)
{
  bool ok = true;
  struct Options flags = { 0, };
  char **file;
  int n_files;
  int c;
  int i;
  char const *random_source = nullptr;

  initialize_main (&argc, &argv);
  set_program_name (argv[0]);
  setlocale (LC_ALL, "");
  bindtextdomain (PACKAGE, LOCALEDIR);
  textdomain (PACKAGE);

  atexit (close_stdout);

  flags.n_iterations = DEFAULT_PASSES;
  flags.size = -1;

  while ((c = getopt_long (argc, argv, "fn:s:uvxz", long_opts, nullptr)) != -1)
    {
      switch (c)
        {
        case 'f':
          flags.force = true;
          break;

        case 'n':
          flags.n_iterations = xdectoumax (optarg, 0,
                                           MIN (ULONG_MAX,
                                                SIZE_MAX / sizeof (int)), "",
                                           _("invalid number of passes"), 0);
          break;

        case RANDOM_SOURCE_OPTION:
          if (random_source && !STREQ (random_source, optarg))
            error (EXIT_FAILURE, 0, _("multiple random sources specified"));
          random_source = optarg;
          break;

        case 'u':
          if (optarg == nullptr)
            flags.remove_file = remove_wipesync;
          else
            flags.remove_file = XARGMATCH ("--remove", optarg,
                                           remove_args, remove_methods);
          break;

        case 's':
          flags.size = xnumtoumax (optarg, 0, 0, OFF_T_MAX, "cbBkKMGTPEZYRQ0",
                                   _("invalid file size"), 0);
          break;

        case 'v':
          flags.verbose = true;
          break;

        case 'x':
          flags.exact = true;
          break;

        case 'z':
          flags.zero_fill = true;
          break;

        case_GETOPT_HELP_CHAR;

        case_GETOPT_VERSION_CHAR (PROGRAM_NAME, AUTHORS);

        default:
          usage (EXIT_FAILURE);
        }
    }

  file = argv + optind;
  n_files = argc - optind;

  if (n_files == 0)
    {
      error (0, 0, _("missing file operand"));
      usage (EXIT_FAILURE);
    }

  randint_source = randint_all_new (random_source, SIZE_MAX);
  if (! randint_source)
    error (EXIT_FAILURE, errno, "%s",
           quotef (random_source ? random_source : "getrandom"));
  atexit (clear_random_data);

  for (i = 0; i < n_files; i++)
    {
      char *qname = xstrdup (quotef (file[i]));
      if (STREQ (file[i], "-"))
        {
          ok &= wipefd (STDOUT_FILENO, qname, randint_source, &flags);
        }
      else
        {
           
          ok &= wipefile (file[i], qname, randint_source, &flags);
        }
      free (qname);
    }

  return ok ? EXIT_SUCCESS : EXIT_FAILURE;
}
 
