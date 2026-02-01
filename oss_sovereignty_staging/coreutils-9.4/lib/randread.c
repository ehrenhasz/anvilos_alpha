 

 

#include <config.h>

#include "randread.h"

#include <errno.h>
#include <error.h>
#include <exitfail.h>
#include <fcntl.h>
#include <quote.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/random.h>

#include "gettext.h"
#define _(msgid) gettext (msgid)

#include "assure.h"
#include "minmax.h"
#include "rand-isaac.h"
#include "stdio-safer.h"
#include "unlocked-io.h"
#include "xalloc.h"

#if _STRING_ARCH_unaligned || _STRING_INLINE_unaligned
# define ALIGNED_POINTER(ptr, type) true
#else
# define ALIGNED_POINTER(ptr, type) ((size_t) (ptr) % alignof (type) == 0)
#endif

 
#define RANDREAD_BUFFER_SIZE (2 * ISAAC_BYTES)

 
struct randread_source
{
   
  FILE *source;

   
  void (*handler) (void const *);
  void const *handler_arg;

   
  union
  {
     
    char c[RANDREAD_BUFFER_SIZE];

     
    struct isaac
    {
       
      size_t buffered;

       
      struct isaac_state state;

       
      union
      {
        isaac_word w[ISAAC_WORDS];
        unsigned char b[ISAAC_BYTES];
      } data;
    } isaac;
  } buf;
};


 

static void
randread_error (void const *file_name)
{
  affirm (exit_failure);
  error (exit_failure, errno,
         errno == 0 ? _("%s: end of file") : _("%s: read error"),
         quote (file_name));
}

 

static struct randread_source *
simple_new (FILE *source, void const *handler_arg)
{
  struct randread_source *s = xmalloc (sizeof *s);
  s->source = source;
  s->handler = randread_error;
  s->handler_arg = handler_arg;
  return s;
}

 

static bool
get_nonce (void *buffer, size_t bufsize)
{
  char *buf = buffer, *buflim = buf + bufsize;
  while (buf < buflim)
    {
#if defined __sun
# define MAX_GETRANDOM 1024
#else
# define MAX_GETRANDOM SIZE_MAX
#endif
      size_t max_bytes = MIN (buflim - buf, MAX_GETRANDOM);
      ssize_t nbytes = getrandom (buf, max_bytes, 0);
      if (0 <= nbytes)
        buf += nbytes;
      else if (errno != EINTR)
        return false;
    }
  return true;
}

 

static int
randread_free_body (struct randread_source *s)
{
  FILE *source = s->source;
  explicit_bzero (s, sizeof *s);
  free (s);
  return source ? fclose (source) : 0;
}

 

struct randread_source *
randread_new (char const *name, size_t bytes_bound)
{
  if (bytes_bound == 0)
    return simple_new (nullptr, nullptr);
  else
    {
      FILE *source = nullptr;
      struct randread_source *s;

      if (name)
        if (! (source = fopen_safer (name, "rb")))
          return nullptr;

      s = simple_new (source, name);

      if (source)
        setvbuf (source, s->buf.c, _IOFBF, MIN (sizeof s->buf.c, bytes_bound));
      else
        {
          s->buf.isaac.buffered = 0;
          if (! get_nonce (s->buf.isaac.state.m,
                           MIN (sizeof s->buf.isaac.state.m, bytes_bound)))
            {
              int e = errno;
              randread_free_body (s);
              errno = e;
              return nullptr;
            }
          isaac_seed (&s->buf.isaac.state);
        }

      return s;
    }
}


 

void
randread_set_handler (struct randread_source *s, void (*handler) (void const *))
{
  s->handler = handler;
}

void
randread_set_handler_arg (struct randread_source *s, void const *handler_arg)
{
  s->handler_arg = handler_arg;
}


 

static void
readsource (struct randread_source *s, unsigned char *p, size_t size)
{
  while (true)
    {
      size_t inbytes = fread (p, sizeof *p, size, s->source);
      int fread_errno = errno;
      p += inbytes;
      size -= inbytes;
      if (size == 0)
        break;
      errno = (ferror (s->source) ? fread_errno : 0);
      s->handler (s->handler_arg);
    }
}


 

static void
readisaac (struct isaac *isaac, void *p, size_t size)
{
  size_t inbytes = isaac->buffered;

  while (true)
    {
      char *char_p = p;

      if (size <= inbytes)
        {
          memcpy (p, isaac->data.b + ISAAC_BYTES - inbytes, size);
          isaac->buffered = inbytes - size;
          return;
        }

      memcpy (p, isaac->data.b + ISAAC_BYTES - inbytes, inbytes);
      p = char_p + inbytes;
      size -= inbytes;

       
      if (ALIGNED_POINTER (p, isaac_word))
        {
          isaac_word *wp = p;
          while (ISAAC_BYTES <= size)
            {
              isaac_refill (&isaac->state, wp);
              wp += ISAAC_WORDS;
              size -= ISAAC_BYTES;
              if (size == 0)
                {
                  isaac->buffered = 0;
                  return;
                }
            }
          p = wp;
        }

      isaac_refill (&isaac->state, isaac->data.w);
      inbytes = ISAAC_BYTES;
    }
}


 

void
randread (struct randread_source *s, void *buf, size_t size)
{
  if (s->source)
    readsource (s, buf, size);
  else
    readisaac (&s->buf.isaac, buf, size);
}


 

int
randread_free (struct randread_source *s)
{
  return randread_free_body (s);
}
