 

 

#include <config.h>
 
#include <stdio.h>
#include <errno.h>

#include <bashtypes.h>
#include <posixstat.h>
#include <posixdir.h>
#include <bashansi.h>

#if defined (HAVE_SYS_PARAM_H)
#  include <sys/param.h>
#endif

#include <maxpath.h>

 

int
mailstat(path, st)
     const char *path;
     struct stat *st;
{
  static struct stat st_new_last, st_ret_last;
  struct stat st_ret, st_tmp;
  DIR *dd;
  struct dirent *fn;
  char dir[PATH_MAX * 2], file[PATH_MAX * 2 + 1];
  int i, l;
  time_t atime, mtime;

  atime = mtime = 0;

   
  if ((i = stat(path, st)) != 0 || S_ISDIR(st->st_mode) == 0)
    return i;

  if (strlen(path) > sizeof(dir) - 5)
    {
#ifdef ENAMETOOLONG
      errno = ENAMETOOLONG;
#else
      errno = EINVAL;
#endif
      return -1;
    }

  st_ret = *st;
  st_ret.st_nlink = 1;
  st_ret.st_size  = 0;
#ifdef HAVE_STRUCT_STAT_ST_BLOCKS
  st_ret.st_blocks  = 0;
#else
  st_ret.st_nlink = 0;
#endif
  st_ret.st_mode  &= ~S_IFDIR;
  st_ret.st_mode  |= S_IFREG;

   
  sprintf(dir, "%s/cur", path);
  if (stat(dir, &st_tmp) || S_ISDIR(st_tmp.st_mode) == 0)
    return 0;
  st_ret.st_atime = st_tmp.st_atime;

   
  sprintf(dir, "%s/tmp", path);
  if (stat(dir, &st_tmp) || S_ISDIR(st_tmp.st_mode) == 0)
    return 0;
  st_ret.st_mtime = st_tmp.st_mtime;

   
  sprintf(dir, "%s/new", path);
  if (stat(dir, &st_tmp) || S_ISDIR(st_tmp.st_mode) == 0)
    return 0;
  st_ret.st_mtime = st_tmp.st_mtime;

   
  if (st_tmp.st_dev == st_new_last.st_dev &&
      st_tmp.st_ino == st_new_last.st_ino &&
      st_tmp.st_atime == st_new_last.st_atime &&
      st_tmp.st_mtime == st_new_last.st_mtime)
    {
      *st = st_ret_last;
      return 0;
    }
  st_new_last = st_tmp;

   
  for (i = 0; i < 2; i++)
    {
      sprintf(dir, "%s/%s", path, i ? "cur" : "new");
      sprintf(file, "%s/", dir);
      l = strlen(file);
      if ((dd = opendir(dir)) == NULL)
	return 0;
      while ((fn = readdir(dd)) != NULL)
	{
	  if (fn->d_name[0] == '.' || strlen(fn->d_name) + l >= sizeof(file))
	    continue;
	  strcpy(file + l, fn->d_name);
	  if (stat(file, &st_tmp) != 0)
	    continue;
	  st_ret.st_size += st_tmp.st_size;
#ifdef HAVE_STRUCT_STAT_ST_BLOCKS
	  st_ret.st_blocks++;
#else
	  st_ret.st_nlink++;
#endif
	  if (st_tmp.st_atime != st_tmp.st_mtime && st_tmp.st_atime > atime)
	    atime = st_tmp.st_atime;
	  if (st_tmp.st_mtime > mtime)
	    mtime = st_tmp.st_mtime;
	}
      closedir(dd);
    }

 	 
      st_ret.st_atime = atime;
    if (mtime)
      st_ret.st_mtime = mtime;

    *st = st_ret_last = st_ret;
    return 0;
}
