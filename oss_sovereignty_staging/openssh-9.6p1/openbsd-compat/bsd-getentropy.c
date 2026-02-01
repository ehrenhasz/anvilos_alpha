 

#include "includes.h"

#ifndef SSH_RANDOM_DEV
# define SSH_RANDOM_DEV "/dev/urandom"
#endif  

#include <sys/types.h>
#ifdef HAVE_SYS_RANDOM_H
# include <sys/random.h>
#endif

#include <fcntl.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#ifdef WITH_OPENSSL
#include <openssl/rand.h>
#include <openssl/err.h>
#endif

#include "log.h"

int
_ssh_compat_getentropy(void *s, size_t len)
{
#if defined(WITH_OPENSSL) && defined(OPENSSL_PRNG_ONLY)
	if (RAND_bytes(s, len) <= 0)
		fatal("Couldn't obtain random bytes (error 0x%lx)",
		    (unsigned long)ERR_get_error());
#else
	int fd, save_errno;
	ssize_t r;
	size_t o = 0;

#ifdef WITH_OPENSSL
	if (RAND_bytes(s, len) == 1)
		return 0;
#endif
#ifdef HAVE_GETENTROPY
	if ((r = getentropy(s, len)) == 0)
		return 0;
#endif  
#ifdef HAVE_GETRANDOM
	if ((r = getrandom(s, len, 0)) > 0 && (size_t)r == len)
		return 0;
#endif  

	if ((fd = open(SSH_RANDOM_DEV, O_RDONLY)) == -1) {
		save_errno = errno;
		 
		if (seed_from_prngd(s, len) == 0)
			return 0;
		fatal("Couldn't open %s: %s", SSH_RANDOM_DEV,
		    strerror(save_errno));
	}
	while (o < len) {
		r = read(fd, (u_char *)s + o, len - o);
		if (r < 0) {
			if (errno == EAGAIN || errno == EINTR ||
			    errno == EWOULDBLOCK)
				continue;
			fatal("read %s: %s", SSH_RANDOM_DEV, strerror(errno));
		}
		o += r;
	}
	close(fd);
#endif  
	return 0;
}
