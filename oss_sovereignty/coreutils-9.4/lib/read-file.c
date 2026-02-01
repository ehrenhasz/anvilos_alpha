 
#include <sys/stat.h>

 
#include <stdio.h>

 
#include <stdint.h>

 
#include <stdlib.h>

 
#include <string.h>

 
#include <errno.h>

 
char *
fread_file (FILE *stream, int flags, size_t *length)
{
  char *buf = NULL;
  size_t alloc = BUFSIZ;

   
  {
    struct stat st;

    if (fstat (fileno (stream), &st) >= 0 && S_ISREG (st.st_mode))
      {
        off_t pos = ftello (stream);

        if (pos >= 0 && pos < st.st_size)
          {
            off_t alloc_off = st.st_size - pos;

             
            if (PTRDIFF_MAX - 1 < alloc_off)
              {
                errno = ENOMEM;
                return NULL;
              }

            alloc = alloc_off + 1;
          }
      }
  }

  if (!(buf = malloc (alloc)))
    return NULL;  

  {
    size_t size = 0;  
    int save_errno;

    for (;;)
      {
         
        size_t requested = alloc - size;
        size_t count = fread (buf + size, 1, requested, stream);
        size += count;

        if (count != requested)
          {
            save_errno = errno;
            if (ferror (stream))
              break;

             
            if (size < alloc - 1)
              {
                if (flags & RF_SENSITIVE)
                  {
                    char *smaller_buf = malloc (size + 1);
                    if (smaller_buf == NULL)
                      memset_explicit (buf + size, 0, alloc - size);
                    else
                      {
                        memcpy (smaller_buf, buf, size);
                        memset_explicit (buf, 0, alloc);
                        free (buf);
                        buf = smaller_buf;
                      }
                  }
                else
                  {
                    char *smaller_buf = realloc (buf, size + 1);
                    if (smaller_buf != NULL)
                      buf = smaller_buf;
                  }
              }

            buf[size] = '\0';
            *length = size;
            return buf;
          }

        {
          char *new_buf;
          size_t save_alloc = alloc;

          if (alloc == PTRDIFF_MAX)
            {
              save_errno = ENOMEM;
              break;
            }

          if (alloc < PTRDIFF_MAX - alloc / 2)
            alloc = alloc + alloc / 2;
          else
            alloc = PTRDIFF_MAX;

          if (flags & RF_SENSITIVE)
            {
              new_buf = malloc (alloc);
              if (!new_buf)
                {
                   
                  save_errno = errno;
                  break;
                }
              memcpy (new_buf, buf, save_alloc);
              memset_explicit (buf, 0, save_alloc);
              free (buf);
            }
          else if (!(new_buf = realloc (buf, alloc)))
            {
              save_errno = errno;
              break;
            }

          buf = new_buf;
        }
      }

    if (flags & RF_SENSITIVE)
      memset_explicit (buf, 0, alloc);

    free (buf);
    errno = save_errno;
    return NULL;
  }
}

 
char *
read_file (const char *filename, int flags, size_t *length)
{
  const char *mode = (flags & RF_BINARY) ? "rbe" : "re";
  FILE *stream = fopen (filename, mode);
  char *out;

  if (!stream)
    return NULL;

  if (flags & RF_SENSITIVE)
    setvbuf (stream, NULL, _IONBF, 0);

  out = fread_file (stream, flags, length);

  if (fclose (stream) != 0)
    {
      if (out)
        {
          if (flags & RF_SENSITIVE)
            memset_explicit (out, 0, *length);
          free (out);
        }
      return NULL;
    }

  return out;
}
