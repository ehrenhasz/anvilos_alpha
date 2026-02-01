 
# include <sys/types.h>
# include <sys/time.h>

 
# @INCLUDE_NEXT@ @NEXT_SYS_RESOURCE_H@

#endif

#ifndef _@GUARD_PREFIX@_SYS_RESOURCE_H
#define _@GUARD_PREFIX@_SYS_RESOURCE_H

 
#if !_GL_CONFIG_H_INCLUDED
 #error "Please include config.h first."
#endif

#if !@HAVE_SYS_RESOURCE_H@
 

 
# include <sys/time.h>

 
# define RUSAGE_SELF 0
# define RUSAGE_CHILDREN -1

# ifdef __cplusplus
extern "C" {
# endif

# if !GNULIB_defined_struct_rusage
 
struct rusage
{
  struct timeval ru_utime;       
  struct timeval ru_stime;       
  long ru_maxrss;
  long ru_ixrss;
  long ru_idrss;
  long ru_isrss;
  long ru_minflt;
  long ru_majflt;
  long ru_nswap;
  long ru_inblock;
  long ru_oublock;
  long ru_msgsnd;
  long ru_msgrcv;
  long ru_nsignals;
  long ru_nvcsw;
  long ru_nivcsw;
};
#  define GNULIB_defined_struct_rusage 1
# endif

# ifdef __cplusplus
}
# endif

#else

# ifdef __VMS                       
 
#  ifndef RUSAGE_SELF
#   define RUSAGE_SELF 0
#  endif
#  ifndef RUSAGE_CHILDREN
#   define RUSAGE_CHILDREN -1
#  endif
# endif

#endif

 

 

 


 


#if @GNULIB_GETRUSAGE@
# if !@HAVE_GETRUSAGE@
_GL_FUNCDECL_SYS (getrusage, int, (int who, struct rusage *usage_p)
                                  _GL_ARG_NONNULL ((2)));
# endif
_GL_CXXALIAS_SYS (getrusage, int, (int who, struct rusage *usage_p));
_GL_CXXALIASWARN (getrusage);
#elif defined GNULIB_POSIXCHECK
# undef getrusage
# if HAVE_RAW_DECL_GETRUSAGE
_GL_WARN_ON_USE (getrusage, "getrusage is unportable - "
                 "use gnulib module getrusage for portability");
# endif
#endif


#endif  
#endif  
