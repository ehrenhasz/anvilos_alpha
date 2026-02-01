 

#include <config.h>

 
#include "sockets.h"

#if WINDOWS_SOCKETS

 
# include <sys/socket.h>

# include "fd-hook.h"
# if GNULIB_MSVC_NOTHROW
#  include "msvc-nothrow.h"
# else
#  include <io.h>
# endif

 
# include "w32sock.h"

static int
close_fd_maybe_socket (const struct fd_hook *remaining_list,
                       gl_close_fn primary,
                       int fd)
{
   
  SOCKET sock;
  WSANETWORKEVENTS ev;

   
  sock = FD_TO_SOCKET (fd);
  ev.lNetworkEvents = 0xDEADBEEF;
  WSAEnumNetworkEvents (sock, NULL, &ev);
  if (ev.lNetworkEvents != 0xDEADBEEF)
    {
       
       
      if (closesocket (sock))
        {
          set_winsock_errno ();
          return -1;
        }
      else
        {
           
          _close (fd);
          return 0;
        }
    }
  else
     
    return execute_close_hooks (remaining_list, primary, fd);
}

static int
ioctl_fd_maybe_socket (const struct fd_hook *remaining_list,
                       gl_ioctl_fn primary,
                       int fd, int request, void *arg)
{
  SOCKET sock;
  WSANETWORKEVENTS ev;

   
  sock = FD_TO_SOCKET (fd);
  ev.lNetworkEvents = 0xDEADBEEF;
  WSAEnumNetworkEvents (sock, NULL, &ev);
  if (ev.lNetworkEvents != 0xDEADBEEF)
    {
       
      if (ioctlsocket (sock, request, arg) < 0)
        {
          set_winsock_errno ();
          return -1;
        }
      else
        return 0;
    }
  else
     
    return execute_ioctl_hooks (remaining_list, primary, fd, request, arg);
}

static struct fd_hook fd_sockets_hook;

static int initialized_sockets_version  ;

#endif  

int
gl_sockets_startup (_GL_UNUSED int version)
{
#if WINDOWS_SOCKETS
  if (version > initialized_sockets_version)
    {
      WSADATA data;
      int err;

      err = WSAStartup (version, &data);
      if (err != 0)
        return 1;

      if (data.wVersion != version)
        {
          WSACleanup ();
          return 2;
        }

      if (initialized_sockets_version == 0)
        register_fd_hook (close_fd_maybe_socket, ioctl_fd_maybe_socket,
                          &fd_sockets_hook);

      initialized_sockets_version = version;
    }
#endif

  return 0;
}

int
gl_sockets_cleanup (void)
{
#if WINDOWS_SOCKETS
  int err;

  initialized_sockets_version = 0;

  unregister_fd_hook (&fd_sockets_hook);

  err = WSACleanup ();
  if (err != 0)
    return 1;
#endif

  return 0;
}
