 
#if HAVE_MSVC_INVALID_PARAMETER_HANDLER \
    && MSVC_INVALID_PARAMETER_HANDLING == DEFAULT_HANDLING
  gl_msvc_inval_ensure_handler ();
#endif

   
  #if !defined __ANDROID__  
  {
    FILE *fp = fopen (filename, "w");
    ASSERT (fp != NULL);
    setvbuf (fp, NULL, _IONBF, 0);
    ASSERT (close (fileno (fp)) == 0);
    errno = 0;
    ASSERT (fputc ('x', fp) == EOF);
    ASSERT (errno == EBADF);
    ASSERT (ferror (fp));
    fclose (fp);
  }
  #endif

   
  {
    FILE *fp = fdopen (-1, "w");
    if (fp != NULL)
      {
        setvbuf (fp, NULL, _IONBF, 0);
        errno = 0;
        ASSERT (fputc ('x', fp) == EOF);
        ASSERT (errno == EBADF);
        ASSERT (ferror (fp));
        fclose (fp);
      }
  }
  {
    FILE *fp;
    close (99);
    fp = fdopen (99, "w");
    if (fp != NULL)
      {
        setvbuf (fp, NULL, _IONBF, 0);
        errno = 0;
        ASSERT (fputc ('x', fp) == EOF);
        ASSERT (errno == EBADF);
        ASSERT (ferror (fp));
        fclose (fp);
      }
  }

   
  unlink (filename);

  return 0;
}
