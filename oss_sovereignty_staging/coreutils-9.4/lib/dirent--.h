 

#include "dirent-safer.h"

#undef opendir
#define opendir opendir_safer
#define GNULIB_defined_opendir 1
