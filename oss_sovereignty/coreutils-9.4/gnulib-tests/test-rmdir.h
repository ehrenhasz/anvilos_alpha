 

 

static int
test_rmdir_func (int (*func) (char const *name), bool print)
{
   
  ASSERT (mkdir (BASE "dir", 0700) == 0);
  ASSERT (close (creat (BASE "dir/file", 0600)) == 0);

   
  errno = 0;
  ASSERT (func ("") == -1);
  ASSERT (errno == ENOENT);
  errno = 0;
  ASSERT (func (BASE "nosuch") == -1);
  ASSERT (errno == ENOENT);
  errno = 0;
  ASSERT (func (BASE "nosuch/") == -1);
  ASSERT (errno == ENOENT);
  errno = 0;
  ASSERT (func (".") == -1);
  ASSERT (errno == EINVAL || errno == EBUSY);
   
  ASSERT (func ("..") == -1);
  ASSERT (func ("/") == -1);
  ASSERT (func ("///") == -1);
  errno = 0;
  ASSERT (func (BASE "dir/file/") == -1);
  ASSERT (errno == ENOTDIR);

   
  errno = 0;
  ASSERT (func (BASE "dir") == -1);
  ASSERT (errno == EEXIST || errno == ENOTEMPTY);

   
  errno = 0;
  ASSERT (func (BASE "dir/file") == -1);
  ASSERT (errno == ENOTDIR);

   
  ASSERT (unlink (BASE "dir/file") == 0);
  errno = 0;
  ASSERT (func (BASE "dir/.//") == -1);
  ASSERT (errno == EINVAL || errno == EBUSY || errno == EEXIST
          || errno == ENOTEMPTY);
  ASSERT (func (BASE "dir") == 0);

   
  if (symlink (BASE "dir", BASE "link") != 0)
    {
      if (print)
        fputs ("skipping test: symlinks not supported on this file system\n",
               stderr);
      return 77;
    }
  ASSERT (mkdir (BASE "dir", 0700) == 0);
  errno = 0;
  if (func (BASE "link/") == 0)
    {
      struct stat st;
      errno = 0;
      ASSERT (stat (BASE "link", &st) == -1);
      ASSERT (errno == ENOENT);
    }
  else
    {
      ASSERT (errno == ENOTDIR);
      ASSERT (func (BASE "dir") == 0);
    }
  ASSERT (unlink (BASE "link") == 0);

  return 0;
}
