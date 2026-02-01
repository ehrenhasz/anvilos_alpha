 

 
#include <stdio.h>
#ifndef RENAME_NOREPLACE
# define RENAME_NOREPLACE  (1 << 0)
# define RENAME_EXCHANGE   (1 << 1)
# define RENAME_WHITEOUT   (1 << 2)
#endif

extern int renameatu (int, char const *, int, char const *, unsigned int);
