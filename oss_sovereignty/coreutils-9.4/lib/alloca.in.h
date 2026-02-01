 
#ifndef _GL_ALLOCA_H
#define _GL_ALLOCA_H

 

#ifndef alloca
   
# if defined __GNUC__ && (defined _WIN32 && ! defined __CYGWIN__) && @HAVE_ALLOCA_H@
#  include_next <alloca.h>
# endif
#endif
#ifndef alloca
# if defined __GNUC__ || (__clang_major__ >= 4)
#  define alloca __builtin_alloca
# elif defined _AIX
#  define alloca __alloca
# elif defined _MSC_VER
#  include <malloc.h>
#  define alloca _alloca
# elif defined __DECC && defined __VMS
#  define alloca __ALLOCA
# elif defined __TANDEM && defined _TNS_E_TARGET
#  ifdef  __cplusplus
extern "C"
#  endif
void *_alloca (unsigned short);
#  pragma intrinsic (_alloca)
#  define alloca _alloca
# elif defined __MVS__
#  include <stdlib.h>
# else
#  include <stddef.h>
#  ifdef  __cplusplus
extern "C"
#  endif
void *alloca (size_t);
# endif
#endif

#endif  
