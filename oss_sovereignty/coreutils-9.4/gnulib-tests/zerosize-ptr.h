 

 
#if defined __ANDROID__
# undef _FILE_OFFSET_BITS
# undef __USE_FILE_OFFSET64
#endif

#include <stdlib.h>

 
#if HAVE_SYS_MMAN_H && HAVE_MPROTECT && !defined __KLIBC__
# include <fcntl.h>
# include <unistd.h>
# include <sys/types.h>
# include <sys/mman.h>
 
# ifndef MAP_FILE
#  define MAP_FILE 0
# endif
#endif

 

static void *
zerosize_ptr (void)
{
 
#if HAVE_SYS_MMAN_H && HAVE_MPROTECT && !defined __KLIBC__
# if HAVE_MAP_ANONYMOUS
  const int flags = MAP_ANONYMOUS | MAP_PRIVATE;
  const int fd = -1;
# else  
  const int flags = MAP_FILE | MAP_PRIVATE;
  int fd = open ("/dev/zero", O_RDONLY, 0666);
  if (fd >= 0)
# endif
    {
      int pagesize = getpagesize ();
      char *two_pages =
        (char *) mmap (NULL, 2 * pagesize, PROT_READ | PROT_WRITE,
                       flags, fd, 0);
      if (two_pages != (char *)(-1)
          && mprotect (two_pages + pagesize, pagesize, PROT_NONE) == 0)
        return two_pages + pagesize;
    }
#endif
  return NULL;
}
