 

 
#if !_GL_CONFIG_H_INCLUDED
 #error "Please include config.h first."
#endif

#include <time.h>
int fdutimens (int, char const *, struct timespec const [2]);
int utimens (char const *, struct timespec const [2]);
int lutimens (char const *, struct timespec const [2]);

#if GNULIB_FDUTIMENSAT
# include <fcntl.h>
# include <sys/stat.h>

_GL_INLINE_HEADER_BEGIN
#ifndef _GL_UTIMENS_INLINE
# define _GL_UTIMENS_INLINE _GL_INLINE
#endif

int fdutimensat (int fd, int dir, char const *name, struct timespec const [2],
                 int atflag);

 
_GL_UTIMENS_INLINE int
lutimensat (int dir, char const *file, struct timespec const times[2])
{
  return utimensat (dir, file, times, AT_SYMLINK_NOFOLLOW);
}

_GL_INLINE_HEADER_END

#endif
