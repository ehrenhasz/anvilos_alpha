 

#include <sys/types.h>

int open_safer (char const *, int, ...);
int creat_safer (char const *, mode_t);

#if GNULIB_OPENAT_SAFER
int openat_safer (int, char const *, int, ...);
#endif
