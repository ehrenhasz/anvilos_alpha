 

#include <config.h>

#include <fcntl.h>

 
int o = (O_DIRECT | O_DIRECTORY | O_DSYNC | O_IGNORE_CTTY | O_NDELAY | O_NOATIME
         | O_NONBLOCK | O_NOCTTY | O_NOFOLLOW | O_NOLINK | O_NOLINKS | O_NOTRANS
         | O_RSYNC | O_SYNC | O_TTY_INIT | O_BINARY | O_TEXT);

 
int sk[] = { SEEK_CUR, SEEK_END, SEEK_SET };

 
int i = FD_CLOEXEC;

 
pid_t t1;
off_t t2;
mode_t t3;

int
main (void)
{
   
  switch (0)
    {
    case SEEK_CUR:
    case SEEK_END:
    case SEEK_SET:
      ;
    }

   
  switch (O_RDONLY)
    {
       
    case O_RDONLY:
    case O_WRONLY:
    case O_RDWR:
#if O_EXEC && O_EXEC != O_RDONLY
    case O_EXEC:
#endif
#if O_SEARCH && O_EXEC != O_SEARCH && O_SEARCH != O_RDONLY
    case O_SEARCH:
#endif
      i = ! (~O_ACCMODE & (O_RDONLY | O_WRONLY | O_RDWR | O_EXEC | O_SEARCH));
      break;

       
    case O_CREAT:
    case O_EXCL:
    case O_TRUNC:
    case O_APPEND:
      break;

       
#if O_CLOEXEC
    case O_CLOEXEC:
#endif
#if O_DIRECT
    case O_DIRECT:
#endif
#if O_DIRECTORY
    case O_DIRECTORY:
#endif
#if O_DSYNC
    case O_DSYNC:
#endif
#if O_IGNORE_CTTY
    case O_IGNORE_CTTY:
#endif
#if O_NOATIME
    case O_NOATIME:
#endif
#if O_NONBLOCK
    case O_NONBLOCK:
#endif
#if O_NOCTTY
    case O_NOCTTY:
#endif
#if O_NOFOLLOW
    case O_NOFOLLOW:
#endif
#if O_NOLINK
    case O_NOLINK:
#endif
#if O_NOLINKS
    case O_NOLINKS:
#endif
#if O_NOTRANS
    case O_NOTRANS:
#endif
#if O_RSYNC && O_RSYNC != O_DSYNC
    case O_RSYNC:
#endif
#if O_SYNC && O_SYNC != O_DSYNC && O_SYNC != O_RSYNC
    case O_SYNC:
#endif
#if O_TTY_INIT
    case O_TTY_INIT:
#endif
#if O_BINARY
    case O_BINARY:
#endif
#if O_TEXT
    case O_TEXT:
#endif
      ;
    }

  return !i;
}
