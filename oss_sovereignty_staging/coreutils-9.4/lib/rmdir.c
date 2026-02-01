 

int
rpl_rmdir (char const *dir)
{
   
  size_t len = strlen (dir);
  int result;
  while (len && ISSLASH (dir[len - 1]))
    len--;
  if (len && dir[len - 1] == '.' && (1 == len || ISSLASH (dir[len - 2])))
    {
      errno = EINVAL;
      return -1;
    }
  result = rmdir (dir);
   
  if (result == -1 && errno == EINVAL)
    errno = ENOTDIR;
  return result;
}
