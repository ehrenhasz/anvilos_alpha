 
# if !_GL_CONFIG_H_INCLUDED
#  error "Please include config.h first."
# endif

# include <stdio.h>
# include "u64.h"

# if HAVE_OPENSSL_SHA512
#  ifndef OPENSSL_API_COMPAT
#   define OPENSSL_API_COMPAT 0x10101000L  
#  endif
 
#  include <openssl/configuration.h>
#  if (OPENSSL_CONFIGURED_API \
       < (OPENSSL_API_COMPAT < 0x900000L ? OPENSSL_API_COMPAT : \
          ((OPENSSL_API_COMPAT >> 28) & 0xF) * 10000 \
          + ((OPENSSL_API_COMPAT >> 20) & 0xFF) * 100 \
          + ((OPENSSL_API_COMPAT >> 12) & 0xFF)))
#   undef HAVE_OPENSSL_SHA512
#  else
#   include <openssl/sha.h>
#  endif
# endif

# ifdef __cplusplus
extern "C" {
# endif

enum { SHA384_DIGEST_SIZE = 384 / 8 };
enum { SHA512_DIGEST_SIZE = 512 / 8 };

# if HAVE_OPENSSL_SHA512
#  define GL_OPENSSL_NAME 384
#  include "gl_openssl.h"
#  define GL_OPENSSL_NAME 512
#  include "gl_openssl.h"
# else
 
struct sha512_ctx
{
  u64 state[8];

  u64 total[2];
  size_t buflen;   
  u64 buffer[32];  
};

 
extern void sha512_init_ctx (struct sha512_ctx *ctx);
extern void sha384_init_ctx (struct sha512_ctx *ctx);

 
extern void sha512_process_block (const void *buffer, size_t len,
                                  struct sha512_ctx *ctx);

 
extern void sha512_process_bytes (const void *buffer, size_t len,
                                  struct sha512_ctx *ctx);

 
extern void *sha512_finish_ctx (struct sha512_ctx *ctx, void *restrict resbuf);
extern void *sha384_finish_ctx (struct sha512_ctx *ctx, void *restrict resbuf);


 
extern void *sha512_read_ctx (const struct sha512_ctx *ctx,
                              void *restrict resbuf);
extern void *sha384_read_ctx (const struct sha512_ctx *ctx,
                              void *restrict resbuf);


 
extern void *sha512_buffer (const char *buffer, size_t len,
                            void *restrict resblock);
extern void *sha384_buffer (const char *buffer, size_t len,
                            void *restrict resblock);

# endif

 
extern int sha512_stream (FILE *stream, void *resblock);
extern int sha384_stream (FILE *stream, void *resblock);


# ifdef __cplusplus
}
# endif

#endif

 
