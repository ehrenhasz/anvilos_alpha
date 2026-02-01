 

size_t
dir_len (char const *file)
{
  size_t prefix_length = FILE_SYSTEM_PREFIX_LEN (file);
  size_t length;

   
  prefix_length += (prefix_length != 0
                    ? (FILE_SYSTEM_DRIVE_PREFIX_CAN_BE_RELATIVE
                       && ISSLASH (file[prefix_length]))
                    : (ISSLASH (file[0])
                       ? ((DOUBLE_SLASH_IS_DISTINCT_ROOT
                           && ISSLASH (file[1]) && ! ISSLASH (file[2])
                           ? 2 : 1))
                       : 0));

   
  for (length = last_component (file) - file;
       prefix_length < length; length--)
    if (! ISSLASH (file[length - 1]))
      break;
  return length;
}


 

char *
mdir_name (char const *file)
{
  size_t length = dir_len (file);
  bool append_dot = (length == 0
                     || (FILE_SYSTEM_DRIVE_PREFIX_CAN_BE_RELATIVE
                         && length == FILE_SYSTEM_PREFIX_LEN (file)
                         && file[2] != '\0' && ! ISSLASH (file[2])));
  char *dir = malloc (length + append_dot + 1);
  if (!dir)
    return NULL;
  memcpy (dir, file, length);
  if (append_dot)
    dir[length++] = '.';
  dir[length] = '\0';
  return dir;
}
