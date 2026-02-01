 
#include <dirent.h>

#include <errno.h>

#if GNULIB_defined_DIR
# include "dirent-private.h"
#endif

 
#undef FindFirstFile
#define FindFirstFile FindFirstFileA

void
rewinddir (DIR *dirp)
#undef rewinddir
{
#if HAVE_DIRENT_H                        
  rewinddir (dirp->real_dirp);
#else
   
  if (dirp->current != INVALID_HANDLE_VALUE)
    FindClose (dirp->current);

   
  dirp->status = -1;
  dirp->current = FindFirstFile (dirp->dir_name_mask, &dirp->entry);
  if (dirp->current == INVALID_HANDLE_VALUE)
    {
      switch (GetLastError ())
        {
        case ERROR_FILE_NOT_FOUND:
          dirp->status = -2;
          break;
        default:
           
          dirp->status = ENOENT;
          break;
        }
    }
#endif
}
