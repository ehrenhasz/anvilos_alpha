 
#if !_GL_CONFIG_H_INCLUDED
 #error "Please include config.h first."
#endif

#include <signal.h>

_GL_INLINE_HEADER_BEGIN
#ifndef SIG_HANDLER_INLINE
# define SIG_HANDLER_INLINE _GL_INLINE
#endif

 
typedef void (*sa_handler_t) (int);

 
SIG_HANDLER_INLINE sa_handler_t _GL_ATTRIBUTE_PURE
get_handler (struct sigaction const *a)
{
   
  return a->sa_handler;
}

_GL_INLINE_HEADER_END

#endif  
