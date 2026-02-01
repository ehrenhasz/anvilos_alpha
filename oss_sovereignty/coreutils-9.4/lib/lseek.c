 
#include <unistd.h>

#if defined _WIN32 && ! defined __CYGWIN__
 
 
# include <windows.h>
 
# if GNULIB_MSVC_NOTHROW
#  include "msvc-nothrow.h"
# else
#  include <io.h>
# endif
#else
# include <sys/stat.h>
#endif
#include <errno.h>

#undef lseek

off_t
rpl_lseek (int fd, off_t offset, int whence)
{
#if defined _WIN32 && ! defined __CYGWIN__
   
  HANDLE h = (HANDLE) _get_osfhandle (fd);
  if (h == INVALID_HANDLE_VALUE)
    {
      errno = EBADF;
      return -1;
    }
  if (GetFileType (h) != FILE_TYPE_DISK)
    {
      errno = ESPIPE;
      return -1;
    }
#elif defined __APPLE__ && defined __MACH__ && defined SEEK_DATA
  if (whence == SEEK_DATA)
    {
       
      off_t next_hole = lseek (fd, offset, SEEK_HOLE);
      if (next_hole < 0)
        return errno == ENXIO ? offset : next_hole;
      if (next_hole != offset)
        whence = SEEK_SET;
    }
#else
   
  struct stat statbuf;
  if (fstat (fd, &statbuf) < 0)
    return -1;
  if (!S_ISREG (statbuf.st_mode))
    {
      errno = ESPIPE;
      return -1;
    }
#endif
#if _GL_WINDOWS_64_BIT_OFF_T || (defined __MINGW32__ && defined _FILE_OFFSET_BITS && (_FILE_OFFSET_BITS == 64))
  return _lseeki64 (fd, offset, whence);
#else
  return lseek (fd, offset, whence);
#endif
}
