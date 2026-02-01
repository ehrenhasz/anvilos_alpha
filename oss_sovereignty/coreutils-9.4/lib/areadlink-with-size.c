 

#include <config.h>

#include "areadlink.h"

#include <errno.h>
#include <limits.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

 
#ifndef SYMLINK_MAX
# define SYMLINK_MAX 1024
#endif

#define MAXSIZE (SIZE_MAX < SSIZE_MAX ? SIZE_MAX : SSIZE_MAX)

 

char *
areadlink_with_size (char const *file, size_t size)
{
   
  size_t symlink_max = SYMLINK_MAX;
  size_t INITIAL_LIMIT_BOUND = 8 * 1024;
  size_t initial_limit = (symlink_max < INITIAL_LIMIT_BOUND
                          ? symlink_max + 1
                          : INITIAL_LIMIT_BOUND);

  enum { stackbuf_size = 128 };

   
  size_t buf_size = (size == 0 ? stackbuf_size
                     : size < initial_limit ? size + 1 : initial_limit);

  while (1)
    {
      ssize_t r;
      size_t link_length;
      char stackbuf[stackbuf_size];
      char *buf = stackbuf;
      char *buffer = NULL;

      if (! (size == 0 && buf_size == stackbuf_size))
        {
          buf = buffer = malloc (buf_size);
          if (!buffer)
            {
              errno = ENOMEM;
              return NULL;
            }
        }

      r = readlink (file, buf, buf_size);
      link_length = r;

      if (r < 0)
        {
          free (buffer);
          return NULL;
        }

      if (link_length < buf_size)
        {
          buf[link_length] = 0;
          if (!buffer)
            {
              buffer = malloc (link_length + 1);
              if (buffer)
                return memcpy (buffer, buf, link_length + 1);
            }
          else if (link_length + 1 < buf_size)
            {
               
              char *shrinked_buffer = realloc (buffer, link_length + 1);
              if (shrinked_buffer != NULL)
                buffer = shrinked_buffer;
            }
          return buffer;
        }

      free (buffer);
      if (buf_size <= MAXSIZE / 2)
        buf_size *= 2;
      else if (buf_size < MAXSIZE)
        buf_size = MAXSIZE;
      else
        {
          errno = ENOMEM;
          return NULL;
        }
    }
}
