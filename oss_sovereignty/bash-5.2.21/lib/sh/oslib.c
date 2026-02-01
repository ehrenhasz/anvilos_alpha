 

 

#include <config.h>

#include <bashtypes.h>
#if defined (HAVE_SYS_PARAM_H)
#  include <sys/param.h>
#endif

#if defined (HAVE_UNISTD_H)
#  include <unistd.h>
#endif

#if defined (HAVE_LIMITS_H)
#  include <limits.h>
#endif

#include <posixstat.h>
#include <filecntl.h>
#include <bashansi.h>

#if !defined (HAVE_KILLPG)
#  include <signal.h>
#endif

#include <stdio.h>
#include <errno.h>
#include <chartypes.h>

#include <shell.h>

#if !defined (errno)
extern int errno;
#endif  

 
#if !defined (HAVE_STRCHR)
char *
strchr (string, c)
     char *string;
     int c;
{
  register char *s;

  for (s = string; s && *s; s++)
    if (*s == c)
      return (s);

  return ((char *) NULL);
}

char *
strrchr (string, c)
     char *string;
     int c;
{
  register char *s, *t;

  for (s = string, t = (char *)NULL; s && *s; s++)
    if (*s == c)
      t = s;
  return (t);
}
#endif  

#if !defined (HAVE_DUP2) || defined (DUP2_BROKEN)
 
int
dup2 (fd1, fd2)
     int fd1, fd2;
{
  int saved_errno, r;

   
  if (fcntl (fd1, F_GETFL, 0) == -1)
    return (-1);

  if (fd2 < 0 || fd2 >= getdtablesize ())
    {
      errno = EBADF;
      return (-1);
    }

  if (fd1 == fd2)
    return (0);

  saved_errno = errno;

  (void) close (fd2);
  r = fcntl (fd1, F_DUPFD, fd2);

  if (r >= 0)
    errno = saved_errno;
  else
    if (errno == EINVAL)
      errno = EBADF;

   
  SET_OPEN_ON_EXEC (fd2);
  return (r);
}
#endif  

 

#if !defined (HAVE_GETDTABLESIZE)
int
getdtablesize ()
{
#  if defined (_POSIX_VERSION) && defined (HAVE_SYSCONF) && defined (_SC_OPEN_MAX)
  return (sysconf(_SC_OPEN_MAX));	 
#  else  
#    if defined (ULIMIT_MAXFDS)
  return (ulimit (4, 0L));	 
#    else  
#      if defined (NOFILE)	 
  return (NOFILE);
#      else  
  return (20);			 
#      endif  
#    endif  
#  endif  
}
#endif  

#if !defined (HAVE_BCOPY)
#  if defined (bcopy)
#    undef bcopy
#  endif
void
bcopy (s,d,n)
     void *d, *s;
     size_t n;
{
  FASTCOPY (s, d, n);
}
#endif  

#if !defined (HAVE_BZERO)
#  if defined (bzero)
#    undef bzero
#  endif
void
bzero (s, n)
     void *s; 
     size_t n;
{
  register int i;
  register char *r;

  for (i = 0, r = s; i < n; i++)
    *r++ = '\0';
}
#endif

#if !defined (HAVE_GETHOSTNAME)
#  if defined (HAVE_UNAME)
#    include <sys/utsname.h>
int
gethostname (name, namelen)
     char *name;
     size_t namelen;
{
  int i;
  struct utsname ut;

  --namelen;

  uname (&ut);
  i = strlen (ut.nodename) + 1;
  strncpy (name, ut.nodename, i < namelen ? i : namelen);
  name[namelen] = '\0';
  return (0);
}
#  else  
int
gethostname (name, namelen)
     char *name;
     size_t namelen;
{
  strncpy (name, "unknown", namelen);
  name[namelen] = '\0';
  return 0;
}
#  endif  
#endif  

#if !defined (HAVE_KILLPG)
int
killpg (pgrp, sig)
     pid_t pgrp;
     int sig;
{
  return (kill (-pgrp, sig));
}
#endif  

#if !defined (HAVE_MKFIFO) && defined (PROCESS_SUBSTITUTION)
int
mkfifo (path, mode)
     char *path;
     mode_t mode;
{
#if defined (S_IFIFO)
  return (mknod (path, (mode | S_IFIFO), 0));
#else  
  return (-1);
#endif  
}
#endif  

#define DEFAULT_MAXGROUPS 64

int
getmaxgroups ()
{
  static int maxgroups = -1;

  if (maxgroups > 0)
    return maxgroups;

#if defined (HAVE_SYSCONF) && defined (_SC_NGROUPS_MAX)
  maxgroups = sysconf (_SC_NGROUPS_MAX);
#else
#  if defined (NGROUPS_MAX)
  maxgroups = NGROUPS_MAX;
#  else  
#    if defined (NGROUPS)
  maxgroups = NGROUPS;
#    else  
  maxgroups = DEFAULT_MAXGROUPS;
#    endif  
#  endif    
#endif  

  if (maxgroups <= 0)
    maxgroups = DEFAULT_MAXGROUPS;

  return maxgroups;
}

long
getmaxchild ()
{
  static long maxchild = -1L;

  if (maxchild > 0)
    return maxchild;

#if defined (HAVE_SYSCONF) && defined (_SC_CHILD_MAX)
  maxchild = sysconf (_SC_CHILD_MAX);
#else
#  if defined (CHILD_MAX)
  maxchild = CHILD_MAX;
#  else
#    if defined (MAXUPRC)
  maxchild = MAXUPRC;
#    endif  
#  endif  
#endif  

  return (maxchild);
}
