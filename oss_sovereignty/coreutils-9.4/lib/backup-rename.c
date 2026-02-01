 

char *
backup_file_rename (int dir_fd, char const *file, enum backup_type backup_type)
{
  return backupfile_internal (dir_fd, file, backup_type, true);
}
