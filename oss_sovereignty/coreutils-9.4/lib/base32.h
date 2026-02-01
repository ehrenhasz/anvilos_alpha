 
#if !_GL_CONFIG_H_INCLUDED
 #error "Please include config.h first."
#endif

 
# include <idx.h>

 
# define BASE32_LENGTH(inlen) ((((inlen) + 4) / 5) * 8)

struct base32_decode_context
{
  int i;
  char buf[8];
};

extern bool isbase32 (char ch) _GL_ATTRIBUTE_CONST;

extern void base32_encode (const char *restrict in, idx_t inlen,
                           char *restrict out, idx_t outlen);

extern idx_t base32_encode_alloc (const char *in, idx_t inlen, char **out);

extern void base32_decode_ctx_init (struct base32_decode_context *ctx);

extern bool base32_decode_ctx (struct base32_decode_context *ctx,
                               const char *restrict in, idx_t inlen,
                               char *restrict out, idx_t *outlen);

extern bool base32_decode_alloc_ctx (struct base32_decode_context *ctx,
                                     const char *in, idx_t inlen,
                                     char **out, idx_t *outlen);

#define base32_decode(in, inlen, out, outlen) \
        base32_decode_ctx (NULL, in, inlen, out, outlen)

#define base32_decode_alloc(in, inlen, out, outlen) \
        base32_decode_alloc_ctx (NULL, in, inlen, out, outlen)

#endif  
