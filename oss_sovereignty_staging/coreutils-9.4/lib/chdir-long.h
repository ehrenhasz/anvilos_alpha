 

#include <unistd.h>
#include <limits.h>

#include "pathmax.h"

 
#ifndef PATH_MAX
# define chdir_long(Dir) chdir (Dir)
#else
int chdir_long (char *dir);
#endif
