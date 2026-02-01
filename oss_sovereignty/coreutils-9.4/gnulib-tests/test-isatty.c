 
#if defined _WIN32 && ! defined __CYGWIN__
 
# define DEV_NULL "NUL"
#else
 
# define DEV_NULL "/dev/null"
#endif

int
main (void)
{
  const char *file = "test-isatty.txt";

   
  {
    errno = 0;
    ASSERT (isatty (-1) == 0);
    ASSERT (errno == EBADF
            || errno == 0  
           );
  }
  {
    close (99);
    errno = 0;
    ASSERT (isatty (99) == 0);
    ASSERT (errno == EBADF
            || errno == 0  
           );
  }

   
  {
    int fd;

    fd = open (file, O_WRONLY|O_CREAT|O_TRUNC, 0644);
    ASSERT (0 <= fd);
    ASSERT (write (fd, "hello", 5) == 5);
    ASSERT (close (fd) == 0);

    fd = open (file, O_RDONLY);
    ASSERT (0 <= fd);
    ASSERT (! isatty (fd));
    ASSERT (close (fd) == 0);
  }

   
  {
    int fd[2];

    ASSERT (pipe (fd) == 0);
    ASSERT (! isatty (fd[0]));
    ASSERT (! isatty (fd[1]));
    ASSERT (close (fd[0]) == 0);
    ASSERT (close (fd[1]) == 0);
  }

   
  {
    int fd;

    fd = open (DEV_NULL, O_RDONLY);
    ASSERT (0 <= fd);
    ASSERT (! isatty (fd));
    ASSERT (close (fd) == 0);
  }

  ASSERT (unlink (file) == 0);

  return 0;
}
