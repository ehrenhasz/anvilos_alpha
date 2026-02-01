 

 

#include <config.h>

#include "mkancesdirs.h"

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#include <errno.h>
#include <unistd.h>

#include "filename.h"
#include "savewd.h"

 

ptrdiff_t
mkancesdirs (char *file, struct savewd *wd,
             int (*make_dir) (char const *, char const *, void *),
             void *make_dir_arg)
{
   
  char *sep = NULL;

   
  char *component = file;

  char *p = file + FILE_SYSTEM_PREFIX_LEN (file);
  char c;
  bool made_dir = false;

   

  while ((c = *p++))
    if (ISSLASH (*p))
      {
        if (! ISSLASH (c))
          sep = p;
      }
    else if (ISSLASH (c) && *p && sep)
      {
         
        if (! (sep - component == 1 && component[0] == '.'))
          {
            int make_dir_errno = 0;
            int savewd_chdir_options = 0;
            int chdir_result;

             
            *sep = '\0';

             
            if (sep - component == 2
                && component[0] == '.' && component[1] == '.')
              made_dir = false;
            else if (make_dir (file, component, make_dir_arg) < 0)
              make_dir_errno = errno;
            else
              made_dir = true;

            if (made_dir)
              savewd_chdir_options |= SAVEWD_CHDIR_NOFOLLOW;

            chdir_result =
              savewd_chdir (wd, component, savewd_chdir_options, NULL);

             
            if (chdir_result != -1)
              *sep = '/';

            if (chdir_result != 0)
              {
                if (make_dir_errno != 0 && errno == ENOENT)
                  errno = make_dir_errno;
                return chdir_result;
              }
          }

        component = p;
      }

  return component - file;
}
