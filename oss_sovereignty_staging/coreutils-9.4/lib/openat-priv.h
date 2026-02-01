 

#ifndef _GL_HEADER_OPENAT_PRIV
#define _GL_HEADER_OPENAT_PRIV

#include <errno.h>
#include <limits.h>
#include <stdlib.h>

 
#define SAFER_ALLOCA_MAX (4096 - 64)

#define SAFER_ALLOCA(m) ((m) < SAFER_ALLOCA_MAX ? (m) : SAFER_ALLOCA_MAX)

#if defined PATH_MAX
# define OPENAT_BUFFER_SIZE SAFER_ALLOCA (PATH_MAX)
#elif defined _XOPEN_PATH_MAX
# define OPENAT_BUFFER_SIZE SAFER_ALLOCA (_XOPEN_PATH_MAX)
#else
# define OPENAT_BUFFER_SIZE SAFER_ALLOCA (1024)
#endif

char *openat_proc_name (char buf[OPENAT_BUFFER_SIZE], int fd, char const *file);

 
#define EXPECTED_ERRNO(Errno)                   \
  ((Errno) == ENOTDIR || (Errno) == ENOENT      \
   || (Errno) == EPERM || (Errno) == EACCES     \
   || (Errno) == ENOSYS           \
   || (Errno) == EOPNOTSUPP  )

 
int at_func2 (int fd1, char const *file1,
              int fd2, char const *file2,
              int (*func) (char const *file1, char const *file2));

#endif  
