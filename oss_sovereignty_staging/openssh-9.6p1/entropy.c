 

#include "includes.h"

#define RANDOM_SEED_SIZE 48

#ifdef WITH_OPENSSL

#include <sys/types.h>

#include <errno.h>
#include <signal.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <openssl/rand.h>
#include <openssl/crypto.h>
#include <openssl/err.h>

#include "openbsd-compat/openssl-compat.h"

#include "ssh.h"
#include "misc.h"
#include "xmalloc.h"
#include "atomicio.h"
#include "pathnames.h"
#include "log.h"
#include "sshbuf.h"
#include "ssherr.h"

 

void
seed_rng(void)
{
	unsigned char buf[RANDOM_SEED_SIZE];

	 
	ssh_libcrypto_init();

	if (!ssh_compatible_openssl(OPENSSL_VERSION_NUMBER,
	    OpenSSL_version_num()))
		fatal("OpenSSL version mismatch. Built against %lx, you "
		    "have %lx", (u_long)OPENSSL_VERSION_NUMBER,
		    OpenSSL_version_num());

#ifndef OPENSSL_PRNG_ONLY
	if (RAND_status() == 1)
		debug3("RNG is ready, skipping seeding");
	else {
		if (seed_from_prngd(buf, sizeof(buf)) == -1)
			fatal("Could not obtain seed from PRNGd");
		RAND_add(buf, sizeof(buf), sizeof(buf));
	}
#endif  

	if (RAND_status() != 1)
		fatal("PRNG is not seeded");

	 
	arc4random_buf(buf, sizeof(buf));
	explicit_bzero(buf, sizeof(buf));
}

#else  

#include <stdlib.h>
#include <string.h>

 
void
seed_rng(void)
{
	unsigned char buf[RANDOM_SEED_SIZE];

	 
	arc4random_buf(buf, sizeof(buf));
	explicit_bzero(buf, sizeof(buf));
}

#endif  
