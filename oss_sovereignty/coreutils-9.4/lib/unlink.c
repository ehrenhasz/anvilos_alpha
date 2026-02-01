 

int
rpl_unlink (char const *name)
{
   
  size_t len = strlen (name);
  int result = 0;
  if (len && ISSLASH (name[len - 1]))
    {
       
      struct stat st;
      result = lstat (name, &st);
      if (result == 0 || errno == EOVERFLOW)
        {
           
          char *short_name = malloc (len);
          if (!short_name)
            return -1;
          memcpy (short_name, name, len);
          while (len && ISSLASH (short_name[len - 1]))
            short_name[--len] = '\0';
          if (len && (lstat (short_name, &st) || S_ISLNK (st.st_mode)))
            {
              free (short_name);
              errno = EPERM;
              return -1;
            }
          free (short_name);
          result = 0;
        }
    }
  if (!result)
    {
#if UNLINK_PARENT_BUG
      if (len >= 2 && name[len - 1] == '.' && name[len - 2] == '.'
          && (len == 2 || ISSLASH (name[len - 3])))
        {
          errno = EISDIR;  
          return -1;
        }
#endif
      result = unlink (name);
    }
  return result;
}
