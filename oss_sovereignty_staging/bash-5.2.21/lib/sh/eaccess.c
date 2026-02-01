 

 

#if defined (HAVE_CONFIG_H)
#  include <config.h>
#endif

#include <stdio.h>

#include "bashtypes.h"

#if defined (HAVE_UNISTD_H)
#  include <unistd.h>
#endif

#include "bashansi.h"

#include <errno.h>
#if !defined (errno)
extern int errno;
#endif  

#if !defined (_POSIX_VERSION) && defined (HAVE_SYS_FILE_H)
#  include <sys/file.h>
#endif  
#include "posixstat.h"
#include "filecntl.h"

#include "shell.h"

#if !defined (R_OK)
#define R_OK 4
#define W_OK 2
#define X_OK 1
#define F_OK 0
#endif  

static int path_is_devfd PARAMS((const char *));
static int sh_stataccess PARAMS((const char *, int));
#if HAVE_DECL_SETREGID
static int sh_euidaccess PARAMS((const char *, int));
#endif

static int
path_is_devfd (path)
     const char *path;
{
  if (path[0] == '/' && path[1] == 'd' && strncmp (path, "/dev/fd/", 8) == 0)
    return 1;
  else if (STREQN (path, "/dev/std", 8))
    {
      if (STREQ (path+8, "in") || STREQ (path+8, "out") || STREQ (path+8, "err"))
	return 1;
      else
	return 0;
    }
  else
    return 0;
}

 
int
sh_stat (path, finfo)
     const char *path;
     struct stat *finfo;
{
  static char *pbuf = 0;

  if (*path == '\0')
    {
      errno = ENOENT;
      return (-1);
    }
  if (path[0] == '/' && path[1] == 'd' && strncmp (path, "/dev/fd/", 8) == 0)
    {
       
#if !defined (HAVE_DEV_FD) || defined (DEV_FD_STAT_BROKEN)
      intmax_t fd;
      int r;

      if (legal_number (path + 8, &fd) && fd == (int)fd)
        {
          r = fstat ((int)fd, finfo);
          if (r == 0 || errno != EBADF)
            return (r);
        }
      errno = ENOENT;
      return (-1);
#else
   
      pbuf = xrealloc (pbuf, sizeof (DEV_FD_PREFIX) + strlen (path + 8));
      strcpy (pbuf, DEV_FD_PREFIX);
      strcat (pbuf, path + 8);
      return (stat (pbuf, finfo));
#endif  
    }
#if !defined (HAVE_DEV_STDIN)
  else if (STREQN (path, "/dev/std", 8))
    {
      if (STREQ (path+8, "in"))
	return (fstat (0, finfo));
      else if (STREQ (path+8, "out"))
	return (fstat (1, finfo));
      else if (STREQ (path+8, "err"))
	return (fstat (2, finfo));
      else
	return (stat (path, finfo));
    }
#endif  
  return (stat (path, finfo));
}

 
static int
sh_stataccess (path, mode)
     const char *path;
     int mode;
{
  struct stat st;

  if (sh_stat (path, &st) < 0)
    return (-1);

  if (current_user.euid == 0)
    {
       
      if ((mode & X_OK) == 0)
	return (0);

       
      if (st.st_mode & S_IXUGO)
	return (0);
    }

  if (st.st_uid == current_user.euid)	 
    mode <<= 6;
  else if (group_member (st.st_gid))
    mode <<= 3;

  if (st.st_mode & mode)
    return (0);

  errno = EACCES;
  return (-1);
}

#if HAVE_DECL_SETREGID
 
static int
sh_euidaccess (path, mode)
     const char *path;
     int mode;
{
  int r, e;

  if (current_user.uid != current_user.euid)
    setreuid (current_user.euid, current_user.uid);
  if (current_user.gid != current_user.egid)
    setregid (current_user.egid, current_user.gid);

  r = access (path, mode);
  e = errno;

  if (current_user.uid != current_user.euid)
    setreuid (current_user.uid, current_user.euid);
  if (current_user.gid != current_user.egid)
    setregid (current_user.gid, current_user.egid);

  errno = e;
  return r;  
}
#endif

int
sh_eaccess (path, mode)
     const char *path;
     int mode;
{
  int ret;

  if (path_is_devfd (path))
    return (sh_stataccess (path, mode));

#if (defined (HAVE_FACCESSAT) && defined (AT_EACCESS)) || defined (HAVE_EACCESS)
#  if defined (HAVE_FACCESSAT) && defined (AT_EACCESS)
  ret = faccessat (AT_FDCWD, path, mode, AT_EACCESS);
#  else		 	 
  ret = eaccess (path, mode);	 
#  endif	 
#  if defined (__FreeBSD__) || defined (SOLARIS) || defined (_AIX)
  if (ret == 0 && current_user.euid == 0 && mode == X_OK)
    return (sh_stataccess (path, mode));
#  endif	 
  return ret;
#elif defined (EFF_ONLY_OK)		 
  return access (path, mode|EFF_ONLY_OK);
#else
  if (mode == F_OK)
    return (sh_stataccess (path, mode));
    
#  if HAVE_DECL_SETREGID
  if (current_user.uid != current_user.euid || current_user.gid != current_user.egid)
    return (sh_euidaccess (path, mode));
#  endif

  if (current_user.uid == current_user.euid && current_user.gid == current_user.egid)
    {
      ret = access (path, mode);
#if defined (__FreeBSD__) || defined (SOLARIS)
      if (ret == 0 && current_user.euid == 0 && mode == X_OK)
	return (sh_stataccess (path, mode));
#endif
      return ret;
    }

  return (sh_stataccess (path, mode));
#endif
}
