 
  {
    struct sockaddr_in addr;

    addr.sin_family = AF_INET;
    inet_pton (AF_INET, "127.0.0.1", &addr.sin_addr);
    addr.sin_port = htons (80);
    {
      errno = 0;
      ASSERT (bind (-1, (const struct sockaddr *) &addr, sizeof (addr)) == -1);
      ASSERT (errno == EBADF);
    }
    {
      close (99);
      errno = 0;
      ASSERT (bind (99, (const struct sockaddr *) &addr, sizeof (addr)) == -1);
      ASSERT (errno == EBADF);
    }
  }

  return 0;
}
