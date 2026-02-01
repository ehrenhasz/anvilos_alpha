 

#ifndef SOCKETS_H
#define SOCKETS_H 1

 
#if !_GL_CONFIG_H_INCLUDED
 #error "Please include config.h first."
#endif

#define SOCKETS_1_0 0x0001
#define SOCKETS_1_1 0x0101
#define SOCKETS_2_0 0x0002
#define SOCKETS_2_1 0x0102
#define SOCKETS_2_2 0x0202

int gl_sockets_startup (int version)
#ifndef WINDOWS_SOCKETS
  _GL_ATTRIBUTE_CONST
#endif
  ;

int gl_sockets_cleanup (void)
#ifndef WINDOWS_SOCKETS
  _GL_ATTRIBUTE_CONST
#endif
  ;

 
#ifdef WINDOWS_SOCKETS

# include <sys/socket.h>

# if GNULIB_MSVC_NOTHROW
#  include "msvc-nothrow.h"
# else
#  include <io.h>
# endif

static inline SOCKET
gl_fd_to_handle (int fd)
{
  return _get_osfhandle (fd);
}

#else

# define gl_fd_to_handle(x) (x)

#endif  

#endif  
