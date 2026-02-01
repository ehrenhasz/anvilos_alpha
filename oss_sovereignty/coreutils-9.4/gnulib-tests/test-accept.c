 
  {
    struct sockaddr_in addr;
    socklen_t addrlen = sizeof (addr);

    errno = 0;
    ASSERT (accept (-1, (struct sockaddr *) &addr, &addrlen) == -1);
    ASSERT (errno == EBADF);
  }
  {
    struct sockaddr_in addr;
    socklen_t addrlen = sizeof (addr);

    close (99);
    errno = 0;
    ASSERT (accept (99, (struct sockaddr *) &addr, &addrlen) == -1);
    ASSERT (errno == EBADF);
  }

  return 0;
}
