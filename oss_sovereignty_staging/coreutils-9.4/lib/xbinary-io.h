 
#if !_GL_CONFIG_H_INCLUDED
 #error "Please include config.h first."
#endif

#include "binary-io.h"

_GL_INLINE_HEADER_BEGIN
#ifndef XBINARY_IO_INLINE
# define XBINARY_IO_INLINE _GL_INLINE
#endif

#if O_BINARY
extern _Noreturn void xset_binary_mode_error (void);
#else
XBINARY_IO_INLINE void xset_binary_mode_error (void) {}
#endif

 

XBINARY_IO_INLINE void
xset_binary_mode (int fd, int mode)
{
  if (set_binary_mode (fd, mode) < 0)
    xset_binary_mode_error ();
}

_GL_INLINE_HEADER_END

#endif  
