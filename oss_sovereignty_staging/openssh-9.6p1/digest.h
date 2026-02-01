 
 

#ifndef _DIGEST_H
#define _DIGEST_H

 
#define SSH_DIGEST_MAX_LENGTH	64

 
#define SSH_DIGEST_MD5		0
#define SSH_DIGEST_SHA1		1
#define SSH_DIGEST_SHA256	2
#define SSH_DIGEST_SHA384	3
#define SSH_DIGEST_SHA512	4
#define SSH_DIGEST_MAX		5

struct sshbuf;
struct ssh_digest_ctx;

 
int ssh_digest_alg_by_name(const char *name);

 
const char *ssh_digest_alg_name(int alg);

 
size_t ssh_digest_bytes(int alg);

 
size_t ssh_digest_blocksize(struct ssh_digest_ctx *ctx);

 
int ssh_digest_copy_state(struct ssh_digest_ctx *from,
    struct ssh_digest_ctx *to);

 
int ssh_digest_memory(int alg, const void *m, size_t mlen,
    u_char *d, size_t dlen)
	__attribute__((__bounded__(__buffer__, 2, 3)))
	__attribute__((__bounded__(__buffer__, 4, 5)));
int ssh_digest_buffer(int alg, const struct sshbuf *b, u_char *d, size_t dlen)
	__attribute__((__bounded__(__buffer__, 3, 4)));

 
struct ssh_digest_ctx *ssh_digest_start(int alg);
int ssh_digest_update(struct ssh_digest_ctx *ctx, const void *m, size_t mlen)
	__attribute__((__bounded__(__buffer__, 2, 3)));
int ssh_digest_update_buffer(struct ssh_digest_ctx *ctx,
    const struct sshbuf *b);
int ssh_digest_final(struct ssh_digest_ctx *ctx, u_char *d, size_t dlen)
	__attribute__((__bounded__(__buffer__, 2, 3)));
void ssh_digest_free(struct ssh_digest_ctx *ctx);

#endif  

