 
#if __GNUC__ >= 13
# pragma GCC diagnostic ignored "-Wanalyzer-fd-use-without-check"
#endif

int
main (void)
{
  (void) gl_sockets_startup (SOCKETS_1_1);

   
  {
    errno = 0;
    ASSERT (listen (-1, 1) == -1);
    ASSERT (errno == EBADF);
  }
  {
    close (99);
    errno = 0;
    ASSERT (listen (99 ,1) == -1);
    ASSERT (errno == EBADF);
  }

  return 0;
}
