 
#if !_GL_CONFIG_H_INCLUDED
 #error "Please include config.h first."
#endif

#if defined _WIN32 && !defined __CYGWIN__ \
    && (defined __need_off_t || defined __need___off64_t \
        || defined __need_ssize_t || defined __need_time_t)

 

#@INCLUDE_NEXT@ @NEXT_SYS_TYPES_H@

#else
 

#ifndef _@GUARD_PREFIX@_SYS_TYPES_H

 
# define _GL_INCLUDING_SYS_TYPES_H
#@INCLUDE_NEXT@ @NEXT_SYS_TYPES_H@
# undef _GL_INCLUDING_SYS_TYPES_H

#ifndef _@GUARD_PREFIX@_SYS_TYPES_H
#define _@GUARD_PREFIX@_SYS_TYPES_H

 
#if @WINDOWS_64_BIT_OFF_T@
 
# if defined _MSC_VER
#  define off_t __int64
# else
#  define off_t long long int
# endif
 
# define _GL_WINDOWS_64_BIT_OFF_T 1
#endif

 
#if @WINDOWS_STAT_INODES@

# if @WINDOWS_STAT_INODES@ == 2
 

 
#  if !defined GNULIB_defined_dev_t
typedef unsigned long long int rpl_dev_t;
#   undef dev_t
#   define dev_t rpl_dev_t
#   define GNULIB_defined_dev_t 1
#  endif

 
#  if !defined GNULIB_defined_ino_t
 
typedef struct { unsigned long long int _gl_ino[2]; } rpl_ino_t;
#   undef ino_t
#   define ino_t rpl_ino_t
#   define GNULIB_defined_ino_t 1
#  endif

# else  

 
#  if !defined GNULIB_defined_ino_t
typedef unsigned long long int rpl_ino_t;
#   undef ino_t
#   define ino_t rpl_ino_t
#   define GNULIB_defined_ino_t 1
#  endif

# endif

 
# define _GL_WINDOWS_STAT_INODES @WINDOWS_STAT_INODES@

#endif

 
 
#if (defined _WIN32 && ! defined __CYGWIN__) && ! defined __GLIBC__
# include <stddef.h>
#endif

#endif  
#endif  
#endif  
