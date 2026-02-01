 

 
#if __GNUC__ >= 13
# pragma GCC diagnostic ignored "-Wanalyzer-fd-leak"
#endif

 

#if __GLIBC__ && defined __always_inline
# define ALWAYS_INLINE __always_inline
#else
# define ALWAYS_INLINE
#endif

 

static ALWAYS_INLINE int
test_open (int (*func) (char const *, int, ...), bool print)
{
  int fd;

   
  unlink (BASE "file");
  unlink (BASE "e.exe");
  unlink (BASE "link");

   
  errno = 0;
  ASSERT (func ("nonexist.ent/", O_CREAT | O_RDONLY, 0600) == -1);
  ASSERT (errno == ENOTDIR || errno == EISDIR || errno == ENOENT
          || errno == EINVAL);

   
  fd = func (BASE "file", O_CREAT | O_RDONLY, 0600);
  ASSERT (0 <= fd);
  ASSERT (close (fd) == 0);

   
  fd = func (BASE "e.exe", O_CREAT | O_RDONLY, 0700);
  ASSERT (0 <= fd);
  ASSERT (close (fd) == 0);

   
  errno = 0;
  ASSERT (func (BASE "file/", O_RDONLY) == -1);
  ASSERT (errno == ENOTDIR || errno == EISDIR || errno == EINVAL);

   
  errno = 0;
  ASSERT (func (".", O_WRONLY) == -1);
  ASSERT (errno == EISDIR || errno == EACCES);

   
  fd = func ("/dev/null", O_RDONLY);
  ASSERT (0 <= fd);
  {
    char c;
    ASSERT (read (fd, &c, 1) == 0);
  }
  ASSERT (close (fd) == 0);
  fd = func ("/dev/null", O_WRONLY);
  ASSERT (0 <= fd);
  ASSERT (write (fd, "c", 1) == 1);
  ASSERT (close (fd) == 0);

   
  fd = func (BASE "file", O_NONBLOCK | O_RDONLY);
  ASSERT (0 <= fd);
  ASSERT (close (fd) == 0);

   
  if (O_CLOEXEC)
    {
       
      int i;

      for (i = 0; i < 2; i++)
        {
          int flags;

          fd = func (BASE "file", O_CLOEXEC | O_RDONLY);
          ASSERT (0 <= fd);
          flags = fcntl (fd, F_GETFD);
          ASSERT (flags >= 0);
          ASSERT ((flags & FD_CLOEXEC) != 0);
          ASSERT (close (fd) == 0);
        }
    }

   
  if (symlink (BASE "file", BASE "link") != 0)
    {
      ASSERT (unlink (BASE "file") == 0);
      if (print)
        fputs ("skipping test: symlinks not supported on this file system\n",
               stderr);
      return 77;
    }
  errno = 0;
  ASSERT (func (BASE "link/", O_RDONLY) == -1);
  ASSERT (errno == ENOTDIR);
  fd = func (BASE "link", O_RDONLY);
  ASSERT (0 <= fd);
  ASSERT (close (fd) == 0);

   
  ASSERT (unlink (BASE "file") == 0);
  ASSERT (unlink (BASE "e.exe") == 0);
  ASSERT (unlink (BASE "link") == 0);

  return 0;
}
