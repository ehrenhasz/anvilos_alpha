 

 

static int
test_unlink_func (int (*func) (char const *name), bool print)
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
   
  if (cannot_unlink_dir ())
    {
      ASSERT (func (".") == -1);
      ASSERT (func ("..") == -1);
      ASSERT (func ("/") == -1);
      ASSERT (func (BASE "dir") == -1);
      ASSERT (mkdir (BASE "dir1", 0700) == 0);
      ASSERT (func (BASE "dir1") == -1);
      ASSERT (rmdir (BASE "dir1") == 0);
    }
  errno = 0;
  ASSERT (func (BASE "dir/file/") == -1);
  ASSERT (errno == ENOTDIR);

   
  if (symlink (BASE "dir", BASE "link") != 0)
    {
      ASSERT (func (BASE "dir/file") == 0);
      ASSERT (rmdir (BASE "dir") == 0);
      if (print)
        fputs ("skipping test: symlinks not supported on this file system\n",
               stderr);
      return 77;
    }
  if (cannot_unlink_dir ())
    ASSERT (func (BASE "link/") == -1);
  ASSERT (func (BASE "link") == 0);
  ASSERT (symlink (BASE "dir/file", BASE "link") == 0);
  errno = 0;
  ASSERT (func (BASE "link/") == -1);
  ASSERT (errno == ENOTDIR);
   
  ASSERT (func (BASE "link") == 0);
  ASSERT (func (BASE "dir/file") == 0);
  ASSERT (rmdir (BASE "dir") == 0);

  return 0;
}
