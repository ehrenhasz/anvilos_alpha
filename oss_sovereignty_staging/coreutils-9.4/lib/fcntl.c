 

#include <config.h>

 
#include <fcntl.h>

#include <errno.h>
#include <limits.h>
#include <stdarg.h>
#include <stdlib.h>
#include <unistd.h>

#ifdef __KLIBC__
# define INCL_DOS
# include <os2.h>
#endif

#if defined _WIN32 && ! defined __CYGWIN__
 
# define WIN32_LEAN_AND_MEAN
# include <windows.h>

 
# if GNULIB_MSVC_NOTHROW
#  include "msvc-nothrow.h"
# else
#  include <io.h>
# endif

 
# define OPEN_MAX_MAX 0x10000

 
static int
dupfd (int oldfd, int newfd, int flags)
{
   
  HANDLE curr_process = GetCurrentProcess ();
  HANDLE old_handle = (HANDLE) _get_osfhandle (oldfd);
  unsigned char fds_to_close[OPEN_MAX_MAX / CHAR_BIT];
  unsigned int fds_to_close_bound = 0;
  int result;
  BOOL inherit = flags & O_CLOEXEC ? FALSE : TRUE;
  int mode;

  if (newfd < 0 || getdtablesize () <= newfd)
    {
      errno = EINVAL;
      return -1;
    }
  if (old_handle == INVALID_HANDLE_VALUE
      || (mode = _setmode (oldfd, O_BINARY)) == -1)
    {
       
      errno = EBADF;
      return -1;
    }
  _setmode (oldfd, mode);
  flags |= mode;

  for (;;)
    {
      HANDLE new_handle;
      int duplicated_fd;
      unsigned int index;

      if (!DuplicateHandle (curr_process,            
                            old_handle,              
                            curr_process,            
                            (PHANDLE) &new_handle,   
                            (DWORD) 0,               
                            inherit,                 
                            DUPLICATE_SAME_ACCESS))  
        {
          switch (GetLastError ())
            {
              case ERROR_TOO_MANY_OPEN_FILES:
                errno = EMFILE;
                break;
              case ERROR_INVALID_HANDLE:
              case ERROR_INVALID_TARGET_HANDLE:
              case ERROR_DIRECT_ACCESS_HANDLE:
                errno = EBADF;
                break;
              case ERROR_INVALID_PARAMETER:
              case ERROR_INVALID_FUNCTION:
              case ERROR_INVALID_ACCESS:
                errno = EINVAL;
                break;
              default:
                errno = EACCES;
                break;
            }
          result = -1;
          break;
        }
      duplicated_fd = _open_osfhandle ((intptr_t) new_handle, flags);
      if (duplicated_fd < 0)
        {
          CloseHandle (new_handle);
          result = -1;
          break;
        }
      if (newfd <= duplicated_fd)
        {
          result = duplicated_fd;
          break;
        }

       
      index = (unsigned int) duplicated_fd / CHAR_BIT;
      if (fds_to_close_bound <= index)
        {
          if (sizeof fds_to_close <= index)
             
            abort ();
          memset (fds_to_close + fds_to_close_bound, '\0',
                  index + 1 - fds_to_close_bound);
          fds_to_close_bound = index + 1;
        }
      fds_to_close[index] |= 1 << ((unsigned int) duplicated_fd % CHAR_BIT);
    }

   
  {
    int saved_errno = errno;
    unsigned int duplicated_fd;

    for (duplicated_fd = 0;
         duplicated_fd < fds_to_close_bound * CHAR_BIT;
         duplicated_fd++)
      if ((fds_to_close[duplicated_fd / CHAR_BIT]
           >> (duplicated_fd % CHAR_BIT))
          & 1)
        close (duplicated_fd);

    errno = saved_errno;
  }

# if REPLACE_FCHDIR
  if (0 <= result)
    result = _gl_register_dup (oldfd, result);
# endif
  return result;
}
#endif  

 
 
static int rpl_fcntl_DUPFD (int fd, int target);
 
static int rpl_fcntl_DUPFD_CLOEXEC (int fd, int target);
#ifdef __KLIBC__
 
static int klibc_fcntl (int fd, int action,  ...);
#endif


 

