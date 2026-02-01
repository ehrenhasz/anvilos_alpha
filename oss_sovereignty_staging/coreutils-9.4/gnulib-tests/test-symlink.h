 

 

static int
test_symlink (int (*func) (char const *, char const *), bool print)
{
  if (func ("nowhere", BASE "link1"))
    {
      if (print)
        fputs ("skipping test: symlinks not supported on this file system\n",
               stderr);
      return 77;
    }

   
  {
    int status;
    errno = 0;
    status = func ("", BASE "link2");
    if (status == -1)
      ASSERT (errno == ENOENT || errno == EINVAL);
    else
      {
        ASSERT (status == 0);
        ASSERT (unlink (BASE "link2") == 0);
      }
  }

   
  errno = 0;
  ASSERT (func ("nowhere", "") == -1);
  ASSERT (errno == ENOENT);
  errno = 0;
  ASSERT (func ("nowhere", ".") == -1);
  ASSERT (errno == EEXIST || errno == EINVAL);
  errno = 0;
  ASSERT (func ("somewhere", BASE "link1") == -1);
  ASSERT (errno == EEXIST);
  errno = 0;
  ASSERT (func ("nowhere", BASE "link2/") == -1);
  ASSERT (errno == ENOTDIR || errno == ENOENT);
  ASSERT (mkdir (BASE "dir", 0700) == 0);
  errno = 0;
  ASSERT (func ("nowhere", BASE "dir") == -1);
  ASSERT (errno == EEXIST);
  errno = 0;
  ASSERT (func ("nowhere", BASE "dir/") == -1);
  ASSERT (errno == EEXIST || errno == EINVAL
          || errno == ENOENT  );
  ASSERT (close (creat (BASE "file", 0600)) == 0);
  errno = 0;
  ASSERT (func ("nowhere", BASE "file") == -1);
  ASSERT (errno == EEXIST);
  errno = 0;
  ASSERT (func ("nowhere", BASE "file/") == -1);
  ASSERT (errno == EEXIST || errno == ENOTDIR || errno == ENOENT);

   
  ASSERT (unlink (BASE "link1") == 0);
  ASSERT (func (BASE "link2", BASE "link1") == 0);
  errno = 0;
  ASSERT (func (BASE "nowhere", BASE "link1/") == -1);
  ASSERT (errno == EEXIST || errno == ENOTDIR || errno == ENOENT);
  errno = 0;
  ASSERT (unlink (BASE "link2") == -1);
  ASSERT (errno == ENOENT);

   
  ASSERT (rmdir (BASE "dir") == 0);
  ASSERT (unlink (BASE "file") == 0);
  ASSERT (unlink (BASE "link1") == 0);

  return 0;
}
