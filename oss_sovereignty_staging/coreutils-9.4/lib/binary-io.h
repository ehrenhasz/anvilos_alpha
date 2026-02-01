 
#if !_GL_CONFIG_H_INCLUDED
 #error "Please include config.h first."
#endif

 
#include <fcntl.h>

 
#include <stdio.h>

_GL_INLINE_HEADER_BEGIN
#ifndef BINARY_IO_INLINE
# define BINARY_IO_INLINE _GL_INLINE
#endif

#if O_BINARY
# if defined __EMX__ || defined __DJGPP__ || defined __CYGWIN__
#  include <io.h>  
#  define __gl_setmode setmode
# else
#  define __gl_setmode _setmode
#  undef fileno
#  define fileno _fileno
# endif
#else
   
   
BINARY_IO_INLINE int
__gl_setmode (_GL_UNUSED int fd, _GL_UNUSED int mode)
{
  return O_BINARY;
}
#endif

 

#if defined __DJGPP__ || defined __EMX__
extern int set_binary_mode (int fd, int mode);
#else
BINARY_IO_INLINE int
set_binary_mode (int fd, int mode)
{
  return __gl_setmode (fd, mode);
}
#endif

 
#define SET_BINARY(fd) ((void) set_binary_mode (fd, O_BINARY))

_GL_INLINE_HEADER_END

#endif  
