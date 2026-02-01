 
  {
    errno = 0;
    ASSERT (ftruncate (-1, 0) == -1);
    ASSERT (errno == EBADF);
  }
  {
    close (99);
    errno = 0;
    ASSERT (ftruncate (99, 0) == -1);
    ASSERT (errno == EBADF);
  }

   
  {
    int fd = open (filename, O_RDONLY);
    ASSERT (fd >= 0);
    errno = 0;
    ASSERT (ftruncate (fd, 0) == -1);
    ASSERT (errno == EBADF || errno == EINVAL
            || errno == EACCES  
           );
    close (fd);
  }

  return 0;
}
