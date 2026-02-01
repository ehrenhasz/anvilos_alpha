 
# ifndef __GLIBC__
#  include <stddef.h>
# endif
 
# if defined __APPLE__ && defined __MACH__                   
#  include <sys/types.h>
#  include <stdlib.h>
# endif

 
# @INCLUDE_NEXT@ @NEXT_SYS_RANDOM_H@

#endif

#ifndef _@GUARD_PREFIX@_SYS_RANDOM_H
#define _@GUARD_PREFIX@_SYS_RANDOM_H

 
#if !_GL_CONFIG_H_INCLUDED
 #error "Please include config.h first."
#endif

#include <sys/types.h>

 
#ifndef GRND_NONBLOCK
# define GRND_NONBLOCK 1
# define GRND_RANDOM 2
#endif

 

 

 


 


#if @GNULIB_GETRANDOM@
 
# if @REPLACE_GETRANDOM@
#  if !(defined __cplusplus && defined GNULIB_NAMESPACE)
#   undef getrandom
#   define getrandom rpl_getrandom
#  endif
_GL_FUNCDECL_RPL (getrandom, ssize_t,
                  (void *buffer, size_t length, unsigned int flags)
                  _GL_ARG_NONNULL ((1)));
_GL_CXXALIAS_RPL (getrandom, ssize_t,
                  (void *buffer, size_t length, unsigned int flags));
# else
#  if !@HAVE_GETRANDOM@
_GL_FUNCDECL_SYS (getrandom, ssize_t,
                  (void *buffer, size_t length, unsigned int flags)
                  _GL_ARG_NONNULL ((1)));
#  endif
_GL_CXXALIAS_SYS (getrandom, ssize_t,
                  (void *buffer, size_t length, unsigned int flags));
# endif
# if __GLIBC__ + (__GLIBC_MINOR__ >= 25) > 2
_GL_CXXALIASWARN (getrandom);
# endif
#elif defined GNULIB_POSIXCHECK
# undef getrandom
# if HAVE_RAW_DECL_GETRANDOM
_GL_WARN_ON_USE (getrandom, "getrandom is unportable - "
                 "use gnulib module getrandom for portability");
# endif
#endif


#endif  
#endif  
