 

 

#ifndef CIPHER_H
#define CIPHER_H

#include <sys/types.h>
#ifdef WITH_OPENSSL
#include <openssl/evp.h>
#endif
#include "cipher-chachapoly.h"
#include "cipher-aesctr.h"

#define CIPHER_ENCRYPT		1
#define CIPHER_DECRYPT		0

struct sshcipher;
struct sshcipher_ctx;

const struct sshcipher *cipher_by_name(const char *);
const char *cipher_warning_message(const struct sshcipher_ctx *);
int	 ciphers_valid(const char *);
char	*cipher_alg_list(char, int);
const char *compression_alg_list(int);
int	 cipher_init(struct sshcipher_ctx **, const struct sshcipher *,
    const u_char *, u_int, const u_char *, u_int, int);
int	 cipher_crypt(struct sshcipher_ctx *, u_int, u_char *, const u_char *,
    u_int, u_int, u_int);
int	 cipher_get_length(struct sshcipher_ctx *, u_int *, u_int,
    const u_char *, u_int);
void	 cipher_free(struct sshcipher_ctx *);
u_int	 cipher_blocksize(const struct sshcipher *);
u_int	 cipher_keylen(const struct sshcipher *);
u_int	 cipher_seclen(const struct sshcipher *);
u_int	 cipher_authlen(const struct sshcipher *);
u_int	 cipher_ivlen(const struct sshcipher *);
u_int	 cipher_is_cbc(const struct sshcipher *);

u_int	 cipher_ctx_is_plaintext(struct sshcipher_ctx *);

int	 cipher_get_keyiv(struct sshcipher_ctx *, u_char *, size_t);
int	 cipher_set_keyiv(struct sshcipher_ctx *, const u_char *, size_t);

#endif				 
