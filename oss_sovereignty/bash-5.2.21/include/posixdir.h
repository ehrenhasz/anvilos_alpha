 

 

 

#if !defined (_POSIXDIR_H_)
#define _POSIXDIR_H_

#if defined (HAVE_DIRENT_H)
#  include <dirent.h>
#  if defined (HAVE_STRUCT_DIRENT_D_NAMLEN)
#    define D_NAMLEN(d)	((d)->d_namlen)
#  else
#    define D_NAMLEN(d)   (strlen ((d)->d_name))
#  endif  
#else
#  if defined (HAVE_SYS_NDIR_H)
#    include <sys/ndir.h>
#  endif
#  if defined (HAVE_SYS_DIR_H)
#    include <sys/dir.h>
#  endif
#  if defined (HAVE_NDIR_H)
#    include <ndir.h>
#  endif
#  if !defined (dirent)
#    define dirent direct
#  endif  
#  define D_NAMLEN(d)   ((d)->d_namlen)
#endif  

 
#if defined (HAVE_STRUCT_DIRENT_D_INO) && !defined (HAVE_STRUCT_DIRENT_D_FILENO)
#  define d_fileno d_ino
#endif

 
#if !defined (HAVE_STRUCT_DIRENT_D_INO) || defined (BROKEN_DIRENT_D_INO)
#  define REAL_DIR_ENTRY(dp) 1
#else
#  define REAL_DIR_ENTRY(dp) (dp->d_ino != 0)
#endif  

#if defined (HAVE_STRUCT_DIRENT_D_INO) && !defined (BROKEN_DIRENT_D_INO)
#  define D_INO_AVAILABLE
#endif

 
#if defined (D_INO_AVAILABLE) || defined (HAVE_STRUCT_DIRENT_D_FILENO)
#  define D_FILENO_AVAILABLE 1
#endif

#endif  