int
fcntl (int fd, int action,  ...)
#undef fcntl
#ifdef __KLIBC__
# define fcntl klibc_fcntl
#endif
{
  va_list arg;
  int result = -1;
  va_start (arg, action);
  switch (action)
    {
    case F_DUPFD:
      {
        int target = va_arg (arg, int);
        result = rpl_fcntl_DUPFD (fd, target);
        break;
      }

    case F_DUPFD_CLOEXEC:
      {
        int target = va_arg (arg, int);
        result = rpl_fcntl_DUPFD_CLOEXEC (fd, target);
        break;
      }

#if !HAVE_FCNTL
    case F_GETFD:
      {
# if defined _WIN32 && ! defined __CYGWIN__
        HANDLE handle = (HANDLE) _get_osfhandle (fd);
        DWORD flags;
        if (handle == INVALID_HANDLE_VALUE
            || GetHandleInformation (handle, &flags) == 0)
          errno = EBADF;
        else
          result = (flags & HANDLE_FLAG_INHERIT) ? 0 : FD_CLOEXEC;
# else  
         
        if (0 <= dup2 (fd, fd))
          result = 0;
# endif  
        break;
      }  
#endif  

       

    default:
      {
#if HAVE_FCNTL
        switch (action)
          {
          #ifdef F_BARRIERFSYNC                   
          case F_BARRIERFSYNC:
          #endif
          #ifdef F_CHKCLEAN                       
          case F_CHKCLEAN:
          #endif
          #ifdef F_CLOSEM                         
          case F_CLOSEM:
          #endif
          #ifdef F_FLUSH_DATA                     
          case F_FLUSH_DATA:
          #endif
          #ifdef F_FREEZE_FS                      
          case F_FREEZE_FS:
          #endif
          #ifdef F_FULLFSYNC                      
          case F_FULLFSYNC:
          #endif
          #ifdef F_GETCONFINED                    
          case F_GETCONFINED:
          #endif
          #ifdef F_GETDEFAULTPROTLEVEL            
          case F_GETDEFAULTPROTLEVEL:
          #endif
          #ifdef F_GETFD                          
          case F_GETFD:
          #endif
          #ifdef F_GETFL                          
          case F_GETFL:
          #endif
          #ifdef F_GETLEASE                       
          case F_GETLEASE:
          #endif
          #ifdef F_GETNOSIGPIPE                   
          case F_GETNOSIGPIPE:
          #endif
          #ifdef F_GETOWN                         
          case F_GETOWN:
          #endif
          #ifdef F_GETPIPE_SZ                     
          case F_GETPIPE_SZ:
          #endif
          #ifdef F_GETPROTECTIONCLASS             
          case F_GETPROTECTIONCLASS:
          #endif
          #ifdef F_GETPROTECTIONLEVEL             
          case F_GETPROTECTIONLEVEL:
          #endif
          #ifdef F_GET_SEALS                      
          case F_GET_SEALS:
          #endif
          #ifdef F_GETSIG                         
          case F_GETSIG:
          #endif
          #ifdef F_MAXFD                          
          case F_MAXFD:
          #endif
          #ifdef F_RECYCLE                        
          case F_RECYCLE:
          #endif
          #ifdef F_SETFIFOENH                     
          case F_SETFIFOENH:
          #endif
          #ifdef F_THAW_FS                        
          case F_THAW_FS:
          #endif
             
            result = fcntl (fd, action);
            break;

          #ifdef F_ADD_SEALS                      
          case F_ADD_SEALS:
          #endif
          #ifdef F_BADFD                          
          case F_BADFD:
          #endif
          #ifdef F_CHECK_OPENEVT                  
          case F_CHECK_OPENEVT:
          #endif
          #ifdef F_DUP2FD                         
          case F_DUP2FD:
          #endif
          #ifdef F_DUP2FD_CLOEXEC                 
          case F_DUP2FD_CLOEXEC:
          #endif
          #ifdef F_DUP2FD_CLOFORK                 
          case F_DUP2FD_CLOFORK:
          #endif
          #ifdef F_DUPFD                          
          case F_DUPFD:
          #endif
          #ifdef F_DUPFD_CLOEXEC                  
          case F_DUPFD_CLOEXEC:
          #endif
          #ifdef F_DUPFD_CLOFORK                  
          case F_DUPFD_CLOFORK:
          #endif
          #ifdef F_GETXFL                         
          case F_GETXFL:
          #endif
          #ifdef F_GLOBAL_NOCACHE                 
          case F_GLOBAL_NOCACHE:
          #endif
          #ifdef F_MAKECOMPRESSED                 
          case F_MAKECOMPRESSED:
          #endif
          #ifdef F_MOVEDATAEXTENTS                
          case F_MOVEDATAEXTENTS:
          #endif
          #ifdef F_NOCACHE                        
          case F_NOCACHE:
          #endif
          #ifdef F_NODIRECT                       
          case F_NODIRECT:
          #endif
          #ifdef F_NOTIFY                         
          case F_NOTIFY:
          #endif
          #ifdef F_OPLKACK                        
          case F_OPLKACK:
          #endif
          #ifdef F_OPLKREG                        
          case F_OPLKREG:
          #endif
          #ifdef F_RDAHEAD                        
          case F_RDAHEAD:
          #endif
          #ifdef F_SETBACKINGSTORE                
          case F_SETBACKINGSTORE:
          #endif
          #ifdef F_SETCONFINED                    
          case F_SETCONFINED:
          #endif
          #ifdef F_SETFD                          
          case F_SETFD:
          #endif
          #ifdef F_SETFL                          
          case F_SETFL:
          #endif
          #ifdef F_SETLEASE                       
          case F_SETLEASE:
          #endif
          #ifdef F_SETNOSIGPIPE                   
          case F_SETNOSIGPIPE:
          #endif
          #ifdef F_SETOWN                         
          case F_SETOWN:
          #endif
          #ifdef F_SETPIPE_SZ                     
          case F_SETPIPE_SZ:
          #endif
          #ifdef F_SETPROTECTIONCLASS             
          case F_SETPROTECTIONCLASS:
          #endif
          #ifdef F_SETSIG                         
          case F_SETSIG:
          #endif
          #ifdef F_SINGLE_WRITER                  
          case F_SINGLE_WRITER:
          #endif
             
            {
              int x = va_arg (arg, int);
              result = fcntl (fd, action, x);
            }
            break;

          default:
             
            {
              void *p = va_arg (arg, void *);
              result = fcntl (fd, action, p);
            }
            break;
          }
#else
        errno = EINVAL;
#endif
        break;
      }
    }
  va_end (arg);
  return result;
}

