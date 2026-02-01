 
  {
    FILE *fp = fopen (filename, "r");
    ASSERT (fp != NULL);
    #if !defined __ANDROID__  
    setvbuf (fp, NULL, _IONBF, 0);
    ASSERT (ftell (fp) == 0);
    ASSERT (fseeko (fp, 0, SEEK_END) == 0);
    ASSERT (ftell (fp) > 0);
    ASSERT (close (fileno (fp)) == 0);
    errno = 0;
    ASSERT (fseeko (fp, 0, SEEK_SET) == -1);
    ASSERT (errno == EBADF);
    #endif
    fclose (fp);
  }

   
  {
    FILE *fp = fdopen (-1, "w");
    if (fp != NULL)
      {
        errno = 0;
        ASSERT (fseeko (fp, 0, SEEK_END) == -1);
        ASSERT (errno == EBADF);
        fclose (fp);
      }
  }
  {
    FILE *fp;
    close (99);
    fp = fdopen (99, "w");
    if (fp != NULL)
      {
        errno = 0;
        ASSERT (fseeko (fp, 0, SEEK_END) == -1);
        ASSERT (errno == EBADF);
        fclose (fp);
      }
  }

  return 0;
}
