 

#ifndef GLTEST_NAP_H
# define GLTEST_NAP_H

# include <limits.h>

# include <stdckdint.h>

 
# if defined _SCO_DS || (defined __SCO_VERSION__ || defined __sysv5__)   
#  include <unistd.h>
#  undef nap
#  define nap gl_nap
# endif

 
#define TEMPFILE BASE "nap.tmp"

 
static int nap_fd = -1;

 
static int
diff_timespec (struct timespec a, struct timespec b)
{
  time_t as = a.tv_sec;
  time_t bs = b.tv_sec;
  int ans = a.tv_nsec;
  int bns = b.tv_nsec;
  int sdiff;

  ASSERT (0 <= ans && ans < 2000000000);
  ASSERT (0 <= bns && bns < 2000000000);

  if (! (bs < as || (bs == as && bns < ans)))
    return 0;

  if (ckd_sub (&sdiff, as, bs)
      || ckd_mul (&sdiff, sdiff, 1000000000)
      || ckd_add (&sdiff, sdiff, ans - bns))
    return INT_MAX;

  return sdiff;
}

 
static void
nap_get_stat (struct stat *st, int do_write)
{
  if (do_write)
    {
      ASSERT (write (nap_fd, "\n", 1) == 1);
#if defined _WIN32 || defined __CYGWIN__
       
static bool
nap_works (int delay, struct stat old_st)
{
  struct stat st;
  struct timespec delay_spec;
  delay_spec.tv_sec = delay / 1000000000;
  delay_spec.tv_nsec = delay % 1000000000;
  ASSERT (nanosleep (&delay_spec, 0) == 0);
  nap_get_stat (&st, 1);

  if (diff_timespec (get_stat_mtime (&st), get_stat_mtime (&old_st)))
    return true;

  return false;
}

static void
clear_temp_file (void)
{
  if (0 <= nap_fd)
    {
      ASSERT (close (nap_fd) != -1);
      ASSERT (unlink (TEMPFILE) != -1);
    }
}

 
static void
nap (void)
{
  struct stat old_st;
  static int delay = 1;

  if (-1 == nap_fd)
    {
      atexit (clear_temp_file);
      ASSERT ((nap_fd = creat (TEMPFILE, 0600)) != -1);
      nap_get_stat (&old_st, 0);
    }
  else
    {
      ASSERT (0 <= nap_fd);
      nap_get_stat (&old_st, 1);
    }

  if (1 < delay)
    delay = delay / 2;   
  ASSERT (0 < delay);

  for (;;)
    {
      if (nap_works (delay, old_st))
        return;
      if (delay <= (2147483647 - 1) / 2)
        {
          delay = delay * 2 + 1;
          continue;
        }
      else
        break;
    }

   
  ASSERT (0);
}

#endif  
