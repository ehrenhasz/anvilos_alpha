 

#ifndef _GL_HEADER_OPENAT
#define _GL_HEADER_OPENAT

 
#if !_GL_CONFIG_H_INCLUDED
 #error "Please include config.h first."
#endif

#include <fcntl.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>

_GL_INLINE_HEADER_BEGIN

#if !HAVE_OPENAT

int openat_permissive (int fd, char const *file, int flags, mode_t mode,
                       int *cwd_errno);
bool openat_needs_fchdir (void);

#else

# define openat_permissive(Fd, File, Flags, Mode, Cwd_errno) \
    openat (Fd, File, Flags, Mode)
# define openat_needs_fchdir() false

#endif

_Noreturn void openat_restore_fail (int);
_Noreturn void openat_save_fail (int);

 

#if GNULIB_CHOWNAT

# ifndef CHOWNAT_INLINE
#  define CHOWNAT_INLINE _GL_INLINE
# endif

CHOWNAT_INLINE int
chownat (int fd, char const *file, uid_t owner, gid_t group)
{
  return fchownat (fd, file, owner, group, 0);
}

CHOWNAT_INLINE int
lchownat (int fd, char const *file, uid_t owner, gid_t group)
{
  return fchownat (fd, file, owner, group, AT_SYMLINK_NOFOLLOW);
}

#endif

#if GNULIB_CHMODAT

# ifndef CHMODAT_INLINE
#  define CHMODAT_INLINE _GL_INLINE
# endif

CHMODAT_INLINE int
chmodat (int fd, char const *file, mode_t mode)
{
  return fchmodat (fd, file, mode, 0);
}

CHMODAT_INLINE int
lchmodat (int fd, char const *file, mode_t mode)
{
  return fchmodat (fd, file, mode, AT_SYMLINK_NOFOLLOW);
}

#endif

#if GNULIB_STATAT

# ifndef STATAT_INLINE
#  define STATAT_INLINE _GL_INLINE
# endif

_GL_ATTRIBUTE_DEPRECATED
STATAT_INLINE int
statat (int fd, char const *name, struct stat *st)
{
  return fstatat (fd, name, st, 0);
}

_GL_ATTRIBUTE_DEPRECATED
STATAT_INLINE int
lstatat (int fd, char const *name, struct stat *st)
{
  return fstatat (fd, name, st, AT_SYMLINK_NOFOLLOW);
}

#endif

 

_GL_INLINE_HEADER_END

#endif  
