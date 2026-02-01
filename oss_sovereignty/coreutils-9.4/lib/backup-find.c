 

char *
find_backup_file_name (int dir_fd, char const *file,
                       enum backup_type backup_type)
{
  char *result = backupfile_internal (dir_fd, file, backup_type, false);
  if (!result)
    xalloc_die ();
  return result;
}

static char const *const backup_args[] =
{
   
  "none", "off",
  "simple", "never",
  "existing", "nil",
  "numbered", "t",
  NULL
};

static const enum backup_type backup_types[] =
{
  no_backups, no_backups,
  simple_backups, simple_backups,
  numbered_existing_backups, numbered_existing_backups,
  numbered_backups, numbered_backups
};

 
ARGMATCH_VERIFY (backup_args, backup_types);

 

enum backup_type
get_version (char const *context, char const *version)
{
  if (version == 0 || *version == 0)
    return numbered_existing_backups;
  else
    return XARGMATCH (context, version, backup_args, backup_types);
}


 

enum backup_type
xget_version (char const *context, char const *version)
{
  if (version && *version)
    return get_version (context, version);
  else
    return get_version ("$VERSION_CONTROL", getenv ("VERSION_CONTROL"));
}
