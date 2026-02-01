 
#include <stdlib.h>

#include <errno.h>  

#ifdef _AIX
# include <unistd.h>
# include <procinfo.h>
# include <string.h>
#endif

#ifdef __MVS__
# ifndef _OPEN_SYS
#  define _OPEN_SYS
# endif
# include <string.h>
# include <sys/ps.h>
#endif

#ifdef __hpux
# include <unistd.h>
# include <sys/param.h>
# include <sys/pstat.h>
# include <string.h>
#endif

#if defined __sgi || defined __osf__
# include <string.h>
# include <unistd.h>
# include <stdio.h>
# include <fcntl.h>
# include <sys/procfs.h>
#endif

#if defined __SCO_VERSION__ || defined __sysv5__
# include <fcntl.h>
# include <string.h>
#endif

#include "basename-lgpl.h"

#ifndef HAVE_GETPROGNAME   
char const *
getprogname (void)
{
# if HAVE_DECL_PROGRAM_INVOCATION_SHORT_NAME                 
   
   
   
   
   
  extern char *__progname;
  const char *p = __progname;
#  if defined __ANDROID__
  return last_component (p);
#  else
  return p && p[0] ? p : "?";
#  endif
# elif _AIX                                                  
   
  static char *p;
  static int first = 1;
  if (first)
    {
      first = 0;
      pid_t pid = getpid ();
      struct procentry64 procs;
      p = (0 < getprocs64 (&procs, sizeof procs, NULL, 0, &pid, 1)
           ? strdup (procs.pi_comm)
           : NULL);
      if (!p)
        p = "?";
    }
  return p;
# elif defined __hpux
  static char *p;
  static int first = 1;
  if (first)
    {
      first = 0;
      pid_t pid = getpid ();
      struct pst_status status;
      if (pstat_getproc (&status, sizeof status, 0, pid) > 0)
        {
          char *ucomm = status.pst_ucomm;
          char *cmd = status.pst_cmd;
          if (strlen (ucomm) < PST_UCOMMLEN - 1)
            p = ucomm;
          else
            {
               
              char *space = strchr (cmd, ' ');
              if (space != NULL)
                *space = '\0';
              p = strrchr (cmd, '/');
              if (p != NULL)
                p++;
              else
                p = cmd;
              if (strlen (p) > PST_UCOMMLEN - 1
                  && memcmp (p, ucomm, PST_UCOMMLEN - 1) == 0)
                 
                ;
              else
                p = ucomm;
            }
          p = strdup (p);
        }
      else
        {
#  if !defined __LP64__
           
          char status64[1216];
          if (__pstat_getproc64 (status64, sizeof status64, 0, pid) > 0)
            {
              char *ucomm = status64 + 288;
              char *cmd = status64 + 168;
              if (strlen (ucomm) < PST_UCOMMLEN - 1)
                p = ucomm;
              else
                {
                   
                  char *space = strchr (cmd, ' ');
                  if (space != NULL)
                    *space = '\0';
                  p = strrchr (cmd, '/');
                  if (p != NULL)
                    p++;
                  else
                    p = cmd;
                  if (strlen (p) > PST_UCOMMLEN - 1
                      && memcmp (p, ucomm, PST_UCOMMLEN - 1) == 0)
                     
                    ;
                  else
                    p = ucomm;
                }
              p = strdup (p);
            }
          else
#  endif
            p = NULL;
        }
      if (!p)
        p = "?";
    }
  return p;
# elif __MVS__                                               
   
  char filename[50];
  int fd;

  # if defined __sgi
    sprintf (filename, "/proc/pinfo/%d", (int) getpid ());
  # else
    sprintf (filename, "/proc/%d", (int) getpid ());
  # endif
  fd = open (filename, O_RDONLY | O_CLOEXEC);
  if (0 <= fd)
    {
      prpsinfo_t buf;
      int ioctl_ok = 0 <= ioctl (fd, PIOCPSINFO, &buf);
      close (fd);
      if (ioctl_ok)
        {
          char *name = buf.pr_fname;
          size_t namesize = sizeof buf.pr_fname;
           
          char *namenul = memchr (name, '\0', namesize);
          size_t namelen = namenul ? namenul - name : namesize;
          char *namecopy = malloc (namelen + 1);
          if (namecopy)
            {
              namecopy[namelen] = '\0';
              return memcpy (namecopy, name, namelen);
            }
        }
    }
  return NULL;
# elif defined __SCO_VERSION__ || defined __sysv5__                 
  char buf[80];
  int fd;
  sprintf (buf, "/proc/%d/cmdline", getpid());
  fd = open (buf, O_RDONLY);
  if (0 <= fd)
    {
      size_t n = read (fd, buf, 79);
      if (n > 0)
        {
          buf[n] = '\0';  
          char *progname;
          progname = strrchr (buf, '/');
          if (progname)
            {
              progname = progname + 1;  
            }
          else
            {
              progname = buf;
            }
          char *ret;
          ret = malloc (strlen (progname) + 1);
          if (ret)
            {
              strcpy (ret, progname);
              return ret;
            }
        }
      close (fd);
    }
  return "?";
# else
#  error "getprogname module not ported to this OS"
# endif
}

#endif

 
