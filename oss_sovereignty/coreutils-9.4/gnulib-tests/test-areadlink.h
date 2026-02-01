 

 

static int
test_areadlink (char * (*func) (char const *, size_t), bool print)
{
   
  errno = 0;
  ASSERT (func ("no_such", 1) == NULL);
  ASSERT (errno == ENOENT);
  errno = 0;
  ASSERT (func ("no_such/", 1) == NULL);
  ASSERT (errno == ENOENT);
  errno = 0;
  ASSERT (func ("", 1) == NULL);
  ASSERT (errno == ENOENT || errno == EINVAL);
  errno = 0;
  ASSERT (func (".", 1) == NULL);
  ASSERT (errno == EINVAL);
  errno = 0;
  ASSERT (func ("./", 1) == NULL);
  ASSERT (errno == EINVAL);
  ASSERT (close (creat (BASE "file", 0600)) == 0);
  errno = 0;
  ASSERT (func (BASE "file", 1) == NULL);
  ASSERT (errno == EINVAL);
  errno = 0;
  ASSERT (func (BASE "file/", 1) == NULL);
  ASSERT (errno == ENOTDIR || errno == EINVAL);  
  ASSERT (unlink (BASE "file") == 0);

   
  if (symlink (BASE "dir", BASE "link"))
    {
      if (print)
        fputs ("skipping test: symlinks not supported on this file system\n",
               stderr);
      return 77;
    }
  ASSERT (mkdir (BASE "dir", 0700) == 0);
  errno = 0;
  ASSERT (func (BASE "link/", 1) == NULL);
  ASSERT (errno == EINVAL);
  {
     
    char *buf = func (BASE "link", 1);
    ASSERT (buf);
    ASSERT (strcmp (buf, BASE "dir") == 0);
    free (buf);
     
    buf = func (BASE "link", 10000000);
    ASSERT (buf);
    ASSERT (strcmp (buf, BASE "dir") == 0);
    free (buf);
  }
  ASSERT (rmdir (BASE "dir") == 0);
  ASSERT (unlink (BASE "link") == 0);

  return 0;
}
