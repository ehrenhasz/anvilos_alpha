


#ifndef _HMAC_H
#define _HMAC_H


size_t ssh_hmac_bytes(int alg);

struct sshbuf;
struct ssh_hmac_ctx;
struct ssh_hmac_ctx *ssh_hmac_start(int alg);


int ssh_hmac_init(struct ssh_hmac_ctx *ctx, const void *key, size_t klen)
	__attribute__((__bounded__(__buffer__, 2, 3)));
int ssh_hmac_update(struct ssh_hmac_ctx *ctx, const void *m, size_t mlen)
	__attribute__((__bounded__(__buffer__, 2, 3)));
int ssh_hmac_update_buffer(struct ssh_hmac_ctx *ctx, const struct sshbuf *b);
int ssh_hmac_final(struct ssh_hmac_ctx *ctx, u_char *d, size_t dlen)
	__attribute__((__bounded__(__buffer__, 2, 3)));
void ssh_hmac_free(struct ssh_hmac_ctx *ctx);

#endif 
