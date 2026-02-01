 

#include <config.h>

 
#if HAVE_OPENSSL_SHA256
# define GL_OPENSSL_INLINE _GL_EXTERN_INLINE
#endif
#include "sha256.h"

#include <stdlib.h>

#if USE_UNLOCKED_IO
# include "unlocked-io.h"
#endif

#include "af_alg.h"

#define BLOCKSIZE 32768
#if BLOCKSIZE % 64 != 0
# error "invalid BLOCKSIZE"
#endif

 
static int
shaxxx_stream (FILE *stream, char const *alg, void *resblock,
               ssize_t hashlen, void (*init_ctx) (struct sha256_ctx *),
               void *(*finish_ctx) (struct sha256_ctx *, void *))
{
  switch (afalg_stream (stream, alg, resblock, hashlen))
    {
    case 0: return 0;
    case -EIO: return 1;
    }

  char *buffer = malloc (BLOCKSIZE + 72);
  if (!buffer)
    return 1;

  struct sha256_ctx ctx;
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

       
      sha256_process_block (buffer, BLOCKSIZE, &ctx);
    }

 process_partial_block:;

   
  if (sum > 0)
    sha256_process_bytes (buffer, sum, &ctx);

   
  finish_ctx (&ctx, resblock);
  free (buffer);
  return 0;
}

int
sha256_stream (FILE *stream, void *resblock)
{
  return shaxxx_stream (stream, "sha256", resblock, SHA256_DIGEST_SIZE,
                        sha256_init_ctx, sha256_finish_ctx);
}

int
sha224_stream (FILE *stream, void *resblock)
{
  return shaxxx_stream (stream, "sha224", resblock, SHA224_DIGEST_SIZE,
                        sha224_init_ctx, sha224_finish_ctx);
}

 
