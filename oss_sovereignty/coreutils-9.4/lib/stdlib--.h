 

#include <stdlib.h>
#include "stdlib-safer.h"

#undef mkstemp
#define mkstemp mkstemp_safer

#if GNULIB_MKOSTEMP
# define mkostemp mkostemp_safer
#endif

#if GNULIB_MKOSTEMPS
# define mkostemps mkostemps_safer
#endif

#if GNULIB_MKSTEMPS
# define mkstemps mkstemps_safer
#endif
