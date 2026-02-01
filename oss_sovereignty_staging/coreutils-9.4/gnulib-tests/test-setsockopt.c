 
  {
    int value = 1;

    errno = 0;
    ASSERT (setsockopt (-1, SOL_SOCKET, SO_REUSEADDR, &value, sizeof (value))
            == -1);
    ASSERT (errno == EBADF);
  }
  {
    int value = 1;

    close (99);
    errno = 0;
    ASSERT (setsockopt (99, SOL_SOCKET, SO_REUSEADDR, &value, sizeof (value))
            == -1);
    ASSERT (errno == EBADF);
  }

  return 0;
}
