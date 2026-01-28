


#ifndef CHACHA_POLY_AEAD_H
#define CHACHA_POLY_AEAD_H

#include <sys/types.h>
#include "chacha.h"
#include "poly1305.h"

#define CHACHA_KEYLEN	32 

struct chachapoly_ctx;

struct chachapoly_ctx *chachapoly_new(const u_char *key, u_int keylen)
    __attribute__((__bounded__(__buffer__, 1, 2)));
void chachapoly_free(struct chachapoly_ctx *cpctx);

int	chachapoly_crypt(struct chachapoly_ctx *cpctx, u_int seqnr,
    u_char *dest, const u_char *src, u_int len, u_int aadlen, u_int authlen,
    int do_encrypt);
int	chachapoly_get_length(struct chachapoly_ctx *cpctx,
    u_int *plenp, u_int seqnr, const u_char *cp, u_int len)
    __attribute__((__bounded__(__buffer__, 4, 5)));

#endif 
