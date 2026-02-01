 

#include <unistd.h>
#include "unistd-safer.h"

#undef dup
#define dup dup_safer

#undef pipe
#define pipe pipe_safer

#if GNULIB_PIPE2_SAFER
# undef pipe2
# define pipe2 pipe2_safer
#endif
