 
static int
test_utime (bool print)
{
  struct stat st1;
  struct stat st2;

  ASSERT (close (creat (BASE "file", 0600)) == 0);
  ASSERT (stat (BASE "file", &st1) == 0);
  nap ();
  ASSERT (utime (BASE "file", NULL) == 0);
  ASSERT (stat (BASE "file", &st2) == 0);
  ASSERT (0 <= utimecmp (BASE "file", &st2, &st1, UTIMECMP_TRUNCATE_SOURCE));
  if (check_ctime)
    ASSERT (ctime_compare (&st1, &st2) < 0);
  {
     
    struct utimbuf ts;
    ts.actime = ts.modtime = time (NULL);
    ASSERT (utime (BASE "file", &ts) == 0);
    ASSERT (stat (BASE "file", &st1) == 0);
    nap ();
  }

   
  errno = 0;
  ASSERT (utime ("no_such", NULL) == -1);
  ASSERT (errno == ENOENT);
  errno = 0;
  ASSERT (utime ("no_such/", NULL) == -1);
  ASSERT (errno == ENOENT || errno == ENOTDIR);
  errno = 0;
  ASSERT (utime ("", NULL) == -1);
  ASSERT (errno == ENOENT);
  {
    struct utimbuf ts;
    ts.actime = ts.modtime = Y2K;
    errno = 0;
    ASSERT (utime (BASE "file/", &ts) == -1);
    ASSERT (errno == ENOTDIR || errno == EINVAL);
  }
  ASSERT (stat (BASE "file", &st2) == 0);
  ASSERT (st1.st_atime == st2.st_atime);
  ASSERT (get_stat_atime_ns (&st1) == get_stat_atime_ns (&st2));
  ASSERT (utimecmp (BASE "file", &st1, &st2, 0) == 0);

   
  {
    struct utimbuf ts;
    ts.actime = ts.modtime = Y2K;
    ASSERT (utime (BASE "file", &ts) == 0);
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
  ASSERT (utime (BASE "link/", NULL) == -1);
  ASSERT (errno == ENOTDIR);
  {
    struct utimbuf ts;
    ts.actime = ts.modtime = Y2K;
    ASSERT (utime (BASE "link", &ts) == 0);
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

int
main (void)
{
  int result1;  

   
  ignore_value (system ("rm -rf " BASE "*"));

  result1 = test_utime (true);
  return result1;
}
