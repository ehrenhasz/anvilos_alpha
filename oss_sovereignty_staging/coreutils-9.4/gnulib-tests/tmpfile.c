 

#include <config.h>

 
#include <stdio.h>

#include <errno.h>

#if defined _WIN32 && ! defined __CYGWIN__
 

# include <fcntl.h>
# include <string.h>
# include <sys/stat.h>

# include <io.h>

# define WIN32_LEAN_AND_MEAN   
# include <windows.h>

#else

# include <unistd.h>

#endif

#include "pathmax.h"
#include "tempname.h"
#include "tmpdir.h"

 

#if defined _WIN32 && ! defined __CYGWIN__
 

 
# undef OSVERSIONINFO
# define OSVERSIONINFO OSVERSIONINFOA
# undef GetVersionEx
# define GetVersionEx GetVersionExA
# undef GetTempPath
# define GetTempPath GetTempPathA

 

static bool
supports_delete_on_close ()
{
  static int known;  
  if (!known)
    {
      OSVERSIONINFO v;

       
      v.dwOSVersionInfoSize = sizeof (OSVERSIONINFO);

      if (GetVersionEx (&v))
        known = (v.dwPlatformId == VER_PLATFORM_WIN32_NT ? 1 : -1);
      else
        known = -1;
    }
  return (known > 0);
}

FILE *
tmpfile (void)
{
  char dir[PATH_MAX];
  DWORD retval;

   
  retval = GetTempPath (PATH_MAX, dir);
  if (retval > 0 && retval < PATH_MAX)
    {
      char xtemplate[PATH_MAX];

      if (path_search (xtemplate, PATH_MAX, dir, NULL, true) >= 0)
        {
          size_t len = strlen (xtemplate);
          int o_temporary = (supports_delete_on_close () ? _O_TEMPORARY : 0);
          int fd;

          do
            {
              memcpy (&xtemplate[len - 6], "XXXXXX", 6);
              if (gen_tempname (xtemplate, 0, 0, GT_NOCREATE) < 0)
                {
                  fd = -1;
                  break;
                }

              fd = _open (xtemplate,
                          _O_CREAT | _O_EXCL | o_temporary
                          | _O_RDWR | _O_BINARY,
                          _S_IREAD | _S_IWRITE);
            }
          while (fd < 0 && errno == EEXIST);

          if (fd >= 0)
            {
              FILE *fp = _fdopen (fd, "w+b");

              if (fp != NULL)
                return fp;
              else
                {
                  int saved_errno = errno;
                  _close (fd);
                  errno = saved_errno;
                }
            }
        }
    }
  else
    {
      if (retval > 0)
        errno = ENAMETOOLONG;
      else
         
        errno = ENOENT;
    }

  return NULL;
}

#else

FILE *
tmpfile (void)
{
  char buf[PATH_MAX];
  int fd;
  FILE *fp;

   

  if (path_search (buf, sizeof buf, NULL, "tmpf", true))
    return NULL;

  fd = gen_tempname (buf, 0, 0, GT_FILE);
  if (fd < 0)
    return NULL;

   
  (void) unlink (buf);

  if ((fp = fdopen (fd, "w+b")) == NULL)
    {
      int saved_errno = errno;
      close (fd);
      errno = saved_errno;
    }

  return fp;
}

#endif
