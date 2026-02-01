 

 

static int
test_stat_func (int (*func) (char const *, struct stat *), bool print)
{
  struct stat st1;
  struct stat st2;
  char *cwd = getcwd (NULL, 0);

  ASSERT (cwd);
  ASSERT (func (".", &st1) == 0);
  ASSERT (func ("./", &st2) == 0);
#if !(defined _WIN32 && !defined __CYGWIN__ && !_GL_WINDOWS_STAT_INODES)
  ASSERT (SAME_INODE (st1, st2));
#endif
  ASSERT (func (cwd, &st2) == 0);
#if !(defined _WIN32 && !defined __CYGWIN__ && !_GL_WINDOWS_STAT_INODES)
  ASSERT (SAME_INODE (st1, st2));
#endif
  ASSERT (func ("/", &st1) == 0);
  ASSERT (func ("///", &st2) == 0);
#if !(defined _WIN32 && !defined __CYGWIN__ && !_GL_WINDOWS_STAT_INODES)
  ASSERT (SAME_INODE (st1, st2));
#endif

  errno = 0;
  ASSERT (func ("", &st1) == -1);
  ASSERT (errno == ENOENT);
  errno = 0;
  ASSERT (func ("nosuch", &st1) == -1);
  ASSERT (errno == ENOENT);
  errno = 0;
  ASSERT (func ("nosuch/", &st1) == -1);
  ASSERT (errno == ENOENT);

  ASSERT (close (creat (BASE "file", 0600)) == 0);
  ASSERT (func (BASE "file", &st1) == 0);
  errno = 0;
  ASSERT (func (BASE "file/", &st1) == -1);
  ASSERT (errno == ENOTDIR);

   
  if (symlink (".", BASE "link1") != 0)
    {
      ASSERT (unlink (BASE "file") == 0);
      if (print)
        fputs ("skipping test: symlinks not supported on this file system\n",
               stderr);
      return 77;
    }
  ASSERT (symlink (BASE "file", BASE "link2") == 0);
  ASSERT (symlink (BASE "nosuch", BASE "link3") == 0);
  ASSERT (symlink (BASE "link4", BASE "link4") == 0);

  ASSERT (func (BASE "link1/", &st1) == 0);
  ASSERT (S_ISDIR (st1.st_mode));

  errno = 0;
  ASSERT (func (BASE "link2/", &st1) == -1);
  ASSERT (errno == ENOTDIR);

  errno = 0;
  ASSERT (func (BASE "link3/", &st1) == -1);
  ASSERT (errno == ENOENT);

  errno = 0;
  ASSERT (func (BASE "link4/", &st1) == -1);
  ASSERT (errno == ELOOP);

   
  ASSERT (unlink (BASE "file") == 0);
  ASSERT (unlink (BASE "link1") == 0);
  ASSERT (unlink (BASE "link2") == 0);
  ASSERT (unlink (BASE "link3") == 0);
  ASSERT (unlink (BASE "link4") == 0);
  free (cwd);

  return 0;
}
