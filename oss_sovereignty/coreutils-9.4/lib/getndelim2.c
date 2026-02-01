 

#include <config.h>

#include "getndelim2.h"

#include <stddef.h>
#include <stdlib.h>
#include <string.h>

#if USE_UNLOCKED_IO
# include "unlocked-io.h"
#endif
#if !HAVE_FLOCKFILE
# undef flockfile
# define flockfile(x) ((void) 0)
#endif
#if !HAVE_FUNLOCKFILE
# undef funlockfile
# define funlockfile(x) ((void) 0)
#endif

#include <limits.h>
#include <stdint.h>

#include "freadptr.h"
#include "freadseek.h"
#include "memchr2.h"

 
#if __GNUC__ + (__GNUC_MINOR__ >= 7) > 4
# pragma GCC diagnostic ignored "-Wmaybe-uninitialized"
#endif

 
#define GETNDELIM2_MAXIMUM (PTRDIFF_MAX < SSIZE_MAX ? PTRDIFF_MAX : SSIZE_MAX)

 
#define MIN_CHUNK 64

ssize_t
getndelim2 (char **lineptr, size_t *linesize, size_t offset, size_t nmax,
            int delim1, int delim2, FILE *stream)
{
  size_t nbytes_avail;           
  char *read_pos;                
  ssize_t bytes_stored = -1;
  char *ptr = *lineptr;
  size_t size = *linesize;
  bool found_delimiter;

  if (!ptr)
    {
      size = nmax < MIN_CHUNK ? nmax : MIN_CHUNK;
      ptr = malloc (size);
      if (!ptr)
        return -1;
    }

  if (size < offset)
    goto done;

  nbytes_avail = size - offset;
  read_pos = ptr + offset;

  if (nbytes_avail == 0 && nmax <= size)
    goto done;

   
  if (delim1 == EOF)
    delim1 = delim2;
  else if (delim2 == EOF)
    delim2 = delim1;

  flockfile (stream);

  found_delimiter = false;
  do
    {
       

      int c;
      const char *buffer;
      size_t buffer_len;

      buffer = freadptr (stream, &buffer_len);
      if (buffer)
        {
          if (delim1 != EOF)
            {
              const char *end = memchr2 (buffer, delim1, delim2, buffer_len);
              if (end)
                {
                  buffer_len = end - buffer + 1;
                  found_delimiter = true;
                }
            }
        }
      else
        {
          c = getc (stream);
          if (c == EOF)
            {
               
              if (read_pos == ptr)
                goto unlock_done;
              else
                break;
            }
          if (c == delim1 || c == delim2)
            found_delimiter = true;
          buffer_len = 1;
        }

       

      if (nbytes_avail < buffer_len + 1 && size < nmax)
        {
           
          size_t newsize = size < MIN_CHUNK ? size + MIN_CHUNK : 2 * size;
          char *newptr;

           
          if (newsize - (read_pos - ptr) < buffer_len + 1)
            newsize = (read_pos - ptr) + buffer_len + 1;
           
          if (! (size < newsize && newsize <= nmax))
            newsize = nmax;

          if (GETNDELIM2_MAXIMUM < newsize - offset)
            {
              size_t newsizemax = offset + GETNDELIM2_MAXIMUM + 1;
              if (size == newsizemax)
                goto unlock_done;
              newsize = newsizemax;
            }

          nbytes_avail = newsize - (read_pos - ptr);
          newptr = realloc (ptr, newsize);
          if (!newptr)
            goto unlock_done;
          ptr = newptr;
          size = newsize;
          read_pos = size - nbytes_avail + ptr;
        }

       

      if (1 < nbytes_avail)
        {
          size_t copy_len = nbytes_avail - 1;
          if (buffer_len < copy_len)
            copy_len = buffer_len;
          if (buffer)
            memcpy (read_pos, buffer, copy_len);
          else
            *read_pos = c;
          read_pos += copy_len;
          nbytes_avail -= copy_len;
        }

       

      if (buffer && freadseek (stream, buffer_len))
        goto unlock_done;
    }
  while (!found_delimiter);

   
  *read_pos = '\0';

  bytes_stored = read_pos - (ptr + offset);

 unlock_done:
  funlockfile (stream);

 done:
  *lineptr = ptr;
  *linesize = size;
  return bytes_stored ? bytes_stored : -1;
}
