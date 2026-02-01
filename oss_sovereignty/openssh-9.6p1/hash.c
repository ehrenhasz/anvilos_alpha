 
 

#include "includes.h"

#include "crypto_api.h"

#include <stdarg.h>

#ifdef WITH_OPENSSL
#include <openssl/evp.h>

int
crypto_hash_sha512(unsigned char *out, const unsigned char *in,
    unsigned long long inlen)
{

	if (!EVP_Digest(in, inlen, out, NULL, EVP_sha512(), NULL))
		return -1;
	return 0;
}

#else
# ifdef HAVE_SHA2_H
#  include <sha2.h>
# endif

int
crypto_hash_sha512(unsigned char *out, const unsigned char *in,
    unsigned long long inlen)
{

	SHA2_CTX ctx;

	SHA512Init(&ctx);
	SHA512Update(&ctx, in, inlen);
	SHA512Final(out, &ctx);
	return 0;
}
#endif  
