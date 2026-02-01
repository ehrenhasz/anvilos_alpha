 

 
#ifdef __GETOPT_PREFIX
# ifndef __GETOPT_ID
#  define __GETOPT_CONCAT(x, y) x ## y
#  define __GETOPT_XCONCAT(x, y) __GETOPT_CONCAT (x, y)
#  define __GETOPT_ID(y) __GETOPT_XCONCAT (__GETOPT_PREFIX, y)
# endif
# undef getopt
# undef optarg
# undef opterr
# undef optind
# undef optopt
# define getopt __GETOPT_ID (getopt)
# define optarg __GETOPT_ID (optarg)
# define opterr __GETOPT_ID (opterr)
# define optind __GETOPT_ID (optind)
# define optopt __GETOPT_ID (optopt)

 
# undef _GETOPT_CORE_H
#endif

#include <getopt-core.h>

#endif  
