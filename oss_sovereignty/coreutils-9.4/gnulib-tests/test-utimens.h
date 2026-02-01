 
static int
test_utimens (int (*func) (char const *, struct timespec const *), bool print)
{
  struct stat st1;
  struct stat st2;

  ASSERT (close (creat (BASE "file", 0600)) == 0);
   
  ASSERT (stat (BASE "file", &st1) == 0);
  nap ();
  ASSERT (func (BASE "file", NULL) == 0);
  ASSERT (stat (BASE "file", &st2) == 0);
  ASSERT (0 <= utimecmp (BASE "file", &st2, &st1, UTIMECMP_TRUNCATE_SOURCE));
  if (check_ctime)
    ASSERT (ctime_compare (&st1, &st2) < 0);
  {
     
    struct timespec ts[2];
    gettime (&ts[0]);
    ts[1] = ts[0];
    ASSERT (func (BASE "file", ts) == 0);
    ASSERT (stat (BASE "file", &st1) == 0);
    nap ();
  }

   
  errno = 0;
  ASSERT (func ("no_such", NULL) == -1);
  ASSERT (errno == ENOENT);
  errno = 0;
  ASSERT (func ("no_such/", NULL) == -1);
  ASSERT (errno == ENOENT || errno == ENOTDIR);
  errno = 0;
  ASSERT (func ("", NULL) == -1);
  ASSERT (errno == ENOENT);
  {
    struct timespec ts[2];
    ts[0].tv_sec = Y2K;
    ts[0].tv_nsec = UTIME_BOGUS_POS;
    ts[1].tv_sec = Y2K;
    ts[1].tv_nsec = 0;
    errno = 0;
    ASSERT (func (BASE "file", ts) == -1);
    ASSERT (errno == EINVAL);
  }
  {
    struct timespec ts[2];
    ts[0].tv_sec = Y2K;
    ts[0].tv_nsec = 0;
    ts[1].tv_sec = Y2K;
    ts[1].tv_nsec = UTIME_BOGUS_NEG;
    errno = 0;
    ASSERT (func (BASE "file", ts) == -1);
    ASSERT (errno == EINVAL);
  }
  {
    struct timespec ts[2];
    ts[0].tv_sec = Y2K;
    ts[0].tv_nsec = 0;
    ts[1] = ts[0];
    errno = 0;
    ASSERT (func (BASE "file/", ts) == -1);
    ASSERT (errno == ENOTDIR || errno == EINVAL);
  }
  ASSERT (stat (BASE "file", &st2) == 0);
  ASSERT (st1.st_atime == st2.st_atime);
  ASSERT (get_stat_atime_ns (&st1) == get_stat_atime_ns (&st2));
  ASSERT (utimecmp (BASE "file", &st1, &st2, 0) == 0);

   
  {
    struct timespec ts[2];
    ts[0].tv_sec = Y2K;
    ts[0].tv_nsec = BILLION / 2 - 1;
    ts[1].tv_sec = Y2K;
    ts[1].tv_nsec = BILLION - 1;
    ASSERT (func (BASE "file", ts) == 0);
    ASSERT (stat (BASE "file", &st2) == 0);
    ASSERT (st2.st_atime == Y2K);
    ASSERT (0 <= get_stat_atime_ns (&st2));
    ASSERT (get_stat_atime_ns (&st2) < BILLION / 2);
    ASSERT (st2.st_mtime == Y2K);
    ASSERT (0 <= get_stat_mtime_ns (&st2));
    ASSERT (get_stat_mtime_ns (&st2) < BILLION);
    if (check_ctime)
      ASSERT (ctime_compare (&st1, &st2) < 0);
  }

   
  {
    struct stat st3;
    struct timespec ts[2];
    ts[0].tv_sec = BILLION;
    ts[0].tv_nsec = UTIME_OMIT;
    ts[1].tv_sec = 0;
    ts[1].tv_nsec = UTIME_NOW;
    nap ();
    ASSERT (func (BASE "file", ts) == 0);
    ASSERT (stat (BASE "file", &st3) == 0);
    ASSERT (st3.st_atime == Y2K);
    ASSERT (0 <= get_stat_atime_ns (&st3));
    ASSERT (get_stat_atime_ns (&st3) < BILLION / 2);
     
    ASSERT (0 <= utimecmp (BASE "file", &st3, &st1, UTIMECMP_TRUNCATE_SOURCE));
    if (check_ctime)
      ASSERT (ctime_compare (&st2, &st3) < 0);
    nap ();
    ts[0].tv_nsec = 0;
    ts[1].tv_nsec = UTIME_OMIT;
    ASSERT (func (BASE "file", ts) == 0);
    ASSERT (stat (BASE "file", &st2) == 0);
    ASSERT (st2.st_atime == BILLION);
    ASSERT (get_stat_atime_ns (&st2) == 0);
    ASSERT (st3.st_mtime == st2.st_mtime);
    ASSERT (get_stat_mtime_ns (&st3) == get_stat_mtime_ns (&st2));
    if (check_ctime > 0)
      ASSERT (ctime_compare (&st3, &st2) < 0);
  }

   
  if (symlink (BASE "file", BASE "link"))
    {
      ASSERT (unlink (BASE "file") == 0);
      if (print)
        fputs ("skipping test: symlinks not supported on this file system\n",
               stderr);
      return 77;
    }
  ASSERT (lstat (BASE "link", &st1) == 0);
  ASSERT (st1.st_mtime != Y2K);
  errno = 0;
  ASSERT (func (BASE "link/", NULL) == -1);
  ASSERT (errno == ENOTDIR);
  {
    struct timespec ts[2];
    ts[0].tv_sec = Y2K;
    ts[0].tv_nsec = 0;
    ts[1] = ts[0];
    ASSERT (func (BASE "link", ts) == 0);
    ASSERT (lstat (BASE "link", &st2) == 0);
     
    ASSERT (st1.st_mtime == st2.st_mtime);
    ASSERT (stat (BASE "link", &st2) == 0);
    ASSERT (st2.st_mtime == Y2K);
    ASSERT (get_stat_mtime_ns (&st2) == 0);
  }

   
  ASSERT (unlink (BASE "link") == 0);
  ASSERT (unlink (BASE "file") == 0);
  return 0;
}
