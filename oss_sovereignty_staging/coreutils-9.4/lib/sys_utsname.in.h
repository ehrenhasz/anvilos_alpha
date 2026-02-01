 
#if @HAVE_SYS_UTSNAME_H@

 
# if defined __minix && !defined __GLIBC__
#  include <stddef.h>
# endif

# @INCLUDE_NEXT@ @NEXT_SYS_UTSNAME_H@

#endif

#ifndef _@GUARD_PREFIX@_SYS_UTSNAME_H
#define _@GUARD_PREFIX@_SYS_UTSNAME_H

 
#if !_GL_CONFIG_H_INCLUDED
 #error "Please include config.h first."
#endif

 

 


#ifdef __cplusplus
extern "C" {
#endif

#if !@HAVE_STRUCT_UTSNAME@
 
# define _UTSNAME_LENGTH 256

# ifndef _UTSNAME_NODENAME_LENGTH
#  define _UTSNAME_NODENAME_LENGTH _UTSNAME_LENGTH
# endif
# ifndef _UTSNAME_SYSNAME_LENGTH
#  define _UTSNAME_SYSNAME_LENGTH _UTSNAME_LENGTH
# endif
# ifndef _UTSNAME_RELEASE_LENGTH
#  define _UTSNAME_RELEASE_LENGTH _UTSNAME_LENGTH
# endif
# ifndef _UTSNAME_VERSION_LENGTH
#  define _UTSNAME_VERSION_LENGTH _UTSNAME_LENGTH
# endif
# ifndef _UTSNAME_MACHINE_LENGTH
#  define _UTSNAME_MACHINE_LENGTH _UTSNAME_LENGTH
# endif

# if !GNULIB_defined_struct_utsname
 
struct utsname
  {
     
    char nodename[_UTSNAME_NODENAME_LENGTH];

     
    char sysname[_UTSNAME_SYSNAME_LENGTH];
     
    char release[_UTSNAME_RELEASE_LENGTH];
     
    char version[_UTSNAME_VERSION_LENGTH];

     
    char machine[_UTSNAME_MACHINE_LENGTH];
  };
#  define GNULIB_defined_struct_utsname 1
# endif

#endif  


#if @GNULIB_UNAME@
# if !@HAVE_UNAME@
extern int uname (struct utsname *buf) _GL_ARG_NONNULL ((1));
# endif
#elif defined GNULIB_POSIXCHECK
# undef uname
# if HAVE_RAW_DECL_UNAME
_GL_WARN_ON_USE (uname, "uname is unportable - "
                 "use gnulib module uname for portability");
# endif
#endif


#ifdef __cplusplus
}
#endif


#endif  
#endif  
