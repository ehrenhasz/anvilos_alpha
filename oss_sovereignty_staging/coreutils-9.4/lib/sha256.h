 
# if !_GL_CONFIG_H_INCLUDED
#  error "Please include config.h first."
# endif

# include <stdio.h>
# include <stdint.h>

# if HAVE_OPENSSL_SHA256
#  ifndef OPENSSL_API_COMPAT
#   define OPENSSL_API_COMPAT 0x10101000L  
#  endif
 
#  include <openssl/configuration.h>
#  if (OPENSSL_CONFIGURED_API \
       < (OPENSSL_API_COMPAT < 0x900000L ? OPENSSL_API_COMPAT : \
          ((OPENSSL_API_COMPAT >> 28) & 0xF) * 10000 \
          + ((OPENSSL_API_COMPAT >> 20) & 0xFF) * 100 \
          + ((OPENSSL_API_COMPAT >> 12) & 0xFF)))
#   undef HAVE_OPENSSL_SHA256
#  else
#   include <openssl/sha.h>
#  endif
# endif

# ifdef __cplusplus
extern "C" {
# endif

enum { SHA224_DIGEST_SIZE = 224 / 8 };
enum { SHA256_DIGEST_SIZE = 256 / 8 };

# if HAVE_OPENSSL_SHA256
#  define GL_OPENSSL_NAME 224
#  include "gl_openssl.h"
#  define GL_OPENSSL_NAME 256
#  include "gl_openssl.h"
# else
 
struct sha256_ctx
{
  uint32_t state[8];

  uint32_t total[2];
  size_t buflen;        
  uint32_t buffer[32];  
};

 
extern void sha256_init_ctx (struct sha256_ctx *ctx);
extern void sha224_init_ctx (struct sha256_ctx *ctx);

 
extern void sha256_process_block (const void *buffer, size_t len,
                                  struct sha256_ctx *ctx);

 
extern void sha256_process_bytes (const void *buffer, size_t len,
                                  struct sha256_ctx *ctx);

 
extern void *sha256_finish_ctx (struct sha256_ctx *ctx, void *restrict resbuf);
extern void *sha224_finish_ctx (struct sha256_ctx *ctx, void *restrict resbuf);


 
extern void *sha256_read_ctx (const struct sha256_ctx *ctx,
                              void *restrict resbuf);
extern void *sha224_read_ctx (const struct sha256_ctx *ctx,
                              void *restrict resbuf);


 
extern void *sha256_buffer (const char *buffer, size_t len,
                            void *restrict resblock);
extern void *sha224_buffer (const char *buffer, size_t len,
                            void *restrict resblock);

# endif

 
extern int sha256_stream (FILE *stream, void *resblock);
extern int sha224_stream (FILE *stream, void *resblock);


# ifdef __cplusplus
}
# endif

#endif

 
