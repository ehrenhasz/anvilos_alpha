 

#include <config.h>

#include "stat-time.h"

#include <fcntl.h>
#include <signal.h>
#include <stdio.h>
#include <sys/stat.h>
#include <unistd.h>
#include <time.h>

#include "macros.h"

#define BASE "test-stat-time.t"
#include "nap.h"

enum { NFILES = 4 };

static char filename_stamp1[50];
static char filename_testfile[50];
static char filename_stamp2[50];
static char filename_stamp3[50];

 
  chmod (filename, 0600);
  return unlink (filename);
}

static void
cleanup (int sig)
{
   
  force_unlink (filename_stamp1);
  force_unlink (filename_testfile);
  force_unlink (filename_stamp2);
  force_unlink (filename_stamp3);

  if (sig != 0)
    _exit (1);
}

static int
open_file (const char *filename, int flags)
{
  int fd = open (filename, flags | O_WRONLY, 0500);
  if (fd >= 0)
    {
      close (fd);
      return 1;
    }
  else
    {
      return 0;
    }
}

static void
create_file (const char *filename)
{
  ASSERT (open_file (filename, O_CREAT | O_EXCL));
}

static void
do_stat (const char *filename, struct stat *p)
{
  ASSERT (stat (filename, p) == 0);
}

static void
prepare_test (struct stat *statinfo, struct timespec *modtimes)
{
  int i;

  create_file (filename_stamp1);
  nap ();
  create_file (filename_testfile);
  nap ();
  create_file (filename_stamp2);
  nap ();
  ASSERT (chmod (filename_testfile, 0400) == 0);
  nap ();
  create_file (filename_stamp3);

  do_stat (filename_stamp1,   &statinfo[0]);
  do_stat (filename_testfile, &statinfo[1]);
  do_stat (filename_stamp2,   &statinfo[2]);
  do_stat (filename_stamp3,   &statinfo[3]);

   
  for (i = 0; i < NFILES; ++i)
    {
      modtimes[i] = get_stat_mtime (&statinfo[i]);
    }
}

static void
test_mtime (const struct stat *statinfo, struct timespec *modtimes)
{
  int i;

   
   
  ASSERT (statinfo[0].st_mtime < statinfo[2].st_mtime
          || (statinfo[0].st_mtime == statinfo[2].st_mtime
              && (get_stat_mtime_ns (&statinfo[0])
                  < get_stat_mtime_ns (&statinfo[2]))));
   
  ASSERT (statinfo[2].st_mtime < statinfo[3].st_mtime
          || (statinfo[2].st_mtime == statinfo[3].st_mtime
              && (get_stat_mtime_ns (&statinfo[2])
                  < get_stat_mtime_ns (&statinfo[3]))));

   
   
  ASSERT (modtimes[0].tv_sec < modtimes[2].tv_sec
          || (modtimes[0].tv_sec == modtimes[2].tv_sec
              && modtimes[0].tv_nsec < modtimes[2].tv_nsec));
   
  ASSERT (modtimes[2].tv_sec < modtimes[3].tv_sec
          || (modtimes[2].tv_sec == modtimes[3].tv_sec
              && modtimes[2].tv_nsec < modtimes[3].tv_nsec));

   
  for (i = 0; i < NFILES; ++i)
    {
      struct timespec ts;
      ts = get_stat_mtime (&statinfo[i]);
      ASSERT (ts.tv_sec == statinfo[i].st_mtime);
    }
}

#if defined _WIN32 && !defined __CYGWIN__
 
# define test_ctime(ignored) ((void) 0)
#else
static void
test_ctime (const struct stat *statinfo)
{
   
  if (statinfo[0].st_mtime != statinfo[0].st_ctime)
    return;

   
  ASSERT (statinfo[2].st_mtime < statinfo[1].st_ctime
          || (statinfo[2].st_mtime == statinfo[1].st_ctime
              && (get_stat_mtime_ns (&statinfo[2])
                  < get_stat_ctime_ns (&statinfo[1]))));
}
#endif

static void
test_birthtime (const struct stat *statinfo,
                const struct timespec *modtimes,
                struct timespec *birthtimes)
{
  int i;

   
  for (i = 0; i < NFILES; ++i)
    {
      birthtimes[i] = get_stat_birthtime (&statinfo[i]);
      if (birthtimes[i].tv_nsec < 0)
        return;
    }

   
  ASSERT (modtimes[0].tv_sec < birthtimes[1].tv_sec
          || (modtimes[0].tv_sec == birthtimes[1].tv_sec
              && modtimes[0].tv_nsec < birthtimes[1].tv_nsec));
   
  ASSERT (birthtimes[1].tv_sec < modtimes[2].tv_sec
          || (birthtimes[1].tv_sec == modtimes[2].tv_sec
              && birthtimes[1].tv_nsec < modtimes[2].tv_nsec));
}

int
main (void)
{
  struct stat statinfo[NFILES];
  struct timespec modtimes[NFILES];
  struct timespec birthtimes[NFILES];

  initialize_filenames ();

#ifdef SIGHUP
  signal (SIGHUP, cleanup);
#endif
#ifdef SIGINT
  signal (SIGINT, cleanup);
#endif
#ifdef SIGQUIT
  signal (SIGQUIT, cleanup);
#endif
#ifdef SIGTERM
  signal (SIGTERM, cleanup);
#endif

  cleanup (0);
  prepare_test (statinfo, modtimes);
  test_mtime (statinfo, modtimes);
  test_ctime (statinfo);
  test_birthtime (statinfo, modtimes, birthtimes);

  cleanup (0);
  return 0;
}
