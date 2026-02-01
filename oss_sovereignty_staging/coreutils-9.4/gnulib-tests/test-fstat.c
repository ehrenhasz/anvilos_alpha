 
  {
    struct stat statbuf;

    errno = 0;
    ASSERT (fstat (-1, &statbuf) == -1);
    ASSERT (errno == EBADF);
  }
  {
    struct stat statbuf;

    close (99);
    errno = 0;
    ASSERT (fstat (99, &statbuf) == -1);
    ASSERT (errno == EBADF);
  }

  return 0;
}
