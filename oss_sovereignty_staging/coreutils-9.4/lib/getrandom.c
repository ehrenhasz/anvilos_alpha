 

#include <config.h>

#include <sys/random.h>

#include <errno.h>
#include <fcntl.h>
#include <unistd.h>

#if defined _WIN32 && ! defined __CYGWIN__
# define WIN32_LEAN_AND_MEAN
# include <windows.h>
# if HAVE_BCRYPT_H
#  include <bcrypt.h>
# else
#  define NTSTATUS LONG
typedef void * BCRYPT_ALG_HANDLE;
#  define BCRYPT_USE_SYSTEM_PREFERRED_RNG 0x00000002
#  if HAVE_LIB_BCRYPT
extern NTSTATUS WINAPI BCryptGenRandom (BCRYPT_ALG_HANDLE, UCHAR *, ULONG, ULONG);
#  endif
# endif
# if !HAVE_LIB_BCRYPT
#  include <wincrypt.h>
#  ifndef CRYPT_VERIFY_CONTEXT
#   define CRYPT_VERIFY_CONTEXT 0xF0000000
#  endif
# endif
#endif

#include "minmax.h"

#if defined _WIN32 && ! defined __CYGWIN__

 
# undef LoadLibrary
# define LoadLibrary LoadLibraryA
# undef CryptAcquireContext
# define CryptAcquireContext CryptAcquireContextA

# if !HAVE_LIB_BCRYPT

 
#  define GetProcAddress \
    (void *) GetProcAddress

 
typedef NTSTATUS (WINAPI * BCryptGenRandomFuncType) (BCRYPT_ALG_HANDLE, UCHAR *, ULONG, ULONG);
static BCryptGenRandomFuncType BCryptGenRandomFunc = NULL;
static BOOL initialized = FALSE;

static void
initialize (void)
{
  HMODULE bcrypt = LoadLibrary ("bcrypt.dll");
  if (bcrypt != NULL)
    {
      BCryptGenRandomFunc =
        (BCryptGenRandomFuncType) GetProcAddress (bcrypt, "BCryptGenRandom");
    }
  initialized = TRUE;
}

# else

#  define BCryptGenRandomFunc BCryptGenRandom

# endif

#else
 

 
# ifndef NAME_OF_RANDOM_DEVICE
#  define NAME_OF_RANDOM_DEVICE "/dev/random"
# endif

 
# ifndef NAME_OF_NONCE_DEVICE
#  define NAME_OF_NONCE_DEVICE "/dev/urandom"
# endif

#endif

 
ssize_t
getrandom (void *buffer, size_t length, unsigned int flags)
#undef getrandom
{
#if defined _WIN32 && ! defined __CYGWIN__
   
  static int bcrypt_not_working  ;
  if (!bcrypt_not_working)
    {
# if !HAVE_LIB_BCRYPT
      if (!initialized)
        initialize ();
# endif
      if (BCryptGenRandomFunc != NULL
          && BCryptGenRandomFunc (NULL, buffer, length,
                                  BCRYPT_USE_SYSTEM_PREFERRED_RNG)
             == 0  )
        return length;
      bcrypt_not_working = 1;
    }
# if !HAVE_LIB_BCRYPT
   ;
    static HCRYPTPROV provider;
    if (!crypt_initialized)
      {
        if (CryptAcquireContext (&provider, NULL, NULL, PROV_RSA_FULL,
                                 CRYPT_VERIFY_CONTEXT))
          crypt_initialized = 1;
        else
          crypt_initialized = -1;
      }
    if (crypt_initialized >= 0)
      {
        if (!CryptGenRandom (provider, length, buffer))
          {
            errno = EIO;
            return -1;
          }
        return length;
      }
  }
# endif
  errno = ENOSYS;
  return -1;
#elif HAVE_GETRANDOM
  return getrandom (buffer, length, flags);
#else
  static int randfd[2] = { -1, -1 };
  bool devrandom = (flags & GRND_RANDOM) != 0;
  int fd = randfd[devrandom];

  if (fd < 0)
    {
      static char const randdevice[][MAX (sizeof NAME_OF_NONCE_DEVICE,
                                          sizeof NAME_OF_RANDOM_DEVICE)]
        = { NAME_OF_NONCE_DEVICE, NAME_OF_RANDOM_DEVICE };
      int oflags = (O_RDONLY + O_CLOEXEC
                    + (flags & GRND_NONBLOCK ? O_NONBLOCK : 0));
      fd = open (randdevice[devrandom], oflags);
      if (fd < 0)
        {
          if (errno == ENOENT || errno == ENOTDIR)
            errno = ENOSYS;
          return -1;
        }
      randfd[devrandom] = fd;
    }

  return read (fd, buffer, length);
#endif
}
