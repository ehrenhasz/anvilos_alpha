 

#include <config.h>

#include "isapipe.h"

#include <errno.h>

#if defined _WIN32 && ! defined __CYGWIN__
 

 
# include <windows.h>

 
# if GNULIB_MSVC_NOTHROW
#  include "msvc-nothrow.h"
# else
#  include <io.h>
# endif

int
isapipe (int fd)
{
  HANDLE h = (HANDLE) _get_osfhandle (fd);

  if (h == INVALID_HANDLE_VALUE)
    {
      errno = EBADF;
      return -1;
    }

  return (GetFileType (h) == FILE_TYPE_PIPE);
}

#else
 

# include <sys/types.h>
# include <sys/stat.h>
# include <unistd.h>

 
# ifndef PIPE_LINK_COUNT_MAX
#  define PIPE_LINK_COUNT_MAX ((nlink_t) (-1))
# endif

 

int
isapipe (int fd)
{
  nlink_t pipe_link_count_max = PIPE_LINK_COUNT_MAX;
  bool check_for_fifo = (HAVE_FIFO_PIPES == 1);
  struct stat st;
  int fstat_result = fstat (fd, &st);

  if (fstat_result != 0)
    return fstat_result;

   

  if (! ((HAVE_FIFO_PIPES == 0 || HAVE_FIFO_PIPES == 1)
         && PIPE_LINK_COUNT_MAX != (nlink_t) -1)
      && (S_ISFIFO (st.st_mode) | S_ISSOCK (st.st_mode)))
    {
      int fd_pair[2];
      int pipe_result = pipe (fd_pair);
      if (pipe_result != 0)
        return pipe_result;
      else
        {
          struct stat pipe_st;
          int fstat_pipe_result = fstat (fd_pair[0], &pipe_st);
          int fstat_pipe_errno = errno;
          close (fd_pair[0]);
          close (fd_pair[1]);
          if (fstat_pipe_result != 0)
            {
              errno = fstat_pipe_errno;
              return fstat_pipe_result;
            }
          check_for_fifo = (S_ISFIFO (pipe_st.st_mode) != 0);
          pipe_link_count_max = pipe_st.st_nlink;
        }
    }

  return
    (st.st_nlink <= pipe_link_count_max
     && (check_for_fifo ? S_ISFIFO (st.st_mode) : S_ISSOCK (st.st_mode)));
}

#endif
