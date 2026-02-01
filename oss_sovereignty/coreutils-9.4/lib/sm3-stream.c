 

#include <config.h>

 
#if HAVE_OPENSSL_SM3
# define GL_OPENSSL_INLINE _GL_EXTERN_INLINE
#endif
#include "sm3.h"

#include <stdlib.h>

#if USE_UNLOCKED_IO
# include "unlocked-io.h"
#endif

#define BLOCKSIZE 32768
#if BLOCKSIZE % 64 != 0
# error "invalid BLOCKSIZE"
#endif

 
int
sm3_stream (FILE *stream, void *resblock)
{
  struct sm3_ctx ctx;
  size_t sum;

  char *buffer = malloc (BLOCKSIZE + 72);
  if (!buffer)
    return 1;

   
  sm3_init_ctx (&ctx);

   
  while (1)
    {
       
      size_t n;
      sum = 0;

       
      while (1)
        {
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

           
          if (feof (stream))
            goto process_partial_block;
        }

       
      sm3_process_block (buffer, BLOCKSIZE, &ctx);
    }

 process_partial_block:;

   
  if (sum > 0)
    sm3_process_bytes (buffer, sum, &ctx);

   
  sm3_finish_ctx (&ctx, resblock);
  free (buffer);
  return 0;
}

 
