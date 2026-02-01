 

 
#ifdef __GETOPT_PREFIX
# ifndef __GETOPT_ID
#  define __GETOPT_CONCAT(x, y) x ## y
#  define __GETOPT_XCONCAT(x, y) __GETOPT_CONCAT (x, y)
#  define __GETOPT_ID(y) __GETOPT_XCONCAT (__GETOPT_PREFIX, y)
# endif
# undef getopt_long
# undef getopt_long_only
# undef option
# undef _getopt_internal
# define getopt_long __GETOPT_ID (getopt_long)
# define getopt_long_only __GETOPT_ID (getopt_long_only)
# define option __GETOPT_ID (option)
# define _getopt_internal __GETOPT_ID (getopt_internal)

 
# undef _GETOPT_EXT_H
#endif

 
#ifndef __getopt_argv_const
# if defined __GETOPT_PREFIX
#  define __getopt_argv_const  
# else
#  define __getopt_argv_const const
# endif
#endif

#include <getopt-ext.h>

#endif  
