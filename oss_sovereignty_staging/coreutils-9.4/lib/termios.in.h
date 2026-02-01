 
#if defined __hpux && defined __cplusplus
# include <sys/types.h>
# include <sys/ioctl.h>
extern "C" {
# include <sys/termio.h>
}
#endif

 
#if @HAVE_TERMIOS_H@
# @INCLUDE_NEXT@ @NEXT_TERMIOS_H@
#endif

#ifndef _@GUARD_PREFIX@_TERMIOS_H
#define _@GUARD_PREFIX@_TERMIOS_H

 
#if !_GL_CONFIG_H_INCLUDED
 #error "Please include config.h first."
#endif

 
#include <sys/types.h>

 

 


 

#if @GNULIB_TCGETSID@
 
# if !@HAVE_DECL_TCGETSID@
_GL_FUNCDECL_SYS (tcgetsid, pid_t, (int fd));
# endif
_GL_CXXALIAS_SYS (tcgetsid, pid_t, (int fd));
_GL_CXXALIASWARN (tcgetsid);
#elif defined GNULIB_POSIXCHECK
# undef tcgetsid
# if HAVE_RAW_DECL_TCGETSID
_GL_WARN_ON_USE (tcgetsid, "tcgetsid is not portable - "
                 "use gnulib module tcgetsid for portability");
# endif
#endif


#endif  
#endif  
