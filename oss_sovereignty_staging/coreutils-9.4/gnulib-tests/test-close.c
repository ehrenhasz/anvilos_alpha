 
  {
    errno = 0;
    ASSERT (close (-1) == -1);
    ASSERT (errno == EBADF);
  }
  {
    close (99);
    errno = 0;
    ASSERT (close (99) == -1);
    ASSERT (errno == EBADF);
  }

  return 0;
}
