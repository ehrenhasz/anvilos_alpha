 
#include <dirent.h>

#include <errno.h>
#include <stddef.h>

#if GNULIB_defined_DIR
# include "dirent-private.h"
#endif

 
#undef FindNextFile
#define FindNextFile FindNextFileA

struct dirent *
readdir (DIR *dirp)
#undef readdir
{
#if HAVE_DIRENT_H                        
  return readdir (dirp->real_dirp);
#else
  char type;
  struct dirent *result;

   

  switch (dirp->status)
    {
    case -2:
       
      return NULL;
    case -1:
      break;
    case 0:
      if (!FindNextFile (dirp->current, &dirp->entry))
        {
          switch (GetLastError ())
            {
            case ERROR_NO_MORE_FILES:
              dirp->status = -2;
              return NULL;
            default:
              errno = EIO;
              return NULL;
            }
        }
      break;
    default:
      errno = dirp->status;
      return NULL;
    }

  dirp->status = 0;

  if (dirp->entry.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
    type = DT_DIR;
  else if (dirp->entry.dwFileAttributes & FILE_ATTRIBUTE_REPARSE_POINT)
    type = DT_LNK;
  else if ((dirp->entry.dwFileAttributes
            & ~(FILE_ATTRIBUTE_READONLY
                | FILE_ATTRIBUTE_HIDDEN
                | FILE_ATTRIBUTE_SYSTEM
                | FILE_ATTRIBUTE_ARCHIVE
                | FILE_ATTRIBUTE_NORMAL
                | FILE_ATTRIBUTE_TEMPORARY
                | FILE_ATTRIBUTE_SPARSE_FILE
                | FILE_ATTRIBUTE_COMPRESSED
                | FILE_ATTRIBUTE_NOT_CONTENT_INDEXED
                | FILE_ATTRIBUTE_ENCRYPTED)) == 0)
     
    type = DT_REG;
  else
    type = DT_UNKNOWN;

   
  result =
    (struct dirent *)
    ((char *) dirp->entry.cFileName - offsetof (struct dirent, d_name[0]));
  result->d_type = type;

  return result;
#endif
}
