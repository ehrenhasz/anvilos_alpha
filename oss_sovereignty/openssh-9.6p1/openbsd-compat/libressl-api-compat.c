 

#include "includes.h"

#ifdef WITH_OPENSSL

#include <sys/types.h>

#include <stdlib.h>
#include <string.h>

#include <openssl/evp.h>

#ifndef HAVE_EVP_CIPHER_CTX_GET_IV
int
EVP_CIPHER_CTX_get_iv(const EVP_CIPHER_CTX *ctx, unsigned char *iv, size_t len)
{
	if (ctx == NULL)
		return 0;
	if (EVP_CIPHER_CTX_iv_length(ctx) < 0)
		return 0;
	if (len != (size_t)EVP_CIPHER_CTX_iv_length(ctx))
		return 0;
	if (len > EVP_MAX_IV_LENGTH)
		return 0;  
	 
	if (len != 0) {
		if (iv == NULL)
			return 0;
# ifdef HAVE_EVP_CIPHER_CTX_IV
		memcpy(iv, EVP_CIPHER_CTX_iv(ctx), len);
# else
		memcpy(iv, ctx->iv, len);
# endif  
	}
	return 1;
}
#endif  

#ifndef HAVE_EVP_CIPHER_CTX_SET_IV
int
EVP_CIPHER_CTX_set_iv(EVP_CIPHER_CTX *ctx, const unsigned char *iv, size_t len)
{
	if (ctx == NULL)
		return 0;
	if (EVP_CIPHER_CTX_iv_length(ctx) < 0)
		return 0;
	if (len != (size_t)EVP_CIPHER_CTX_iv_length(ctx))
		return 0;
	if (len > EVP_MAX_IV_LENGTH)
		return 0;  
	 
	if (len != 0) {
		if (iv == NULL)
			return 0;
# ifdef HAVE_EVP_CIPHER_CTX_IV_NOCONST
		memcpy(EVP_CIPHER_CTX_iv_noconst(ctx), iv, len);
# else
		memcpy(ctx->iv, iv, len);
# endif  
	}
	return 1;
}
#endif  

#endif  
