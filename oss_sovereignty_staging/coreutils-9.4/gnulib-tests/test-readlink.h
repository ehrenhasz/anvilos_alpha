 

 

static int
test_readlink (ssize_t (*func) (char const *, char *, size_t), bool print)
{
  char buf[80];

   
  memset (buf, 0xff, sizeof buf);
  errno = 0;
  ASSERT (func ("no_such", buf, sizeof buf) == -1);
  ASSERT (errno == ENOENT);
  errno = 0;
  ASSERT (func ("no_such/", buf, sizeof buf) == -1);
  ASSERT (errno == ENOENT);
  errno = 0;
  ASSERT (func ("", buf, sizeof buf) == -1);
  ASSERT (errno == ENOENT || errno == EINVAL);
  errno = 0;
  ASSERT (func (".", buf, sizeof buf) == -1);
  ASSERT (errno == EINVAL);
  errno = 0;
  ASSERT (func ("./", buf, sizeof buf) == -1);
  ASSERT (errno == EINVAL);
  ASSERT (close (creat (BASE "file", 0600)) == 0);
  errno = 0;
  ASSERT (func (BASE "file", buf, sizeof buf) == -1);
  ASSERT (errno == EINVAL);
  errno = 0;
  ASSERT (func (BASE "file/", buf, sizeof buf) == -1);
  ASSERT (errno == ENOTDIR || errno == EINVAL);  

   
  if (symlink (BASE "dir", BASE "link"))
    {
      ASSERT (unlink (BASE "file") == 0);
      if (print)
        fputs ("skipping test: symlinks not supported on this file system\n",
               stderr);
      return 77;
    }
  ASSERT (mkdir (BASE "dir", 0700) == 0);
  errno = 0;
  ASSERT (func (BASE "link/", buf, sizeof buf) == -1);
  ASSERT (errno == EINVAL);
  ASSERT (symlink (BASE "link", BASE "link2") == 0);
  errno = 0;
  ASSERT (func (BASE "link2/", buf, sizeof buf) == -1);
  ASSERT (errno == EINVAL);
  ASSERT (unlink (BASE "link2") == 0);
  ASSERT (symlink (BASE "file", BASE "link2") == 0);
  errno = 0;
  ASSERT (func (BASE "link2/", buf, sizeof buf) == -1);
  ASSERT (errno == ENOTDIR || errno == EINVAL);  
  ASSERT (unlink (BASE "file") == 0);
  ASSERT (unlink (BASE "link2") == 0);
  {
     
    int i;
    for (i = 0; i < sizeof buf; i++)
      ASSERT (buf[i] == (char) 0xff);
  }
  {
    size_t len = strlen (BASE "dir");
     
    ssize_t result;
    errno = 0;
    result = readlink (BASE "link", buf, 1);
    if (result == -1)
      {
        ASSERT (errno == ERANGE);
        ASSERT (buf[0] == (char) 0xff);
      }
    else
      {
        ASSERT (result == 1);
        ASSERT (buf[0] == BASE[0]);
      }
    ASSERT (buf[1] == (char) 0xff);
    ASSERT (func (BASE "link", buf, len) == len);
    ASSERT (strncmp (buf, BASE "dir", len) == 0);
    ASSERT (buf[len] == (char) 0xff);
    ASSERT (func (BASE "link", buf, sizeof buf) == len);
    ASSERT (strncmp (buf, BASE "dir", len) == 0);
     
    ASSERT (buf[len] == '\0' || buf[len] == (char) 0xff);
  }
  ASSERT (rmdir (BASE "dir") == 0);
  ASSERT (unlink (BASE "link") == 0);

  return 0;
}
