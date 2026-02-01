 

#include <config.h>

 
#include <stdlib.h>

#include <errno.h>
#include <string.h>
#include <unistd.h>
#if defined __linux__
# include <fcntl.h>
# include <stdint.h>
# include <string.h>
# include <sys/mman.h>
#endif

#include "macros.h"

 
static int
get_errno (void)
{
  volatile int err = errno;
  return err;
}

static int (* volatile get_errno_func) (void) = get_errno;

int
main ()
{
   
  {
    errno = 1789;  
    free (NULL);
    ASSERT_NO_STDIO (get_errno_func () == 1789);
  }
  {  
    #define N 10000
    void * volatile ptrs[N];
    size_t i;
    for (i = 0; i < N; i++)
      ptrs[i] = malloc (15);
    for (i = 0; i < N; i++)
      {
        errno = 1789;
        free (ptrs[i]);
        ASSERT_NO_STDIO (get_errno_func () == 1789);
      }
    #undef N
  }
  {  
    #define N 1000
    void * volatile ptrs[N];
    size_t i;
    for (i = 0; i < N; i++)
      ptrs[i] = malloc (729);
    for (i = 0; i < N; i++)
      {
        errno = 1789;
        free (ptrs[i]);
        ASSERT_NO_STDIO (get_errno_func () == 1789);
      }
    #undef N
  }
  {  
    #define N 10
    void * volatile ptrs[N];
    size_t i;
    for (i = 0; i < N; i++)
      ptrs[i] = malloc (5318153);
    for (i = 0; i < N; i++)
      {
        errno = 1789;
        free (ptrs[i]);
        ASSERT_NO_STDIO (get_errno_func () == 1789);
      }
    #undef N
  }

   
  #if defined __linux__ && !(__GLIBC__ == 2 && __GLIBC_MINOR__ < 15)
  if (open ("/proc/sys/vm/max_map_count", O_RDONLY) >= 0)
    {
       
      size_t pagesize = getpagesize ();
      void *firstpage_backup = malloc (pagesize);
      void *lastpage_backup = malloc (pagesize);
       
      void *bumper_region =
        mmap (NULL, 0x1000000, PROT_READ, MAP_ANONYMOUS | MAP_PRIVATE, -1, 0);
       
      int fd = open ("test-free", O_RDONLY);

      if (firstpage_backup != NULL && lastpage_backup != NULL
          && bumper_region != (void *)(-1)
          && fd >= 0)
        {
           
          size_t big_size = 0x1000000;
          void * volatile ptr = malloc (big_size - 0x100);
          char *ptr_aligned = (char *) ((uintptr_t) ptr & ~(pagesize - 1));
           
          memcpy (firstpage_backup, ptr_aligned, pagesize);
          memcpy (lastpage_backup, ptr_aligned + big_size - pagesize, pagesize);
          if (mmap (ptr_aligned - pagesize, pagesize + big_size + pagesize,
                    PROT_READ | PROT_WRITE,
                    MAP_ANONYMOUS | MAP_PRIVATE | MAP_FIXED, -1, 0)
              != (void *)(-1))
            {
              memcpy (ptr_aligned, firstpage_backup, pagesize);
              memcpy (ptr_aligned + big_size - pagesize, lastpage_backup, pagesize);

               
              size_t i;
              for (i = 0; i < 65536; i++)
                if (mmap (NULL, pagesize, PROT_READ, MAP_FILE | MAP_PRIVATE, fd, 0)
                    == (void *)(-1))
                  break;
               

              errno = 1789;
               
              free (ptr);
              ASSERT_NO_STDIO (get_errno_func () == 1789);
            }
        }
    }
  #endif

  return 0;
}
