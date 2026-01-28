#ifndef _CRYPTO_ARCH_S390_SHA_H
#define _CRYPTO_ARCH_S390_SHA_H
#include <linux/crypto.h>
#include <crypto/sha1.h>
#include <crypto/sha2.h>
#include <crypto/sha3.h>
#define SHA3_STATE_SIZE			200
#define CPACF_MAX_PARMBLOCK_SIZE	SHA3_STATE_SIZE
#define SHA_MAX_BLOCK_SIZE		SHA3_224_BLOCK_SIZE
struct s390_sha_ctx {
	u64 count;		 
	u32 state[CPACF_MAX_PARMBLOCK_SIZE / sizeof(u32)];
	u8 buf[SHA_MAX_BLOCK_SIZE];
	int func;		 
};
struct shash_desc;
int s390_sha_update(struct shash_desc *desc, const u8 *data, unsigned int len);
int s390_sha_final(struct shash_desc *desc, u8 *out);
#endif
