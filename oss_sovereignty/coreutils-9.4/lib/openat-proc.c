 

#include <config.h>

#include "openat-priv.h"

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#ifdef __KLIBC__  
# include <InnoTekLIBC/backend.h>
#endif
#ifdef __MVS__  
# include <termios.h>
#endif

#include "intprops.h"

 
char *
openat_proc_name (char buf[OPENAT_BUFFER_SIZE], int fd, char const *file)
{
  char *result = buf;
  int dirlen;

   
  if (!*file)
    {
      buf[0] = '\0';
      return buf;
    }

#if !(defined __KLIBC__ || defined __MVS__)
   
# define PROC_SELF_FD_FORMAT "/proc/self/fd/%d/"
  {
    enum {
      PROC_SELF_FD_DIR_SIZE_BOUND
        = (sizeof PROC_SELF_FD_FORMAT - (sizeof "%d" - 1)
           + INT_STRLEN_BOUND (int))
    };

    static int proc_status = 0;
    if (! proc_status)
      {
         

        int proc_self_fd =
          open ("/proc/self/fd",
                O_SEARCH | O_DIRECTORY | O_NOCTTY | O_NONBLOCK | O_CLOEXEC);
        if (proc_self_fd < 0)
          proc_status = -1;
        else
          {
             
            char dotdot_buf[PROC_SELF_FD_DIR_SIZE_BOUND + sizeof "../fd" - 1];
            sprintf (dotdot_buf, PROC_SELF_FD_FORMAT "../fd", proc_self_fd);
            proc_status = access (dotdot_buf, F_OK) ? -1 : 1;
            close (proc_self_fd);
          }
      }

    if (proc_status < 0)
      return NULL;
    else
      {
        size_t bufsize = PROC_SELF_FD_DIR_SIZE_BOUND + strlen (file);
        if (OPENAT_BUFFER_SIZE < bufsize)
          {
            result = malloc (bufsize);
            if (! result)
              return NULL;
          }

        dirlen = sprintf (result, PROC_SELF_FD_FORMAT, fd);
      }
  }
#else  
   
  {
    size_t bufsize;

# ifdef __KLIBC__
    char dir[_MAX_PATH];
    if (__libc_Back_ioFHToPath (fd, dir, sizeof dir))
      return NULL;
# endif
# ifdef __MVS__
    char dir[_XOPEN_PATH_MAX];
     
    if (OPENAT_BUFFER_SIZE < bufsize)
      {
        result = malloc (bufsize);
        if (! result)
          return NULL;
      }

    strcpy (result, dir);
    result[dirlen++] = '/';
  }
#endif

  strcpy (result + dirlen, file);
  return result;
}
