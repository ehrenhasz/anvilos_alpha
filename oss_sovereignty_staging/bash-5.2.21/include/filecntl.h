 

 

#if !defined (_FILECNTL_H_)
#define _FILECNTL_H_

#include <fcntl.h>

 
#if !defined (FD_CLOEXEC)
#define FD_CLOEXEC     1
#endif

#define FD_NCLOEXEC    0

#define SET_CLOSE_ON_EXEC(fd)  (fcntl ((fd), F_SETFD, FD_CLOEXEC))
#define SET_OPEN_ON_EXEC(fd)   (fcntl ((fd), F_SETFD, FD_NCLOEXEC))

 
#if !defined (O_NONBLOCK)
#  if defined (O_NDELAY)
#    define O_NONBLOCK O_NDELAY
#  else
#    define O_NONBLOCK 0
#  endif
#endif

 
#if !defined (O_BINARY)
#  define O_BINARY 0
#endif
#if !defined (O_TEXT)
#  define O_TEXT 0
#endif

#endif  
