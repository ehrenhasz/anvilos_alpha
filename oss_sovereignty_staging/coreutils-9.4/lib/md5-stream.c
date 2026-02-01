 

#include <config.h>

 
#if HAVE_OPENSSL_MD5
# define GL_OPENSSL_INLINE _GL_EXTERN_INLINE
#endif
#include "md5.h"

#include <stdlib.h>

#if USE_UNLOCKED_IO
# include "unlocked-io.h"
#endif

#include "af_alg.h"

#ifdef _LIBC
# include <endian.h>
# if __BYTE_ORDER == __BIG_ENDIAN
#  define WORDS_BIGENDIAN 1
# endif
 
# define md5_init_ctx __md5_init_ctx
# define md5_process_block __md5_process_block
# define md5_process_bytes __md5_process_bytes
# define md5_finish_ctx __md5_finish_ctx
# define md5_stream __md5_stream
#endif

#define BLOCKSIZE 32768
#if BLOCKSIZE % 64 != 0
# error "invalid BLOCKSIZE"
#endif

 
int
md5_stream (FILE *stream, void *resblock)
{
  switch (afalg_stream (stream, "md5", resblock, MD5_DIGEST_SIZE))
    {
    case 0: return 0;
    case -EIO: return 1;
    }

  char *buffer = malloc (BLOCKSIZE + 72);
  if (!buffer)
    return 1;

  struct md5_ctx ctx;
  md5_init_ctx (&ctx);
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

       
      md5_process_block (buffer, BLOCKSIZE, &ctx);
    }

process_partial_block:

   
  if (sum > 0)
    md5_process_bytes (buffer, sum, &ctx);

   
  md5_finish_ctx (&ctx, resblock);
  free (buffer);
  return 0;
}

 
