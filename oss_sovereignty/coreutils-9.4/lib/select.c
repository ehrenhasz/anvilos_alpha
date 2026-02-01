 
#include <sys/select.h>

#if defined _WIN32 && ! defined __CYGWIN__
 

#include <alloca.h>
#include <assert.h>
#include <sys/types.h>
#include <errno.h>
#include <limits.h>

#include <winsock2.h>
#include <windows.h>
#include <io.h>
#include <stdio.h>
#include <conio.h>
#include <time.h>

 
#include <sys/time.h>

#if GNULIB_MSVC_NOTHROW
# include "msvc-nothrow.h"
#else
# include <io.h>
#endif

#undef select

 
#undef GetModuleHandle
#define GetModuleHandle GetModuleHandleA
#undef PeekConsoleInput
#define PeekConsoleInput PeekConsoleInputA
#undef CreateEvent
#define CreateEvent CreateEventA
#undef PeekMessage
#define PeekMessage PeekMessageA
#undef DispatchMessage
#define DispatchMessage DispatchMessageA

 
#define GetProcAddress \
  (void *) GetProcAddress

struct bitset {
  unsigned char in[FD_SETSIZE / CHAR_BIT];
  unsigned char out[FD_SETSIZE / CHAR_BIT];
};

 
typedef struct _FILE_PIPE_LOCAL_INFORMATION {
  ULONG NamedPipeType;
  ULONG NamedPipeConfiguration;
  ULONG MaximumInstances;
  ULONG CurrentInstances;
  ULONG InboundQuota;
  ULONG ReadDataAvailable;
  ULONG OutboundQuota;
  ULONG WriteQuotaAvailable;
  ULONG NamedPipeState;
  ULONG NamedPipeEnd;
} FILE_PIPE_LOCAL_INFORMATION, *PFILE_PIPE_LOCAL_INFORMATION;

typedef struct _IO_STATUS_BLOCK
{
  union {
    DWORD Status;
    PVOID Pointer;
  } u;
  ULONG_PTR Information;
} IO_STATUS_BLOCK, *PIO_STATUS_BLOCK;

typedef enum _FILE_INFORMATION_CLASS {
  FilePipeLocalInformation = 24
} FILE_INFORMATION_CLASS, *PFILE_INFORMATION_CLASS;

typedef DWORD (WINAPI *PNtQueryInformationFile)
         (HANDLE, IO_STATUS_BLOCK *, VOID *, ULONG, FILE_INFORMATION_CLASS);

#ifndef PIPE_BUF
#define PIPE_BUF        512
#endif

static BOOL IsConsoleHandle (HANDLE h)
{
  DWORD mode;
  return GetConsoleMode (h, &mode) != 0;
}

static BOOL
IsSocketHandle (HANDLE h)
{
  WSANETWORKEVENTS ev;

  if (IsConsoleHandle (h))
    return FALSE;

   
  ev.lNetworkEvents = 0xDEADBEEF;
  WSAEnumNetworkEvents ((SOCKET) h, NULL, &ev);
  return ev.lNetworkEvents != 0xDEADBEEF;
}

 

static int
windows_poll_handle (HANDLE h, int fd,
                     struct bitset *rbits,
                     struct bitset *wbits,
                     struct bitset *xbits)
{
  BOOL read, write, except;
  int i, ret;
  INPUT_RECORD *irbuffer;
  DWORD avail, nbuffer;
  BOOL bRet;
  IO_STATUS_BLOCK iosb;
  FILE_PIPE_LOCAL_INFORMATION fpli;
  static PNtQueryInformationFile NtQueryInformationFile;
  static BOOL once_only;

