 

static int
test_link (int (*func) (char const *, char const *), bool print)
{
  int fd;
  int ret;

   
  fd = open (BASE "a", O_CREAT | O_EXCL | O_WRONLY, 0600);
  ASSERT (0 <= fd);
  ASSERT (write (fd, "hello", 5) == 5);
  ASSERT (close (fd) == 0);

   
  ret = func (BASE "a", BASE "b");
  if (!ret)
    {
      struct stat st;
      ASSERT (stat (BASE "b", &st) == 0);
      if (st.st_ino && st.st_nlink != 2)
        {
          ASSERT (unlink (BASE "b") == 0);
          errno = EPERM;
          ret = -1;
        }
    }
  if (ret == -1)
    {
       
      switch (errno)
        {
        case EPERM:
        case EOPNOTSUPP:
        #if defined __ANDROID__
        case EACCES:
        #endif
          if (print)
            fputs ("skipping test: "
                   "hard links not supported on this file system\n",
                   stderr);
          ASSERT (unlink (BASE "a") == 0);
          return 77;
        default:
          perror ("link");
          return 1;
        }
    }
  ASSERT (ret == 0);

   
  fd = open (BASE "b", O_APPEND | O_WRONLY);
  ASSERT (0 <= fd);
  ASSERT (write (fd, "world", 5) == 5);
  ASSERT (close (fd) == 0);
  {
    char buf[11] = { 0 };
    fd = open (BASE "a", O_RDONLY);
    ASSERT (0 <= fd);
    ASSERT (read (fd, buf, 10) == 10);
    ASSERT (strcmp (buf, "helloworld") == 0);
    ASSERT (close (fd) == 0);
    ASSERT (unlink (BASE "b") == 0);
    fd = open (BASE "a", O_RDONLY);
    ASSERT (0 <= fd);
    ASSERT (read (fd, buf, 10) == 10);
    ASSERT (strcmp (buf, "helloworld") == 0);
    ASSERT (close (fd) == 0);
  }

   
  ASSERT (mkdir (BASE "d", 0700) == 0);
  errno = 0;
  ASSERT (func (BASE "a", ".") == -1);
  ASSERT (errno == EEXIST || errno == EINVAL);
  errno = 0;
  ASSERT (func (BASE "a", BASE "a") == -1);
  ASSERT (errno == EEXIST);
  ASSERT (func (BASE "a", BASE "b") == 0);
  errno = 0;
  ASSERT (func (BASE "a", BASE "b") == -1);
  ASSERT (errno == EEXIST);
  errno = 0;
  ASSERT (func (BASE "a", BASE "d") == -1);
  ASSERT (errno == EEXIST);
  errno = 0;
  ASSERT (func (BASE "c", BASE "e") == -1);
  ASSERT (errno == ENOENT);
  errno = 0;
  ASSERT (func (BASE "a", BASE "c/.") == -1);
  ASSERT (errno == ENOENT);
  errno = 0;
  ASSERT (func (BASE "a/", BASE "c") == -1);
  ASSERT (errno == ENOTDIR || errno == EINVAL);
  errno = 0;
  ASSERT (func (BASE "a", BASE "c/") == -1);
  ASSERT (errno == ENOTDIR || errno == ENOENT || errno == EINVAL);

   
  {
    int result;
    errno = 0;
    result = func (BASE "d", BASE "c");
    if (result == 0)
      {
         
        ASSERT (unlink (BASE "c") == 0);
      }
    else
      {
         
        ASSERT (errno == EPERM || errno == EACCES || errno == EISDIR);
        errno = 0;
        ASSERT (func (BASE "d/.", BASE "c") == -1);
        ASSERT (errno == EPERM || errno == EACCES || errno == EISDIR
                || errno == EINVAL);
        errno = 0;
        ASSERT (func (BASE "d/.//", BASE "c") == -1);
        ASSERT (errno == EPERM || errno == EACCES || errno == EISDIR
                || errno == EINVAL);
      }
  }
  ASSERT (unlink (BASE "a") == 0);
  errno = 0;
  ASSERT (unlink (BASE "c") == -1);
  ASSERT (errno == ENOENT);
  ASSERT (rmdir (BASE "d") == 0);

   
  if (symlink (BASE "a", BASE "link") != 0)
    {
      ASSERT (unlink (BASE "b") == 0);
      if (print)
        fputs ("skipping test: symlinks not supported on this file system\n",
               stderr);
      return 77;
    }
  errno = 0;
  ASSERT (func (BASE "b", BASE "link/") == -1);
  ASSERT (errno == ENOTDIR || errno == ENOENT || errno == EEXIST
          || errno == EINVAL);
  errno = 0;
  ASSERT (func (BASE "b", BASE "link") == -1);
  ASSERT (errno == EEXIST);
  ASSERT (rename (BASE "b", BASE "a") == 0);
  errno = 0;
  ASSERT (func (BASE "link/", BASE "b") == -1);
  ASSERT (errno == ENOTDIR || errno == EEXIST || errno == EINVAL);

   
  ASSERT (unlink (BASE "a") == 0);
  ASSERT (unlink (BASE "link") == 0);

  return 0;
}
