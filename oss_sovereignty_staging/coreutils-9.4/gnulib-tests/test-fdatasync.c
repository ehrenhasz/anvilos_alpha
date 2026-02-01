 
  for (fd = 0; fd < 2; fd++)
    if (fdatasync (fd) != 0)
      {
        ASSERT (errno == EINVAL  
                || errno == ENOTSUP  
                || errno == EBADF  
                || errno == EIO  
                );
      }

   
  {
    errno = 0;
    ASSERT (fdatasync (-1) == -1);
    ASSERT (errno == EBADF);
  }
  {
    close (99);
    errno = 0;
    ASSERT (fdatasync (99) == -1);
    ASSERT (errno == EBADF);
  }

  fd = open (file, O_WRONLY|O_CREAT|O_TRUNC, 0644);
  ASSERT (0 <= fd);
  ASSERT (write (fd, "hello", 5) == 5);
  ASSERT (fdatasync (fd) == 0);
  ASSERT (close (fd) == 0);

#if 0
   
  fd = open (file, O_RDONLY);
  ASSERT (0 <= fd);
  errno = 0;
  ASSERT (fdatasync (fd) == -1);
  ASSERT (errno == EBADF);
  ASSERT (close (fd) == 0);
#endif

  ASSERT (unlink (file) == 0);

  return 0;
}
