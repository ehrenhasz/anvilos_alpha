


#ifndef _CIPHER_H_
#define _CIPHER_H_

#include "common.h"
#include "core.h"

#define QCE_MAX_KEY_SIZE	64

struct qce_cipher_ctx {
	u8 enc_key[QCE_MAX_KEY_SIZE];
	unsigned int enc_keylen;
	struct crypto_skcipher *fallback;
};


struct qce_cipher_reqctx {
	unsigned long flags;
	u8 *iv;
	unsigned int ivsize;
	int src_nents;
	int dst_nents;
	struct scatterlist result_sg;
	struct sg_table dst_tbl;
	struct scatterlist *dst_sg;
	struct scatterlist *src_sg;
	unsigned int cryptlen;
	struct skcipher_request fallback_req;	
};

static inline struct qce_alg_template *to_cipher_tmpl(struct crypto_skcipher *tfm)
{
	struct skcipher_alg *alg = crypto_skcipher_alg(tfm);
	return container_of(alg, struct qce_alg_template, alg.skcipher);
}

extern const struct qce_algo_ops skcipher_ops;

#endif 
