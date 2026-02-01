 
#if !_GL_CONFIG_H_INCLUDED
 #error "Please include config.h first."
#endif

 
#if @HAVE_SCHED_H@
# if @HAVE_SYS_CDEFS_H@
#  include <sys/cdefs.h>
# endif
# @INCLUDE_NEXT@ @NEXT_SCHED_H@
#endif

#ifndef _@GUARD_PREFIX@_SCHED_H
#define _@GUARD_PREFIX@_SCHED_H

 
#if !_GL_CONFIG_H_INCLUDED
 #error "Please include config.h first."
#endif

 
#include <sys/types.h>

#ifdef __KLIBC__
 
# include <spawn.h>
#endif

#ifdef __VMS
 
# include <pthread.h>
#endif

 

 

#if !@HAVE_STRUCT_SCHED_PARAM@

# if !GNULIB_defined_struct_sched_param
struct sched_param
{
  int sched_priority;
};
#  define GNULIB_defined_struct_sched_param 1
# endif

#endif

#if !(defined SCHED_FIFO && defined SCHED_RR && defined SCHED_OTHER)
# define SCHED_FIFO   1
# define SCHED_RR     2
# define SCHED_OTHER  0
#endif

#if @GNULIB_SCHED_YIELD@
# if @REPLACE_SCHED_YIELD@
#  if !(defined __cplusplus && defined GNULIB_NAMESPACE)
#   undef sched_yield
#   define sched_yield rpl_sched_yield
#  endif
_GL_FUNCDECL_RPL (sched_yield, int, (void));
_GL_CXXALIAS_RPL (sched_yield, int, (void));
# else
#  if !@HAVE_SCHED_YIELD@
_GL_FUNCDECL_SYS (sched_yield, int, (void));
#  endif
_GL_CXXALIAS_SYS (sched_yield, int, (void));
# endif
# if __GLIBC__ >= 2
_GL_CXXALIASWARN (sched_yield);
# endif
#elif defined GNULIB_POSIXCHECK
# undef sched_yield
# if HAVE_RAW_DECL_SCHED_YIELD
_GL_WARN_ON_USE (sched_yield, "sched_yield is not portable - "
                 "use gnulib module sched_yield for portability");
# endif
#endif

#endif  
#endif  
