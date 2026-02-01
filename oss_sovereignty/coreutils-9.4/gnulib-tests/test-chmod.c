 
  {
    struct stat statbuf;
    unlink (BASE "file");
    ASSERT (close (creat (BASE "file", 0600)) == 0);
    ASSERT (chmod (BASE "file", 0400) == 0);
    ASSERT (stat (BASE "file", &statbuf) >= 0);
    ASSERT ((statbuf.st_mode & 0700) == 0400);

    errno = 0;
    ASSERT (chmod (BASE "file/", 0600) == -1);
    ASSERT (errno == ENOTDIR);

     
    ASSERT (chmod (BASE "file", 0600) == 0);
    ASSERT (unlink (BASE "file") == 0);
  }

   
  {
    struct stat statbuf;

    rmdir (BASE "dir");
    ASSERT (mkdir (BASE "dir", 0700) == 0);
    ASSERT (chmod (BASE "dir", 0500) == 0);
    ASSERT (stat (BASE "dir", &statbuf) >= 0);
    ASSERT ((statbuf.st_mode & 0700) == 0500);
    ASSERT (chmod (BASE "dir/", 0700) == 0);

     
    ASSERT (rmdir (BASE "dir") == 0);
  }

   
  {
    unlink (BASE "file");
    unlink (BASE "link");
    if (symlink (BASE "file", BASE "link") == 0)
      {
        struct stat statbuf;
        ASSERT (close (creat (BASE "file", 0600)) == 0);
        chmod (BASE "link", 0400);
        ASSERT (stat (BASE "file", &statbuf) >= 0);
        ASSERT ((statbuf.st_mode & 0700) == 0400);
      }
     
    unlink (BASE "file");
    unlink (BASE "link");
  }

  return 0;
}
