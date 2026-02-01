 

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#if !(defined _WIN32 && !defined __CYGWIN__)
# include <pwd.h>
#endif

#include <sys/stat.h>
#include <sys/types.h>

#include "macros.h"

static void
test_getlogin_result (const char *buf, int err)
{
  if (err != 0)
    {
      if (err == ENOENT)
        {
           
          fprintf (stderr, "Skipping test: no entry in utmp file.\n");
          exit (77);
        }

      ASSERT (err == ENOTTY
              || err == EINVAL  
              || err == ENXIO
             );

#if defined __linux__
       
      bool loginuid_undefined = false;
       
      FILE *fp = fopen ("/proc/self/loginuid", "r");
      if (fp != NULL)
        {
          char fread_buf[21];
          size_t n = fread (fread_buf, 1, sizeof fread_buf, fp);
          if (n > 0 && n < sizeof fread_buf)
            {
              fread_buf[n] = '\0';
              errno = 0;
              char *endptr;
              unsigned long value = strtoul (fread_buf, &endptr, 10);
              if (*endptr == '\0' && errno == 0)
                loginuid_undefined = ((uid_t) value == (uid_t)(-1));
            }
          fclose (fp);
        }
      if (loginuid_undefined)
        {
          fprintf (stderr, "Skipping test: loginuid is undefined.\n");
          exit (77);
        }
#endif

       
#if !defined __hpux  
      ASSERT (! isatty (0));
#endif
      fprintf (stderr, "Skipping test: stdin is not a tty.\n");
      exit (77);
    }

   
#if !(defined _WIN32 && !defined __CYGWIN__)
   
  {
    struct stat stat_buf;
    struct passwd *pwd;

    if (!isatty (STDIN_FILENO))
      {
         
        fprintf (stderr, "Skipping test: stdin is not a tty.\n");
        exit (77);
      }

    ASSERT (fstat (STDIN_FILENO, &stat_buf) == 0);

    pwd = getpwnam (buf);
    if (! pwd)
      {
        fprintf (stderr, "Skipping test: %s: no such user\n", buf);
        exit (77);
      }

    ASSERT (pwd->pw_uid == stat_buf.st_uid);
  }
#endif
#if defined _WIN32 && !defined __CYGWIN__
   
  {
    const char *name = getenv ("USERNAME");
    if (name != NULL && name[0] != '\0')
      ASSERT (strcmp (buf, name) == 0);
  }
#endif
}
