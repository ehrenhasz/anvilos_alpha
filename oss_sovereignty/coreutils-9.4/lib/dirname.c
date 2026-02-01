 

char *
dir_name (char const *file)
{
  char *result = mdir_name (file);
  if (!result)
    xalloc_die ();
  return result;
}
