 

#include <config.h>
#include <stdio.h>
#include <stdint.h>
#include "system.h"

 
#undef fprintf
#undef free
#undef malloc
#undef strtoumax

 

static char const *
fileno_to_name (const int fd)
{
  char const *ret = nullptr;

  switch (fd)
    {
    case 0:
      ret = "stdin";
      break;
    case 1:
      ret = "stdout";
      break;
    case 2:
      ret = "stderr";
      break;
    default:
      ret = "unknown";
      break;
    }

  return ret;
}

static void
apply_mode (FILE *stream, char const *mode)
{
  char *buf = nullptr;
  int setvbuf_mode;
  uintmax_t size = 0;

  if (*mode == '0')
    setvbuf_mode = _IONBF;
  else if (*mode == 'L')
    setvbuf_mode = _IOLBF;       
  else
    {
      setvbuf_mode = _IOFBF;
      char *mode_end;
      size = strtoumax (mode, &mode_end, 10);
      if (size == 0 || *mode_end)
        {
          fprintf (stderr, _("invalid buffering mode %s for %s\n"),
                   mode, fileno_to_name (fileno (stream)));
          return;
        }

      buf = size <= SIZE_MAX ? malloc (size) : nullptr;
      if (!buf)
        {
           
          fprintf (stderr,
                   _("failed to allocate a %" PRIuMAX
                     " byte stdio buffer\n"),
                   size);
          return;
        }
       
    }

  if (setvbuf (stream, buf, setvbuf_mode, size) != 0)
    {
      fprintf (stderr, _("could not set buffering of %s to mode %s\n"),
               fileno_to_name (fileno (stream)), mode);
      free (buf);
    }
}

 
static void __attribute ((constructor))
stdbuf (void)
{
  char *e_mode = getenv ("_STDBUF_E");
  char *i_mode = getenv ("_STDBUF_I");
  char *o_mode = getenv ("_STDBUF_O");
  if (e_mode)  
    apply_mode (stderr, e_mode);
  if (i_mode)
    apply_mode (stdin, i_mode);
  if (o_mode)
    apply_mode (stdout, o_mode);
}
