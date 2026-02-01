 

#include <config.h>

 
#if HAVE_OPENSSL_SHA1
# define GL_OPENSSL_INLINE _GL_EXTERN_INLINE
#endif
#include "sha1.h"

#include <stdlib.h>

#if USE_UNLOCKED_IO
# include "unlocked-io.h"
#endif

#include "af_alg.h"

#define BLOCKSIZE 32768
#if BLOCKSIZE % 64 != 0
# error "invalid BLOCKSIZE"
#endif

 
int
sha1_stream (FILE *stream, void *resblock)
{
  switch (afalg_stream (stream, "sha1", resblock, SHA1_DIGEST_SIZE))
    {
    case 0: return 0;
    case -EIO: return 1;
    }

  char *buffer = malloc (BLOCKSIZE + 72);
  if (!buffer)
    return 1;

  struct sha1_ctx ctx;
  sha1_init_ctx (&ctx);
  size_t sum;

   
  while (1)
    {
       
      size_t n;
      sum = 0;

       
      while (1)
        {
           
              if (ferror (stream))
                {
                  free (buffer);
                  return 1;
                }
              goto process_partial_block;
            }
        }

       
      sha1_process_block (buffer, BLOCKSIZE, &ctx);
    }

 process_partial_block:;

   
  if (sum > 0)
    sha1_process_bytes (buffer, sum, &ctx);

   
  sha1_finish_ctx (&ctx, resblock);
  free (buffer);
  return 0;
}

 
