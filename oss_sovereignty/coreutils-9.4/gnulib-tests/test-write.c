 
  fd = open (filename, O_CREAT | O_WRONLY, 0600);
  ASSERT (fd >= 0);
  ASSERT (write (fd, "Hello World", 11) == 11);
  ASSERT (close (fd) == 0);

   
  fd = open (filename, O_WRONLY);
  ASSERT (fd >= 0);
  ASSERT (lseek (fd, 6, SEEK_SET) == 6);
  ASSERT (write (fd, "fascination", 11) == 11);

   
  {
    char buf[64];
    int rfd = open (filename, O_RDONLY);
    ASSERT (rfd >= 0);
    ASSERT (read (rfd, buf, sizeof (buf)) == 17);
    ASSERT (close (rfd) == 0);
    ASSERT (memcmp (buf, "Hello fascination", 17) == 0);
  }

  ASSERT (close (fd) == 0);

   
  {
    char byte = 'x';
    errno = 0;
    ASSERT (write (-1, &byte, 1) == -1);
    ASSERT (errno == EBADF);
  }
  {
    char byte = 'x';
    close (99);
    errno = 0;
    ASSERT (write (99, &byte, 1) == -1);
    ASSERT (errno == EBADF);
  }

   
  unlink (filename);

  return 0;
}
