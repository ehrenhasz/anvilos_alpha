 
#if @HAVE_SYS_IOCTL_H@
# @INCLUDE_NEXT@ @NEXT_SYS_IOCTL_H@
#endif

#ifndef _@GUARD_PREFIX@_SYS_IOCTL_H
#define _@GUARD_PREFIX@_SYS_IOCTL_H

 
#if !_GL_CONFIG_H_INCLUDED
 #error "Please include config.h first."
#endif

 
#ifndef __GLIBC__
# include <unistd.h>
#endif

 

 


 

#if @GNULIB_IOCTL@
# if @REPLACE_IOCTL@
#  if !(defined __cplusplus && defined GNULIB_NAMESPACE)
#   undef ioctl
#   define ioctl rpl_ioctl
#  endif
_GL_FUNCDECL_RPL (ioctl, int,
                  (int fd, int request, ...  ));
_GL_CXXALIAS_RPL (ioctl, int,
                  (int fd, int request, ...  ));
# else
#  if @SYS_IOCTL_H_HAVE_WINSOCK2_H@ || 1
_GL_FUNCDECL_SYS (ioctl, int,
                  (int fd, int request, ...  ));
#  endif
_GL_CXXALIAS_SYS (ioctl, int,
                  (int fd, int request, ...  ));
# endif
# if __GLIBC__ >= 2
_GL_CXXALIASWARN (ioctl);
# endif
#elif @SYS_IOCTL_H_HAVE_WINSOCK2_H_AND_USE_SOCKETS@
# undef ioctl
# define ioctl ioctl_used_without_requesting_gnulib_module_ioctl
#elif defined GNULIB_POSIXCHECK
# undef ioctl
# if HAVE_RAW_DECL_IOCTL
_GL_WARN_ON_USE (ioctl, "ioctl does not portably work on sockets - "
                 "use gnulib module ioctl for portability");
# endif
#endif


#endif  
#endif  
