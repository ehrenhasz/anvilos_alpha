 

#include <config.h>

 

#if defined _AIX || defined __sun || defined __APPLE__ || \
    defined __linux__ || defined __ANDROID__
# define IOPOLL_USES_POLL 1
   
# if defined HAVE_POLL
#  error "gnulib's poll() replacement is currently incompatible"
# endif
#endif

#if IOPOLL_USES_POLL
# include <poll.h>
#else
# include <sys/select.h>
#endif

#include "system.h"
#include "assure.h"
#include "iopoll.h"
#include "isapipe.h"


 

static int
iopoll_internal (int fdin, int fdout, bool block, bool broken_output)
{
  affirm (fdin != -1 || fdout != -1);

#if IOPOLL_USES_POLL
  struct pollfd pfds[2] = {   
    { .fd = fdin,  .events = POLLIN | POLLRDBAND, .revents = 0 },
    { .fd = fdout, .events = POLLRDBAND, .revents = 0 },
  };
  int check_out_events = POLLERR | POLLHUP | POLLNVAL;
  int ret = 0;

  if (! broken_output)
    {
      pfds[0].events = pfds[1].events = POLLOUT;
      check_out_events = POLLOUT;
    }

  while (0 <= ret || errno == EINTR)
    {
      ret = poll (pfds, 2, block ? -1 : 0);

      if (ret < 0)
        continue;
      if (ret == 0 && ! block)
        return 0;
      affirm (0 < ret);
      if (pfds[0].revents)  
        return 0;           
      if (pfds[1].revents & check_out_events)
        return broken_output ? IOPOLL_BROKEN_OUTPUT : 0;
    }

#else   

  int nfds = (fdin > fdout ? fdin : fdout) + 1;
  int ret = 0;

  if (FD_SETSIZE < nfds)
    {
      errno = EINVAL;
      ret = -1;
    }

   
  while (0 <= ret || errno == EINTR)
    {
      fd_set fds;
      FD_ZERO (&fds);
      if (0 <= fdin)
        FD_SET (fdin, &fds);
      if (0 <= fdout)
        FD_SET (fdout, &fds);

      struct timeval delay = { .tv_sec = 0, .tv_usec = 0 };
      ret = select (nfds,
                    broken_output ? &fds : nullptr,
                    broken_output ? nullptr : &fds,
                    nullptr, block ? nullptr : &delay);

      if (ret < 0)
        continue;
      if (ret == 0 && ! block)
        return 0;
      affirm (0 < ret);
      if (0 <= fdin && FD_ISSET (fdin, &fds))     
        return 0;           
      if (0 <= fdout && FD_ISSET (fdout, &fds))   
        return broken_output ? IOPOLL_BROKEN_OUTPUT : 0;
    }

#endif
  return IOPOLL_ERROR;
}

extern int
iopoll (int fdin, int fdout, bool block)
{
  return iopoll_internal (fdin, fdout, block, true);
}



 

extern bool
iopoll_input_ok (int fdin)
{
  struct stat st;
  bool always_ready = fstat (fdin, &st) == 0
                      && (S_ISREG (st.st_mode)
                          || S_ISBLK (st.st_mode));
  return ! always_ready;
}

 

extern bool
iopoll_output_ok (int fdout)
{
  return isapipe (fdout) > 0;
}

#ifdef EWOULDBLOCK
# define IS_EAGAIN(errcode) ((errcode) == EAGAIN || (errcode) == EWOULDBLOCK)
#else
# define IS_EAGAIN(errcode) ((errcode) == EAGAIN)
#endif

 

static bool
fwait_for_nonblocking_write (FILE *f)
{
  if (! IS_EAGAIN (errno))
     
    return false;

  int fd = fileno (f);
  if (fd == -1)
    goto fail;

   
  if (iopoll_internal (-1, fd, true, false) != 0)
    goto fail;

   
  clearerr (f);
  return true;

fail:
  errno = EAGAIN;
  return false;
}


 

extern bool
fclose_wait (FILE *f)
{
  for (;;)
    {
      if (fflush (f) == 0)
        break;

      if (! fwait_for_nonblocking_write (f))
        break;
    }

  return fclose (f) == 0;
}


 

extern bool
fwrite_wait (char const *buf, ssize_t size, FILE *f)
{
  for (;;)
    {
      const size_t written = fwrite (buf, 1, size, f);
      size -= written;
      affirm (size >= 0);
      if (size <= 0)   
        return true;

      if (! fwait_for_nonblocking_write (f))
        return false;

      buf += written;
    }
}
