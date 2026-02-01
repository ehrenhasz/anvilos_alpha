 
#if !_GL_CONFIG_H_INCLUDED
 #error "Please include config.h first."
#endif

#include <stdio.h>
#include <stdint.h>

# if HAVE_OPENSSL_MD5
#  ifndef OPENSSL_API_COMPAT
#   define OPENSSL_API_COMPAT 0x10101000L  
#  endif
 
#  include <openssl/configuration.h>
#  if (OPENSSL_CONFIGURED_API \
       < (OPENSSL_API_COMPAT < 0x900000L ? OPENSSL_API_COMPAT : \
          ((OPENSSL_API_COMPAT >> 28) & 0xF) * 10000 \
          + ((OPENSSL_API_COMPAT >> 20) & 0xFF) * 100 \
          + ((OPENSSL_API_COMPAT >> 12) & 0xFF)))
#   undef HAVE_OPENSSL_MD5
#  else
#   include <openssl/md5.h>
#  endif
# endif

#define MD5_DIGEST_SIZE 16
#define MD5_BLOCK_SIZE 64

#ifndef __GNUC_PREREQ
# if defined __GNUC__ && defined __GNUC_MINOR__
#  define __GNUC_PREREQ(maj, min)                                       \
  ((__GNUC__ << 16) + __GNUC_MINOR__ >= ((maj) << 16) + (min))
# else
#  define __GNUC_PREREQ(maj, min) 0
# endif
#endif

#ifndef __THROW
# if defined __cplusplus && (__GNUC_PREREQ (2,8) || __clang_major__ >= 4)
#  define __THROW       throw ()
# else
#  define __THROW
# endif
#endif

#ifndef _LIBC
# define __md5_buffer md5_buffer
# define __md5_finish_ctx md5_finish_ctx
# define __md5_init_ctx md5_init_ctx
# define __md5_process_block md5_process_block
# define __md5_process_bytes md5_process_bytes
# define __md5_read_ctx md5_read_ctx
# define __md5_stream md5_stream
#endif

# ifdef __cplusplus
extern "C" {
# endif

# if HAVE_OPENSSL_MD5
#  define GL_OPENSSL_NAME 5
#  include "gl_openssl.h"
# else
 
struct md5_ctx
{
  uint32_t A;
  uint32_t B;
  uint32_t C;
  uint32_t D;

  uint32_t total[2];
  uint32_t buflen;      
  uint32_t buffer[32];  
};

 

 
extern void __md5_init_ctx (struct md5_ctx *ctx) __THROW;

 
extern void __md5_process_block (const void *buffer, size_t len,
                                 struct md5_ctx *ctx) __THROW;

 
extern void __md5_process_bytes (const void *buffer, size_t len,
                                 struct md5_ctx *ctx) __THROW;

 
extern void *__md5_finish_ctx (struct md5_ctx *ctx, void *restrict resbuf)
     __THROW;


 
extern void *__md5_read_ctx (const struct md5_ctx *ctx, void *restrict resbuf)
     __THROW;


 
extern void *__md5_buffer (const char *buffer, size_t len,
                           void *restrict resblock) __THROW;

# endif

 
extern int __md5_stream (FILE *stream, void *resblock) __THROW;


# ifdef __cplusplus
}
# endif

#endif  

 
