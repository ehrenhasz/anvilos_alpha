 
#ifndef AXTLS_OS_PORT_H
#define AXTLS_OS_PORT_H

#ifndef __ets__
#include <arpa/inet.h>
#endif
#include <errno.h>
#include <sys/time.h>
#include "py/stream.h"
#include "lib/crypto-algorithms/sha256.h"

#define SSL_CTX_MUTEX_INIT(mutex)
#define SSL_CTX_MUTEX_DESTROY(mutex)
#define SSL_CTX_LOCK(mutex)
#define SSL_CTX_UNLOCK(mutex)

#define SOCKET_READ(s, buf, size)       mp_stream_posix_read((void *)s, buf, size)
#define SOCKET_WRITE(s, buf, size)      mp_stream_posix_write((void *)s, buf, size)
#define SOCKET_CLOSE(A)                 UNUSED
#define SOCKET_ERRNO()                  errno

#define SHA256_CTX                      CRYAL_SHA256_CTX
#define SHA256_Init(ctx)                sha256_init(ctx)
#define SHA256_Update(ctx, buf, size)   sha256_update(ctx, buf, size)
#define SHA256_Final(hash, ctx)         sha256_final(ctx, hash)

#define TTY_FLUSH()

#ifdef WDEV_HWRNG

#define PLATFORM_RNG_U8()               (*WDEV_HWRNG)
#endif

#endif 
