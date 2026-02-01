 
extern char *
find_mount_point (char const *file, struct stat const *file_stat)
{
  struct saved_cwd cwd;
  struct stat last_stat;
  char *mp = nullptr;		 

  if (save_cwd (&cwd) != 0)
    {
      error (0, errno, _("cannot get current directory"));
      return nullptr;
    }

  if (S_ISDIR (file_stat->st_mode))
     
    {
      last_stat = *file_stat;
      if (chdir (file) < 0)
        {
          error (0, errno, _("cannot change to directory %s"), quoteaf (file));
          return nullptr;
        }
    }
  else
     
    {
      char *xdir = dir_name (file);
      char *dir;
      ASSIGN_STRDUPA (dir, xdir);
      free (xdir);

      if (chdir (dir) < 0)
        {
          error (0, errno, _("cannot change to directory %s"), quoteaf (dir));
          return nullptr;
        }

      if (stat (".", &last_stat) < 0)
        {
          error (0, errno, _("cannot stat current directory (now %s)"),
                 quoteaf (dir));
          goto done;
        }
    }

   
  while (true)
    {
      struct stat st;
      if (stat ("..", &st) < 0)
        {
          error (0, errno, _("cannot stat %s"), quoteaf (".."));
          goto done;
        }
      if (st.st_dev != last_stat.st_dev || st.st_ino == last_stat.st_ino)
         
        break;
      if (chdir ("..") < 0)
        {
          error (0, errno, _("cannot change to directory %s"), quoteaf (".."));
          goto done;
        }
      last_stat = st;
    }

   
  mp = xgetcwd ();

done:
   
  {
    int save_errno = errno;
    if (restore_cwd (&cwd) != 0)
      error (EXIT_FAILURE, errno,
             _("failed to return to initial working directory"));
    free_cwd (&cwd);
    errno = save_errno;
  }

  return mp;
}