  read = write = except = FALSE;
  switch (GetFileType (h))
    {
    case FILE_TYPE_DISK:
      read = TRUE;
      write = TRUE;
      break;

    case FILE_TYPE_PIPE:
      if (!once_only)
        {
          NtQueryInformationFile = (PNtQueryInformationFile)
            GetProcAddress (GetModuleHandle ("ntdll.dll"),
                            "NtQueryInformationFile");
          once_only = TRUE;
        }

      if (PeekNamedPipe (h, NULL, 0, NULL, &avail, NULL) != 0)
        {
          if (avail)
            read = TRUE;
        }
      else if (GetLastError () == ERROR_BROKEN_PIPE)
        ;

      else
        {
           
          memset (&iosb, 0, sizeof (iosb));
          memset (&fpli, 0, sizeof (fpli));

          if (!NtQueryInformationFile
              || NtQueryInformationFile (h, &iosb, &fpli, sizeof (fpli),
                                         FilePipeLocalInformation)
              || fpli.WriteQuotaAvailable >= PIPE_BUF
              || (fpli.OutboundQuota < PIPE_BUF &&
                  fpli.WriteQuotaAvailable == fpli.OutboundQuota))
            write = TRUE;
        }
      break;

    case FILE_TYPE_CHAR:
      write = TRUE;
      if (!(rbits->in[fd / CHAR_BIT] & (1 << (fd & (CHAR_BIT - 1)))))
        break;

      ret = WaitForSingleObject (h, 0);
      if (ret == WAIT_OBJECT_0)
        {
          if (!IsConsoleHandle (h))
            {
              read = TRUE;
              break;
            }

          nbuffer = avail = 0;
          bRet = GetNumberOfConsoleInputEvents (h, &nbuffer);

           
          assert (bRet);
          if (nbuffer == 0)
            {
              except = TRUE;
              break;
            }

          irbuffer = (INPUT_RECORD *) alloca (nbuffer * sizeof (INPUT_RECORD));
          bRet = PeekConsoleInput (h, irbuffer, nbuffer, &avail);
          if (!bRet || avail == 0)
            {
              except = TRUE;
              break;
            }

          for (i = 0; i < avail; i++)
            if (irbuffer[i].EventType == KEY_EVENT)
              read = TRUE;
        }
      break;

    default:
      ret = WaitForSingleObject (h, 0);
      write = TRUE;
      if (ret == WAIT_OBJECT_0)
        read = TRUE;

      break;
    }

  ret = 0;
  if (read && (rbits->in[fd / CHAR_BIT] & (1 << (fd & (CHAR_BIT - 1)))))
    {
      rbits->out[fd / CHAR_BIT] |= (1 << (fd & (CHAR_BIT - 1)));
      ret++;
    }

  if (write && (wbits->in[fd / CHAR_BIT] & (1 << (fd & (CHAR_BIT - 1)))))
    {
      wbits->out[fd / CHAR_BIT] |= (1 << (fd & (CHAR_BIT - 1)));
      ret++;
    }

  if (except && (xbits->in[fd / CHAR_BIT] & (1 << (fd & (CHAR_BIT - 1)))))
    {
      xbits->out[fd / CHAR_BIT] |= (1 << (fd & (CHAR_BIT - 1)));
      ret++;
    }

  return ret;
}

int
rpl_select (int nfds, fd_set *rfds, fd_set *wfds, fd_set *xfds,
            struct timeval *timeout)
