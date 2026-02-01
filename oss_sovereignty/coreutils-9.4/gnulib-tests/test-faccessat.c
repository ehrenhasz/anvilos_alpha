 
  {
    errno = 0;
    ASSERT (faccessat (-1, "foo", F_OK, 0) == -1);
    ASSERT (errno == EBADF);
  }
  {
    close (99);
    errno = 0;
    ASSERT (faccessat (99, "foo", F_OK, 0) == -1);
    ASSERT (errno == EBADF);
  }

   
  unlink (BASE "file");
  ASSERT (faccessat (AT_FDCWD, ".", X_OK, 0) == 0);
  ASSERT (faccessat (AT_FDCWD, "./", X_OK, 0) == 0);
  ASSERT (close (open (BASE "file", O_CREAT | O_WRONLY, 0)) == 0);
  ASSERT (faccessat (AT_FDCWD, BASE "file", F_OK, 0) == 0);
  ASSERT (faccessat (AT_FDCWD, BASE "file/", F_OK, 0) != 0);
  unlink (BASE "link1");
  if (symlink (".", BASE "link1") == 0)
    {
      ASSERT (faccessat (AT_FDCWD, BASE "link1", X_OK, 0) == 0);
      ASSERT (faccessat (AT_FDCWD, BASE "link1/", X_OK, 0) == 0);

      unlink (BASE "link2");
      ASSERT (symlink (BASE "file", BASE "link2") == 0);
      ASSERT (faccessat (AT_FDCWD, BASE "link2", F_OK, 0) == 0);
      ASSERT (faccessat (AT_FDCWD, BASE "link2/", F_OK, 0) != 0);
      unlink (BASE "link2");

      unlink (BASE "link3");
      ASSERT (symlink (BASE "no-such-file", BASE "link3") == 0);
      ASSERT (faccessat (AT_FDCWD, BASE "link3", F_OK, 0) != 0);
      ASSERT (faccessat (AT_FDCWD, BASE "link3/", F_OK, 0) != 0);
      unlink (BASE "link3");
    }
  unlink (BASE "link1");
  unlink (BASE "file");

  return 0;
}
