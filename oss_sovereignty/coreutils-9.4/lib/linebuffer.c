 

#include <config.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include "linebuffer.h"
#include "xalloc.h"

#if USE_UNLOCKED_IO
# include "unlocked-io.h"
#endif

 

void
initbuffer (struct linebuffer *linebuffer)
{
  memset (linebuffer, 0, sizeof *linebuffer);
}

struct linebuffer *
readlinebuffer (struct linebuffer *linebuffer, FILE *stream)
{
  return readlinebuffer_delim (linebuffer, stream, '\n');
}

 
struct linebuffer *
readlinebuffer_delim (struct linebuffer *linebuffer, FILE *stream,
                      char delimiter)
{
  int c;
  char *buffer = linebuffer->buffer;
  char *p = linebuffer->buffer;
  char *end = buffer + linebuffer->size;  

  if (feof (stream))
    return NULL;

  do
    {
      c = getc (stream);
      if (c == EOF)
        {
          if (p == buffer || ferror (stream))
            return NULL;
          if (p[-1] == delimiter)
            break;
          c = delimiter;
        }
      if (p == end)
        {
          idx_t oldsize = linebuffer->size;
          buffer = xpalloc (buffer, &linebuffer->size, 1, -1, 1);
          p = buffer + oldsize;
          linebuffer->buffer = buffer;
          end = buffer + linebuffer->size;
        }
      *p++ = c;
    }
  while (c != delimiter);

  linebuffer->length = p - buffer;
  return linebuffer;
}

 

void
freebuffer (struct linebuffer *linebuffer)
{
  free (linebuffer->buffer);
}
