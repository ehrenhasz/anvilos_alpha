 
#if !_GL_CONFIG_H_INCLUDED
 #error "Please include config.h first."
#endif

 
# include <idx.h>

# ifdef __cplusplus
extern "C" {
# endif

 
# define BASE64_LENGTH(inlen) ((((inlen) + 2) / 3) * 4)

struct base64_decode_context
{
  int i;
  char buf[4];
};

extern bool isbase64 (char ch) _GL_ATTRIBUTE_CONST;

extern void base64_encode (const char *restrict in, idx_t inlen,
                           char *restrict out, idx_t outlen);

extern idx_t base64_encode_alloc (const char *in, idx_t inlen, char **out);

extern void base64_decode_ctx_init (struct base64_decode_context *ctx);

extern bool base64_decode_ctx (struct base64_decode_context *ctx,
                               const char *restrict in, idx_t inlen,
                               char *restrict out, idx_t *outlen);

extern bool base64_decode_alloc_ctx (struct base64_decode_context *ctx,
                                     const char *in, idx_t inlen,
                                     char **out, idx_t *outlen);

#define base64_decode(in, inlen, out, outlen) \
        base64_decode_ctx (NULL, in, inlen, out, outlen)

#define base64_decode_alloc(in, inlen, out, outlen) \
        base64_decode_alloc_ctx (NULL, in, inlen, out, outlen)

# ifdef __cplusplus
}
# endif

#endif  
