 

#include <config.h>

#include "careadlinkat.h"

#include "idx.h"
#include "minmax.h"

#include <errno.h>
#include <limits.h>
#include <string.h>
#include <unistd.h>

 
#ifndef SIZE_MAX
# define SIZE_MAX ((size_t) -1)
#endif

#include "allocator.h"

enum { STACK_BUF_SIZE = 1024 };

 
#if _GL_GNUC_PREREQ (10, 1)
# if _GL_GNUC_PREREQ (12, 1)
#  pragma GCC diagnostic ignored "-Wreturn-local-addr"
# elif defined GCC_LINT || defined lint
__attribute__ ((__noinline__))
# elif __OPTIMIZE__ && !__NO_INLINE__
#  define GCC_BOGUS_WRETURN_LOCAL_ADDR
# endif
#endif
static char *
readlink_stk (int fd, char const *filename,
              char *buffer, size_t buffer_size,
              struct allocator const *alloc,
              ssize_t (*preadlinkat) (int, char const *, char *, size_t),
              char stack_buf[STACK_BUF_SIZE])
{
  if (! alloc)
    alloc = &stdlib_allocator;

  if (!buffer)
    {
      buffer = stack_buf;
      buffer_size = STACK_BUF_SIZE;
    }

  char *buf = buffer;
  idx_t buf_size_max = MIN (IDX_MAX, MIN (SSIZE_MAX, SIZE_MAX));
  idx_t buf_size = MIN (buffer_size, buf_size_max);

  while (buf)
    {
       
      idx_t link_length = preadlinkat (fd, filename, buf, buf_size);
      if (link_length < 0)
        {
          if (buf != buffer)
            {
              int readlinkat_errno = errno;
              alloc->free (buf);
              errno = readlinkat_errno;
            }
          return NULL;
        }

      idx_t link_size = link_length;

      if (link_size < buf_size)
        {
          buf[link_size++] = '\0';

          if (buf == stack_buf)
            {
              char *b = alloc->allocate (link_size);
              buf_size = link_size;
              if (! b)
                break;
              return memcpy (b, buf, link_size);
            }

          if (link_size < buf_size && buf != buffer && alloc->reallocate)
            {
               
              char *b = alloc->reallocate (buf, link_size);
              if (b)
                return b;
            }

          return buf;
        }

      if (buf != buffer)
        alloc->free (buf);

      if (buf_size_max / 2 <= buf_size)
        {
          errno = ENAMETOOLONG;
          return NULL;
        }

      buf_size = 2 * buf_size + 1;
      buf = alloc->allocate (buf_size);
    }

  if (alloc->die)
    alloc->die (buf_size);
  errno = ENOMEM;
  return NULL;
}


 

char *
careadlinkat (int fd, char const *filename,
              char *buffer, size_t buffer_size,
              struct allocator const *alloc,
              ssize_t (*preadlinkat) (int, char const *, char *, size_t))
{
   
  #ifdef GCC_BOGUS_WRETURN_LOCAL_ADDR
   #warning "GCC might issue a bogus -Wreturn-local-addr warning here."
   #warning "See <https://gcc.gnu.org/bugzilla/show_bug.cgi?id=93644>."
  #endif
  char stack_buf[STACK_BUF_SIZE];
  return readlink_stk (fd, filename, buffer, buffer_size, alloc,
                       preadlinkat, stack_buf);
}
