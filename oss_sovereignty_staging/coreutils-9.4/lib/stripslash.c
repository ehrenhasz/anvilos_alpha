 

bool
strip_trailing_slashes (char *file)
{
  char *base = last_component (file);
  char *base_lim;
  bool had_slash;

   
  if (! *base)
    base = file;
  base_lim = base + base_len (base);
  had_slash = (*base_lim != '\0');
  *base_lim = '\0';
  return had_slash;
}
