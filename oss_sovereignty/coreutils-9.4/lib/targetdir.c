 

ATTRIBUTE_PURE static bool
must_be_working_directory (char const *f)
{
   
  while (*f++ == '.')
    {
      if (*f != '/')
        return !*f;
      while (*++f == '/')
        continue;
      if (!*f)
        return true;
    }
  return false;
}

 

int
target_directory_operand (char const *file, struct stat *st)
{
  if (must_be_working_directory (file))
    return AT_FDCWD;

  int fd = -1;
  int try_to_open = 1;
  int stat_result;

   
  if (!O_DIRECTORY)
    {
      stat_result = stat (file, st);
      if (stat_result == 0)
        {
          try_to_open = S_ISDIR (st->st_mode);
          errno = ENOTDIR;
        }
      else
        {
           
          try_to_open = errno == EOVERFLOW;
        }
    }

  if (try_to_open)
    {
      fd = open (file, O_PATHSEARCH | O_DIRECTORY);

       
      if (O_PATHSEARCH == O_SEARCH && fd < 0 && errno == EACCES)
        errno = (((O_DIRECTORY ? stat (file, st) : stat_result) == 0
                  && !S_ISDIR (st->st_mode))
                 ? ENOTDIR : EACCES);
    }

  if (!O_DIRECTORY && 0 <= fd)
    {
       
      int err;
      if (fstat (fd, st) == 0
          ? !S_ISDIR (st->st_mode) && (err = ENOTDIR, true)
          : (err = errno) != EOVERFLOW)
        {
          close (fd);
          errno = err;
          fd = -1;
        }
    }

  return fd - (AT_FDCWD == -1 && fd < 0);
}
