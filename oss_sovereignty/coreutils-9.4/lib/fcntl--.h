 

#include <fcntl.h>
#include "fcntl-safer.h"

#undef open
#define open open_safer

#undef creat
#define creat creat_safer

#if GNULIB_OPENAT_SAFER
# undef openat
# define openat openat_safer
#endif
