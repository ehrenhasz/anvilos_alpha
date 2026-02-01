 
  fd = open (filename, O_CREAT | O_WRONLY, 0600);
  ASSERT (fd >= 0);
  ASSERT (write (fd, "Hello World", 11) == 11);
  ASSERT (close (fd) == 0);

   
  fd = open (filename, O_RDONLY);
  ASSERT (fd >= 0);
  ASSERT (lseek (fd, 6, SEEK_SET) == 6);
  {
    char buf[10];
    ssize_t ret = read (fd, buf, 10);
    ASSERT (ret == 5);
    ASSERT (memcmp (buf, "World", 5) == 0);
  }
  ASSERT (close (fd) == 0);

   
  {
    char byte;
    errno = 0;
    ASSERT (read (-1, &byte, 1) == -1);
    ASSERT (errno == EBADF);
  }
  {
    char byte;
    close (99);
    errno = 0;
    ASSERT (read (99, &byte, 1) == -1);
    ASSERT (errno == EBADF);
  }

   
  unlink (filename);

  return 0;
}
