 

#ifndef _OPENSSL_COMPAT_H
#define _OPENSSL_COMPAT_H

#include "includes.h"
#ifdef WITH_OPENSSL

#include <openssl/opensslv.h>
#include <openssl/crypto.h>
#include <openssl/evp.h>
#include <openssl/rsa.h>
#include <openssl/dsa.h>
#ifdef OPENSSL_HAS_ECC
#include <openssl/ecdsa.h>
#endif
#include <openssl/dh.h>

int ssh_compatible_openssl(long, long);
void ssh_libcrypto_init(void);

#if (OPENSSL_VERSION_NUMBER < 0x10100000L)
# error OpenSSL 1.1.0 or greater is required
#endif
#ifdef LIBRESSL_VERSION_NUMBER
# if LIBRESSL_VERSION_NUMBER < 0x3010000fL
#  error LibreSSL 3.1.0 or greater is required
# endif
#endif

#ifndef OPENSSL_RSA_MAX_MODULUS_BITS
# define OPENSSL_RSA_MAX_MODULUS_BITS	16384
#endif
#ifndef OPENSSL_DSA_MAX_MODULUS_BITS
# define OPENSSL_DSA_MAX_MODULUS_BITS	10000
#endif

#ifdef LIBRESSL_VERSION_NUMBER
# if LIBRESSL_VERSION_NUMBER < 0x3010000fL
#  define HAVE_BROKEN_CHACHA20
# endif
#endif

#ifdef OPENSSL_IS_BORINGSSL
 
# define BN_set_flags(a, b)
#endif

#ifndef HAVE_EVP_CIPHER_CTX_GET_IV
# ifdef HAVE_EVP_CIPHER_CTX_GET_UPDATED_IV
#  define EVP_CIPHER_CTX_get_iv EVP_CIPHER_CTX_get_updated_iv
# else  
int EVP_CIPHER_CTX_get_iv(const EVP_CIPHER_CTX *ctx,
    unsigned char *iv, size_t len);
# endif  
#endif  

#ifndef HAVE_EVP_CIPHER_CTX_SET_IV
int EVP_CIPHER_CTX_set_iv(EVP_CIPHER_CTX *ctx,
    const unsigned char *iv, size_t len);
#endif  

#endif  
#endif  
