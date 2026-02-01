 
#if HAVE_MSVC_INVALID_PARAMETER_HANDLER \
    && MSVC_INVALID_PARAMETER_HANDLING == DEFAULT_HANDLING
  gl_msvc_inval_ensure_handler ();
#endif

   
  {
    const char text[] = "hello world";
    int fd = open (filename, O_RDWR | O_CREAT | O_TRUNC, 0600);
    ASSERT (fd >= 0);
    ASSERT (write (fd, text, sizeof (text)) == sizeof (text));
    ASSERT (close (fd) == 0);
  }

   
  #if !defined __ANDROID__  
  {
    FILE *fp = fopen (filename, "r");
    char buf[5];
    ASSERT (fp != NULL);
    ASSERT (close (fileno (fp)) == 0);
    errno = 0;
    ASSERT (fread (buf, 1, sizeof (buf), fp) == 0);
    ASSERT (errno == EBADF);
    ASSERT (ferror (fp));
    fclose (fp);
  }
  #endif

   
  {
    FILE *fp = fdopen (-1, "r");
    if (fp != NULL)
      {
        char buf[1];
        errno = 0;
        ASSERT (fread (buf, 1, 1, fp) == 0);
        ASSERT (errno == EBADF);
        ASSERT (ferror (fp));
        fclose (fp);
      }
  }
  {
    FILE *fp;
    close (99);
    fp = fdopen (99, "r");
    if (fp != NULL)
      {
        char buf[1];
        errno = 0;
        ASSERT (fread (buf, 1, 1, fp) == 0);
        ASSERT (errno == EBADF);
        ASSERT (ferror (fp));
        fclose (fp);
      }
  }

   
  unlink (filename);

  return 0;
}
