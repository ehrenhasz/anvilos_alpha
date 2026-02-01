 
#if @HAVE_GETOPT_H@
# define _GL_SYSTEM_GETOPT
# @INCLUDE_NEXT@ @NEXT_GETOPT_H@
# undef _GL_SYSTEM_GETOPT
#endif

#define _@GUARD_PREFIX@_GETOPT_H 1

 
#if defined __GETOPT_PREFIX
# if !@HAVE_GETOPT_H@
#  define __need_system_stdlib_h
#  include <stdlib.h>
#  undef __need_system_stdlib_h
#  include <stdio.h>
#  include <unistd.h>
# endif
#endif

 

#include <getopt-cdefs.h>
#include <getopt-pfx-core.h>
#include <getopt-pfx-ext.h>

#endif  
