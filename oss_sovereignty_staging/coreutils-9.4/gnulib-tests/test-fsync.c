 
  for (fd = 0; fd < 2; fd++)
    if (fsync (fd) != 0)
      {
        ASSERT (errno == EINVAL  
                || errno == ENOTSUP  
                || errno == EBADF  
                || errno == EIO  
                );
      }

   
  {
    errno = 0;
    ASSERT (fsync (-1) == -1);
    ASSERT (errno == EBADF);
  }
  {
    close (99);
    errno = 0;
    ASSERT (fsync (99) == -1);
    ASSERT (errno == EBADF);
  }

  fd = open (file, O_WRONLY|O_CREAT|O_TRUNC, 0644);
  ASSERT (0 <= fd);
  ASSERT (write (fd, "hello", 5) == 5);
  ASSERT (fsync (fd) == 0);
  ASSERT (close (fd) == 0);

   
#if !(defined _AIX || defined __CYGWIN__)
  fd = open (file, O_RDONLY);
  ASSERT (0 <= fd);
  {
    char buf[1];
    ASSERT (read (fd, buf, sizeof buf) == sizeof buf);
  }
  ASSERT (fsync (fd) == 0);
  ASSERT (close (fd) == 0);
#endif

  ASSERT (unlink (file) == 0);

  return 0;
}
