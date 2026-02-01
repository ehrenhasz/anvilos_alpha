 
  {
    errno = 0;
    ASSERT (dup (-1) == -1);
    ASSERT (errno == EBADF);
  }
  {
    close (99);
    errno = 0;
    ASSERT (dup (99) == -1);
    ASSERT (errno == EBADF);
  }

  return 0;
}
