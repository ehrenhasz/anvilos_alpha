 

#define SSH_DONT_OVERLOAD_OPENSSL_FUNCS
#include "includes.h"

#ifdef WITH_OPENSSL

#include <stdarg.h>
#include <string.h>

#ifdef USE_OPENSSL_ENGINE
# include <openssl/engine.h>
# include <openssl/conf.h>
#endif

#include "log.h"

#include "openssl-compat.h"

 

int
ssh_compatible_openssl(long headerver, long libver)
{
	long mask, hfix, lfix;

	 
	if (headerver == libver)
		return 1;

	 
	if (headerver >= 0x3000000f) {
		mask = 0xf000000fL;  
		return (headerver & mask) == (libver & mask);
	}

	 
	mask = 0xfff0000fL;  
	hfix = (headerver & 0x000ff000) >> 12;
	lfix = (libver & 0x000ff000) >> 12;
	if ( (headerver & mask) == (libver & mask) && lfix >= hfix)
		return 1;
	return 0;
}

void
ssh_libcrypto_init(void)
{
#if defined(HAVE_OPENSSL_INIT_CRYPTO) && \
      defined(OPENSSL_INIT_ADD_ALL_CIPHERS) && \
      defined(OPENSSL_INIT_ADD_ALL_DIGESTS)
	OPENSSL_init_crypto(OPENSSL_INIT_ADD_ALL_CIPHERS |
	    OPENSSL_INIT_ADD_ALL_DIGESTS, NULL);
#elif defined(HAVE_OPENSSL_ADD_ALL_ALGORITHMS)
	OpenSSL_add_all_algorithms();
#endif

#ifdef	USE_OPENSSL_ENGINE
	 
	ENGINE_load_builtin_engines();
	ENGINE_register_all_complete();

	 
# if defined(HAVE_OPENSSL_INIT_CRYPTO) && defined(OPENSSL_INIT_LOAD_CONFIG)
	OPENSSL_init_crypto(OPENSSL_INIT_ADD_ALL_CIPHERS |
	    OPENSSL_INIT_ADD_ALL_DIGESTS | OPENSSL_INIT_LOAD_CONFIG, NULL);
# else
	OPENSSL_config(NULL);
# endif
#endif  
}

#endif  
