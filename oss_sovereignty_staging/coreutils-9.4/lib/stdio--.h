 

#include <stdio.h>
#include "stdio-safer.h"

#if GNULIB_FOPEN_SAFER
# undef fopen
# define fopen fopen_safer
#endif

#if GNULIB_FREOPEN_SAFER
# undef freopen
# define freopen freopen_safer
#endif

#if GNULIB_TMPFILE_SAFER
# undef tmpfile
# define tmpfile tmpfile_safer
#endif

#if GNULIB_POPEN_SAFER
# undef popen
# define popen popen_safer
#endif
