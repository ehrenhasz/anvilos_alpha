 

 

static int
test_mkfifo (int (*func) (char const *, mode_t), bool print)
{
  int result = func (BASE "fifo", 0600);
  struct stat st;
  if (result == -1 && errno == ENOSYS)
    {
      if (print)
        fputs ("skipping test: no support for named fifos\n", stderr);
      return 77;
    }
  ASSERT (result == 0);
  ASSERT (stat (BASE "fifo", &st) == 0);
  ASSERT (S_ISFIFO (st.st_mode));

   
  errno = 0;
  ASSERT (func ("", S_IRUSR | S_IWUSR) == -1);
  ASSERT (errno == ENOENT);
  errno = 0;
  ASSERT (func (".", 0600) == -1);
   
  ASSERT (errno == EEXIST || errno == EINVAL || errno == EISDIR);
  errno = 0;
  ASSERT (func (BASE "fifo", 0600) == -1);
  ASSERT (errno == EEXIST);
  ASSERT (unlink (BASE "fifo") == 0);
  errno = 0;
  ASSERT (func (BASE "fifo/", 0600) == -1);
  ASSERT (errno == ENOENT || errno == ENOTDIR);

   
  if (symlink (BASE "fifo", BASE "link"))
    {
      if (print)
        fputs ("skipping test: symlinks not supported on this file system\n",
               stderr);
      return 77;
    }
  errno = 0;
  ASSERT (func (BASE "link", 0600) == -1);
  ASSERT (errno == EEXIST);
  errno = 0;
  ASSERT (func (BASE "link/", 0600) == -1);
  ASSERT (errno == EEXIST || errno == ENOENT || errno == ENOTDIR);
  errno = 0;
  ASSERT (unlink (BASE "fifo") == -1);
  ASSERT (errno == ENOENT);
  ASSERT (unlink (BASE "link") == 0);
  return 0;
}
