


#ifndef _SHA_H_
#define _SHA_H_

#include <crypto/scatterwalk.h>
#include <crypto/sha1.h>
#include <crypto/sha2.h>

#include "common.h"
#include "core.h"

#define QCE_SHA_MAX_BLOCKSIZE		SHA256_BLOCK_SIZE
#define QCE_SHA_MAX_DIGESTSIZE		SHA256_DIGEST_SIZE

struct qce_sha_ctx {
	u8 authkey[QCE_SHA_MAX_BLOCKSIZE];
};


struct qce_sha_reqctx {
	u8 buf[QCE_SHA_MAX_BLOCKSIZE];
	u8 tmpbuf[QCE_SHA_MAX_BLOCKSIZE];
	u8 digest[QCE_SHA_MAX_DIGESTSIZE];
	unsigned int buflen;
	unsigned long flags;
	struct scatterlist *src_orig;
	unsigned int nbytes_orig;
	int src_nents;
	__be32 byte_count[2];
	u64 count;
	bool first_blk;
	bool last_blk;
	struct scatterlist sg[2];
	u8 *authkey;
	unsigned int authklen;
	struct scatterlist result_sg;
};

static inline struct qce_alg_template *to_ahash_tmpl(struct crypto_tfm *tfm)
{
	struct crypto_ahash *ahash = __crypto_ahash_cast(tfm);
	struct ahash_alg *alg = container_of(crypto_hash_alg_common(ahash),
					     struct ahash_alg, halg);

	return container_of(alg, struct qce_alg_template, alg.ahash);
}

extern const struct qce_algo_ops ahash_ops;

#endif 