#undef timeval
{
  static struct timeval tv0;
  static HANDLE hEvent;
  HANDLE h, handle_array[FD_SETSIZE + 2];
  fd_set handle_rfds, handle_wfds, handle_xfds;
  struct bitset rbits, wbits, xbits;
  unsigned char anyfds_in[FD_SETSIZE / CHAR_BIT];
  DWORD ret, wait_timeout, nhandles, nsock, nbuffer;
  MSG msg;
  int i, fd, rc;
  clock_t tend;

  if (nfds < 0 || nfds > FD_SETSIZE)
    {
      errno = EINVAL;
      return -1;
    }

  if (!timeout)
    wait_timeout = INFINITE;
  else
    {
      wait_timeout = timeout->tv_sec * 1000 + timeout->tv_usec / 1000;

       
      if (!rfds && !wfds && !xfds)
        {
          Sleep (wait_timeout);
          return 0;
        }
    }

  if (!hEvent)
    hEvent = CreateEvent (NULL, FALSE, FALSE, NULL);

  handle_array[0] = hEvent;
  nhandles = 1;
  nsock = 0;

   
  memset (&rbits, 0, sizeof (rbits));
  memset (&wbits, 0, sizeof (wbits));
  memset (&xbits, 0, sizeof (xbits));
  memset (anyfds_in, 0, sizeof (anyfds_in));
  if (rfds)
    for (i = 0; i < rfds->fd_count; i++)
      {
        fd = rfds->fd_array[i];
        h = (HANDLE) _get_osfhandle (fd);
        if (IsConsoleHandle (h)
            && !GetNumberOfConsoleInputEvents (h, &nbuffer))
          continue;

        rbits.in[fd / CHAR_BIT] |= 1 << (fd & (CHAR_BIT - 1));
        anyfds_in[fd / CHAR_BIT] |= 1 << (fd & (CHAR_BIT - 1));
      }
  else
    rfds = (fd_set *) alloca (sizeof (fd_set));

  if (wfds)
    for (i = 0; i < wfds->fd_count; i++)
      {
        fd = wfds->fd_array[i];
        h = (HANDLE) _get_osfhandle (fd);
        if (IsConsoleHandle (h)
            && GetNumberOfConsoleInputEvents (h, &nbuffer))
          continue;

        wbits.in[fd / CHAR_BIT] |= 1 << (fd & (CHAR_BIT - 1));
        anyfds_in[fd / CHAR_BIT] |= 1 << (fd & (CHAR_BIT - 1));
      }
  else
    wfds = (fd_set *) alloca (sizeof (fd_set));

  if (xfds)
    for (i = 0; i < xfds->fd_count; i++)
      {
        fd = xfds->fd_array[i];
        xbits.in[fd / CHAR_BIT] |= 1 << (fd & (CHAR_BIT - 1));
        anyfds_in[fd / CHAR_BIT] |= 1 << (fd & (CHAR_BIT - 1));
      }
  else
    xfds = (fd_set *) alloca (sizeof (fd_set));

   
  FD_ZERO (rfds);
  FD_ZERO (wfds);
  FD_ZERO (xfds);
  FD_ZERO (&handle_rfds);
  FD_ZERO (&handle_wfds);
  FD_ZERO (&handle_xfds);

   
  for (i = 0; i < nfds; i++)
    {
      if ((anyfds_in[i / CHAR_BIT] & (1 << (i & (CHAR_BIT - 1)))) == 0)
        continue;

      h = (HANDLE) _get_osfhandle (i);
      if (!h)
        {
          errno = EBADF;
          return -1;
        }

      if (IsSocketHandle (h))
        {
          int requested = FD_CLOSE;

           
          if (rbits.in[i / CHAR_BIT] & (1 << (i & (CHAR_BIT - 1))))
            {
              requested |= FD_READ | FD_ACCEPT;
              FD_SET ((SOCKET) h, rfds);
              FD_SET ((SOCKET) h, &handle_rfds);
            }
          if (wbits.in[i / CHAR_BIT] & (1 << (i & (CHAR_BIT - 1))))
            {
              requested |= FD_WRITE | FD_CONNECT;
              FD_SET ((SOCKET) h, wfds);
              FD_SET ((SOCKET) h, &handle_wfds);
            }
          if (xbits.in[i / CHAR_BIT] & (1 << (i & (CHAR_BIT - 1))))
            {
              requested |= FD_OOB;
              FD_SET ((SOCKET) h, xfds);
              FD_SET ((SOCKET) h, &handle_xfds);
            }

          WSAEventSelect ((SOCKET) h, hEvent, requested);
          nsock++;
        }
      else
        {
          handle_array[nhandles++] = h;

           
          if (wait_timeout != 0
              && windows_poll_handle (h, i, &rbits, &wbits, &xbits))
            wait_timeout = 0;
        }
    }

   
  handle_array[nhandles] = NULL;

   
  if (wait_timeout != INFINITE)
    tend = clock () + wait_timeout;

restart:
  if (wait_timeout == 0 || nsock == 0)
    rc = 0;
  else
    {
       
      rc = select (0, &handle_rfds, &handle_wfds, &handle_xfds, &tv0);
      if (rc == 0)
        {
           
          memcpy (&handle_rfds, rfds, sizeof (fd_set));
          memcpy (&handle_wfds, wfds, sizeof (fd_set));
          memcpy (&handle_xfds, xfds, sizeof (fd_set));
        }
      else
        wait_timeout = 0;
    }

   
  if (wait_timeout != INFINITE)
    {
      clock_t tnow = clock ();
      if (tend >= tnow)
        wait_timeout = tend - tnow;
      else
        wait_timeout = 0;
    }

  for (;;)
    {
      ret = MsgWaitForMultipleObjects (nhandles, handle_array, FALSE,
                                       wait_timeout, QS_ALLINPUT);

      if (ret == WAIT_OBJECT_0 + nhandles)
        {
           
          BOOL bRet;
          while ((bRet = PeekMessage (&msg, NULL, 0, 0, PM_REMOVE)) != 0)
            {
              TranslateMessage (&msg);
              DispatchMessage (&msg);
            }
        }
      else
        break;
    }

   
  if (rc == 0 && nsock > 0)
    rc = select (0, &handle_rfds, &handle_wfds, &handle_xfds, &tv0);

  if (nhandles > 1)
    {
       
      nhandles = 1;
      for (i = 0; i < nfds; i++)
        {
          if ((anyfds_in[i / CHAR_BIT] & (1 << (i & (CHAR_BIT - 1)))) == 0)
            continue;

          h = (HANDLE) _get_osfhandle (i);
          if (h == handle_array[nhandles])
            {
               
              nhandles++;
              windows_poll_handle (h, i, &rbits, &wbits, &xbits);
              if (rbits.out[i / CHAR_BIT] & (1 << (i & (CHAR_BIT - 1)))
                  || wbits.out[i / CHAR_BIT] & (1 << (i & (CHAR_BIT - 1)))
                  || xbits.out[i / CHAR_BIT] & (1 << (i & (CHAR_BIT - 1))))
                rc++;
            }
        }

      if (rc == 0
          && (wait_timeout == INFINITE
               
              || (wait_timeout != 0 && nhandles > 1)))
        {
           
          memcpy (&handle_rfds, rfds, sizeof (fd_set));
          memcpy (&handle_wfds, wfds, sizeof (fd_set));
          memcpy (&handle_xfds, xfds, sizeof (fd_set));
          SleepEx (1, TRUE);
          goto restart;
        }
      if (timeout && wait_timeout == 0 && rc == 0)
        timeout->tv_sec = timeout->tv_usec = 0;
    }

   
  FD_ZERO (rfds);
  FD_ZERO (wfds);
  FD_ZERO (xfds);
  nhandles = 1;
  for (i = 0; i < nfds; i++)
    {
      if ((anyfds_in[i / CHAR_BIT] & (1 << (i & (CHAR_BIT - 1)))) == 0)
        continue;

      h = (HANDLE) _get_osfhandle (i);
      if (h != handle_array[nhandles])
        {
           
          SOCKET s = (SOCKET) h;
          WSAEventSelect (s, NULL, 0);
          if (FD_ISSET (s, &handle_rfds))
            FD_SET (i, rfds);
          if (FD_ISSET (s, &handle_wfds))
            FD_SET (i, wfds);
          if (FD_ISSET (s, &handle_xfds))
            FD_SET (i, xfds);
        }
      else
        {
           
          nhandles++;
          if (rbits.out[i / CHAR_BIT] & (1 << (i & (CHAR_BIT - 1))))
            FD_SET (i, rfds);
          if (wbits.out[i / CHAR_BIT] & (1 << (i & (CHAR_BIT - 1))))
            FD_SET (i, wfds);
          if (xbits.out[i / CHAR_BIT] & (1 << (i & (CHAR_BIT - 1))))
            FD_SET (i, xfds);
        }
    }

  return rc;
}

#else  

#include <stddef.h>  
#include <errno.h>
#include <unistd.h>

#undef select

int
rpl_select (int nfds, fd_set *rfds, fd_set *wfds, fd_set *xfds,
            struct timeval *timeout)
{
  int i;

   
  if (nfds < 0 || nfds > FD_SETSIZE)
    {
      errno = EINVAL;
      return -1;
    }
  for (i = 0; i < nfds; i++)
    {
      if (((rfds && FD_ISSET (i, rfds))
           || (wfds && FD_ISSET (i, wfds))
           || (xfds && FD_ISSET (i, xfds)))
          && dup2 (i, i) != i)
        return -1;
    }

   
  if (nfds == 0)
    {
      nfds = 1;
      rfds = NULL;
      wfds = NULL;
      xfds = NULL;
    }
  return select (nfds, rfds, wfds, xfds, timeout);
}

#endif
