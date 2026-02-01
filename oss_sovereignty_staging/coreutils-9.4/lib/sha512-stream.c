 

#include <config.h>

 
#if HAVE_OPENSSL_SHA512
# define GL_OPENSSL_INLINE _GL_EXTERN_INLINE
#endif
#include "sha512.h"

#include <stdlib.h>

#if USE_UNLOCKED_IO
# include "unlocked-io.h"
#endif

#include "af_alg.h"

#define BLOCKSIZE 32768
#if BLOCKSIZE % 128 != 0
# error "invalid BLOCKSIZE"
#endif

 
static int
shaxxx_stream (FILE *stream, char const *alg, void *resblock,
               ssize_t hashlen, void (*init_ctx) (struct sha512_ctx *),
               void *(*finish_ctx) (struct sha512_ctx *, void *))
{
  switch (afalg_stream (stream, alg, resblock, hashlen))
    {
    case 0: return 0;
    case -EIO: return 1;
    }

  char *buffer = malloc (BLOCKSIZE + 72);
  if (!buffer)
    return 1;

  struct sha512_ctx ctx;
  init_ctx (&ctx);
  size_t sum;

   
  while (1)
    {
       
      size_t n;
      sum = 0;

       
      while (1)
        {
           
          if (feof (stream))
            goto process_partial_block;

          n = fread (buffer + sum, 1, BLOCKSIZE - sum, stream);

          sum += n;

          if (sum == BLOCKSIZE)
            break;

          if (n == 0)
            {
               
              if (ferror (stream))
                {
                  free (buffer);
                  return 1;
                }
              goto process_partial_block;
            }
        }

       
      sha512_process_block (buffer, BLOCKSIZE, &ctx);
    }

 process_partial_block:;

   
  if (sum > 0)
    sha512_process_bytes (buffer, sum, &ctx);

   
  finish_ctx (&ctx, resblock);
  free (buffer);
  return 0;
}

int
sha512_stream (FILE *stream, void *resblock)
{
  return shaxxx_stream (stream, "sha512", resblock, SHA512_DIGEST_SIZE,
                        sha512_init_ctx, sha512_finish_ctx);
}

int
sha384_stream (FILE *stream, void *resblock)
{
  return shaxxx_stream (stream, "sha384", resblock, SHA384_DIGEST_SIZE,
                        sha384_init_ctx, sha384_finish_ctx);
}

 
