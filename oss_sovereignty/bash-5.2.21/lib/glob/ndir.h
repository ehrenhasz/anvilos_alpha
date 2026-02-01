 

#if defined (VMS)
#  if !defined (FAB$C_BID)
#    include <fab.h>
#  endif
#  if !defined (NAM$C_BID)
#    include <nam.h>
#  endif
#  if !defined (RMS$_SUC)
#    include <rmsdef.h>
#  endif
#  include "dir.h"
#endif  

 
#define DIRBLKSIZ 512

 

#if defined (VMS)
#  define MAXNAMLEN (DIR$S_NAME + 7)	 
#  define MAXFULLSPEC NAM$C_MAXRSS	 
#else
#  define MAXNAMLEN 15			 
#endif  

 
struct direct {
  long d_ino;			 
  unsigned short d_reclen;	 
  unsigned short d_namlen;	 
  char d_name[MAXNAMLEN + 1];	 
};

 
typedef struct {
  int dd_fd;			 
  int dd_loc;			 
  int dd_size;			 
  char	dd_buf[DIRBLKSIZ];	 
} DIR;

extern DIR *opendir ();
extern struct direct *readdir ();
extern long telldir ();
extern void seekdir (), closedir ();

#define rewinddir(dirp) seekdir (dirp, 0L)
