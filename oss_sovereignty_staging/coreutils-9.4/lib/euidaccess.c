 

#ifndef _LIBC
# include <config.h>
#endif

#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#if defined _WIN32 && ! defined __CYGWIN__
# include <io.h>
#else
# include "root-uid.h"
#endif

#if HAVE_LIBGEN_H
# include <libgen.h>
#endif

#include <errno.h>
#ifndef __set_errno
# define __set_errno(val) errno = (val)
#endif

#if defined EACCES && !defined EACCESS
# define EACCESS EACCES
#endif

#ifndef F_OK
# define F_OK 0
# define X_OK 1
# define W_OK 2
# define R_OK 4
#endif


#ifdef _LIBC

# define access __access
# define getuid __getuid
# define getgid __getgid
# define geteuid __geteuid
# define getegid __getegid
# define group_member __group_member
# define euidaccess __euidaccess
# undef stat
# define stat stat64

#endif

 

int
euidaccess (const char *file, int mode)
{
#if HAVE_FACCESSAT                    
  return faccessat (AT_FDCWD, file, mode, AT_EACCESS);
#elif defined EFF_ONLY_OK                
  return access (file, mode | EFF_ONLY_OK);
#elif defined ACC_SELF                   
  return accessx (file, mode, ACC_SELF);
#elif HAVE_EACCESS                       
  return eaccess (file, mode);
#elif defined _WIN32 && ! defined __CYGWIN__   
  return _access (file, mode);
#else               

  uid_t uid = getuid ();
  gid_t gid = getgid ();
  uid_t euid = geteuid ();
  gid_t egid = getegid ();
  struct stat stats;

# if HAVE_DECL_SETREGID && PREFER_NONREENTRANT_EUIDACCESS

   

  if (mode == F_OK)
    {
      int result = stat (file, &stats);
      return result != 0 && errno == EOVERFLOW ? 0 : result;
    }
  else
    {
      int result;
      int saved_errno;

      if (uid != euid)
        setreuid (euid, uid);
      if (gid != egid)
        setregid (egid, gid);

      result = access (file, mode);
      saved_errno = errno;

       
      if (uid != euid)
        setreuid (uid, euid);
      if (gid != egid)
        setregid (gid, egid);

      errno = saved_errno;
      return result;
    }

# else

   

  unsigned int granted;
  if (uid == euid && gid == egid)
     
    return access (file, mode);

  if (stat (file, &stats) == -1)
    return mode == F_OK && errno == EOVERFLOW ? 0 : -1;

   
  if (euid == ROOT_UID
      && ((mode & X_OK) == 0
          || (stats.st_mode & (S_IXUSR | S_IXGRP | S_IXOTH))))
    return 0;

   
  if (R_OK == 4 && W_OK == 2 && X_OK == 1 && F_OK == 0)
    mode &= 7;
  else
    mode = ((mode & R_OK ? 4 : 0)
            + (mode & W_OK ? 2 : 0)
            + (mode & X_OK ? 1 : 0));

  if (mode == 0)
    return 0;                    

   
  if (S_IRUSR == (4 << 6) && S_IWUSR == (2 << 6) && S_IXUSR == (1 << 6)
      && S_IRGRP == (4 << 3) && S_IWGRP == (2 << 3) && S_IXGRP == (1 << 3)
      && S_IROTH == (4 << 0) && S_IWOTH == (2 << 0) && S_IXOTH == (1 << 0))
    granted = stats.st_mode;
  else
    granted = ((stats.st_mode & S_IRUSR ? 4 << 6 : 0)
               + (stats.st_mode & S_IWUSR ? 2 << 6 : 0)
               + (stats.st_mode & S_IXUSR ? 1 << 6 : 0)
               + (stats.st_mode & S_IRGRP ? 4 << 3 : 0)
               + (stats.st_mode & S_IWGRP ? 2 << 3 : 0)
               + (stats.st_mode & S_IXGRP ? 1 << 3 : 0)
               + (stats.st_mode & S_IROTH ? 4 << 0 : 0)
               + (stats.st_mode & S_IWOTH ? 2 << 0 : 0)
               + (stats.st_mode & S_IXOTH ? 1 << 0 : 0));

  if (euid == stats.st_uid)
    granted >>= 6;
  else if (egid == stats.st_gid || group_member (stats.st_gid))
    granted >>= 3;

  if ((mode & ~granted) == 0)
    return 0;
  __set_errno (EACCESS);
  return -1;

# endif
#endif
}
#undef euidaccess
#ifdef weak_alias
weak_alias (__euidaccess, euidaccess)
#endif

#ifdef TEST
# include <error.h>
# include <stdio.h>
# include <stdlib.h>

int
main (int argc, char **argv)
{
  char *file;
  int mode;
  int err;

  if (argc < 3)
    abort ();
  file = argv[1];
  mode = atoi (argv[2]);

  err = euidaccess (file, mode);
  printf ("%d\n", err);
  if (err != 0)
    error (0, errno, "%s", file);
  exit (0);
}
#endif