static int
rpl_fcntl_DUPFD (int fd, int target)
{
  int result;
#if !HAVE_FCNTL
  result = dupfd (fd, target, 0);
#elif FCNTL_DUPFD_BUGGY || REPLACE_FCHDIR
   
  if (target < 0 || getdtablesize () <= target)
    {
      result = -1;
      errno = EINVAL;
    }
  else
    {
       
      int flags = fcntl (fd, F_GETFD);
      if (flags < 0)
        result = -1;
      else
        {
          result = fcntl (fd, F_DUPFD, target);
          if (0 <= result && fcntl (fd, F_SETFD, flags) == -1)
            {
              int saved_errno = errno;
              close (result);
              result = -1;
              errno = saved_errno;
            }
# if REPLACE_FCHDIR
          if (0 <= result)
            result = _gl_register_dup (fd, result);
# endif
        }
    }
#else
  result = fcntl (fd, F_DUPFD, target);
#endif
  return result;
}

static int
rpl_fcntl_DUPFD_CLOEXEC (int fd, int target)
{
  int result;
#if !HAVE_FCNTL
  result = dupfd (fd, target, O_CLOEXEC);
#else  
# if defined __NetBSD__ || defined __HAIKU__
   
   
#  define have_dupfd_cloexec -1
# else
   
  static int have_dupfd_cloexec = GNULIB_defined_F_DUPFD_CLOEXEC ? -1 : 0;
  if (0 <= have_dupfd_cloexec)
    {
      result = fcntl (fd, F_DUPFD_CLOEXEC, target);
      if (0 <= result || errno != EINVAL)
        {
          have_dupfd_cloexec = 1;
#  if REPLACE_FCHDIR
          if (0 <= result)
            result = _gl_register_dup (fd, result);
#  endif
        }
      else
        {
          result = rpl_fcntl_DUPFD (fd, target);
          if (result >= 0)
            have_dupfd_cloexec = -1;
        }
    }
  else
# endif
    result = rpl_fcntl_DUPFD (fd, target);
  if (0 <= result && have_dupfd_cloexec == -1)
    {
      int flags = fcntl (result, F_GETFD);
      if (flags < 0 || fcntl (result, F_SETFD, flags | FD_CLOEXEC) == -1)
        {
          int saved_errno = errno;
          close (result);
          errno = saved_errno;
          result = -1;
        }
    }
#endif  
  return result;
}

#undef fcntl

#ifdef __KLIBC__

static int
klibc_fcntl (int fd, int action,  ...)
{
  va_list arg_ptr;
  int arg;
  struct stat sbuf;
  int result;

  va_start (arg_ptr, action);
  arg = va_arg (arg_ptr, int);
  result = fcntl (fd, action, arg);
   
  if (result == -1 && (errno == EPERM || errno == ENOTSUP)
      && !fstat (fd, &sbuf) && S_ISDIR (sbuf.st_mode))
    {
      ULONG ulMode;

      switch (action)
        {
        case F_DUPFD:
           
          while (fcntl (arg, F_GETFL) != -1 || errno != EBADF)
            arg++;

          result = dup2 (fd, arg);
          break;

         
        case F_GETFD:
          if (DosQueryFHState (fd, &ulMode))
            break;

          result = (ulMode & OPEN_FLAGS_NOINHERIT) ? FD_CLOEXEC : 0;
          break;

        case F_SETFD:
          if (arg & ~FD_CLOEXEC)
            break;

          if (DosQueryFHState (fd, &ulMode))
            break;

          if (arg & FD_CLOEXEC)
            ulMode |= OPEN_FLAGS_NOINHERIT;
          else
            ulMode &= ~OPEN_FLAGS_NOINHERIT;

           
          ulMode &= (OPEN_FLAGS_WRITE_THROUGH | OPEN_FLAGS_FAIL_ON_ERROR
                     | OPEN_FLAGS_NO_CACHE | OPEN_FLAGS_NOINHERIT);

          if (DosSetFHState (fd, ulMode))
            break;

          result = 0;
          break;

        case F_GETFL:
          result = 0;
          break;

        case F_SETFL:
          if (arg != 0)
            break;

          result = 0;
          break;

        default:
          errno = EINVAL;
          break;
        }
    }

  va_end (arg_ptr);

  return result;
}

#endif
