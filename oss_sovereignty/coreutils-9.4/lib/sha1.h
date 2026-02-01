 
# if !_GL_CONFIG_H_INCLUDED
#  error "Please include config.h first."
# endif

# include <stdio.h>
# include <stdint.h>

# if HAVE_OPENSSL_SHA1
#  ifndef OPENSSL_API_COMPAT
#   define OPENSSL_API_COMPAT 0x10101000L  
#  endif
 
#  include <openssl/configuration.h>
#  if (OPENSSL_CONFIGURED_API \
       < (OPENSSL_API_COMPAT < 0x900000L ? OPENSSL_API_COMPAT : \
          ((OPENSSL_API_COMPAT >> 28) & 0xF) * 10000 \
          + ((OPENSSL_API_COMPAT >> 20) & 0xFF) * 100 \
          + ((OPENSSL_API_COMPAT >> 12) & 0xFF)))
#   undef HAVE_OPENSSL_SHA1
#  else
#   include <openssl/sha.h>
#  endif
# endif

# ifdef __cplusplus
extern "C" {
# endif

# define SHA1_DIGEST_SIZE 20

# if HAVE_OPENSSL_SHA1
#  define GL_OPENSSL_NAME 1
#  include "gl_openssl.h"
# else
 
struct sha1_ctx
{
  uint32_t A;
  uint32_t B;
  uint32_t C;
  uint32_t D;
  uint32_t E;

  uint32_t total[2];
  uint32_t buflen;      
  uint32_t buffer[32];  
};

 
extern void sha1_init_ctx (struct sha1_ctx *ctx);

 
extern void sha1_process_block (const void *buffer, size_t len,
                                struct sha1_ctx *ctx);

 
extern void sha1_process_bytes (const void *buffer, size_t len,
                                struct sha1_ctx *ctx);

 
extern void *sha1_finish_ctx (struct sha1_ctx *ctx, void *restrict resbuf);


 
extern void *sha1_read_ctx (const struct sha1_ctx *ctx, void *restrict resbuf);


 
extern void *sha1_buffer (const char *buffer, size_t len,
                          void *restrict resblock);

# endif

 
extern int sha1_stream (FILE *stream, void *resblock);


# ifdef __cplusplus
}
# endif

#endif

 
