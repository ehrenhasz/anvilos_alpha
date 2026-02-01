 

static int
test_mkdir (int (*func) (char const *, mode_t), bool print)
{
   
  ASSERT (close (creat (BASE "file", 0600)) == 0);
  errno = 0;
  ASSERT (func (BASE "file", 0700) == -1);
  ASSERT (errno == EEXIST);
  errno = 0;
  ASSERT (func (BASE "file/", 0700) == -1);
  ASSERT (errno == ENOTDIR || errno == EEXIST);
  errno = 0;
  ASSERT (func (BASE "file/dir", 0700) == -1);
  ASSERT (errno == ENOTDIR || errno == ENOENT || errno == EOPNOTSUPP);
  ASSERT (unlink (BASE "file") == 0);
  errno = 0;
  ASSERT (func ("", 0700) == -1);
  ASSERT (errno == ENOENT);
  errno = 0;
  ASSERT (func (BASE "dir/sub", 0700) == -1);
  ASSERT (errno == ENOENT);
  errno = 0;
  ASSERT (func (BASE "dir/.", 0700) == -1);
  ASSERT (errno == ENOENT);
  errno = 0;
  ASSERT (func (BASE "dir/.//", 0700) == -1);
  ASSERT (errno == ENOENT);

   
  ASSERT (func (BASE "dir", 0700) == 0);
  errno = 0;
  ASSERT (func (BASE "dir", 0700) == -1);
  ASSERT (errno == EEXIST);
  ASSERT (rmdir (BASE "dir") == 0);
  ASSERT (func (BASE "dir/", 0700) == 0);
  errno = 0;
  ASSERT (func (BASE "dir/", 0700) == -1);
  ASSERT (errno == EEXIST);
  ASSERT (rmdir (BASE "dir") == 0);

   
  if (symlink (BASE "dir", BASE "link"))
    {
      if (print)
        fputs ("skipping test: symlinks not supported on this file system\n",
               stderr);
      return 77;
    }
  errno = 0;
  ASSERT (func (BASE "link", 0700) == -1);
  ASSERT (errno == EEXIST);
  {
    int result;
    errno = 0;
    result = func (BASE "link/", 0700);
    if (!result)
      ASSERT (rmdir (BASE "dir") == 0);
    else
      {
        ASSERT (result == -1);
        ASSERT (errno == EEXIST);
        errno = 0;
        ASSERT (rmdir (BASE "dir") == -1);
        ASSERT (errno == ENOENT);
      }
  }
  errno = 0;
  ASSERT (func (BASE "link/.", 0700) == -1);
  ASSERT (errno == ENOENT);
  ASSERT (unlink (BASE "link") == 0);

  return 0;
}
