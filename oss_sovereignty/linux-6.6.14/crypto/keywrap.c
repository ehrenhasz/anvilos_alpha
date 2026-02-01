 

 

#include <linux/module.h>
#include <linux/crypto.h>
#include <linux/scatterlist.h>
#include <crypto/scatterwalk.h>
#include <crypto/internal/cipher.h>
#include <crypto/internal/skcipher.h>

struct crypto_kw_block {
#define SEMIBSIZE 8
	__be64 A;
	__be64 R;
};

 
static void crypto_kw_scatterlist_ff(struct scatter_walk *walk,
				     struct scatterlist *sg,
				     unsigned int end)
{
	unsigned int skip = 0;

	 
	BUG_ON(end < SEMIBSIZE);

	skip = end - SEMIBSIZE;
	while (sg) {
		if (sg->length > skip) {
			scatterwalk_start(walk, sg);
			scatterwalk_advance(walk, skip);
			break;
		}

		skip -= sg->length;
		sg = sg_next(sg);
	}
}

static int crypto_kw_decrypt(struct skcipher_request *req)
{
	struct crypto_skcipher *tfm = crypto_skcipher_reqtfm(req);
	struct crypto_cipher *cipher = skcipher_cipher_simple(tfm);
	struct crypto_kw_block block;
	struct scatterlist *src, *dst;
	u64 t = 6 * ((req->cryptlen) >> 3);
	unsigned int i;
	int ret = 0;

	 
	if (req->cryptlen < (2 * SEMIBSIZE) || req->cryptlen % SEMIBSIZE)
		return -EINVAL;

	 
	memcpy(&block.A, req->iv, SEMIBSIZE);

	 
	src = req->src;
	dst = req->dst;

	for (i = 0; i < 6; i++) {
		struct scatter_walk src_walk, dst_walk;
		unsigned int nbytes = req->cryptlen;

		while (nbytes) {
			 
			crypto_kw_scatterlist_ff(&src_walk, src, nbytes);
			 
			scatterwalk_copychunks(&block.R, &src_walk, SEMIBSIZE,
					       false);

			 
			block.A ^= cpu_to_be64(t);
			t--;
			 
			crypto_cipher_decrypt_one(cipher, (u8 *)&block,
						  (u8 *)&block);

			 
			crypto_kw_scatterlist_ff(&dst_walk, dst, nbytes);
			 
			scatterwalk_copychunks(&block.R, &dst_walk, SEMIBSIZE,
					       true);

			nbytes -= SEMIBSIZE;
		}

		 
		src = req->dst;
		dst = req->dst;
	}

	 
	if (block.A != cpu_to_be64(0xa6a6a6a6a6a6a6a6ULL))
		ret = -EBADMSG;

	memzero_explicit(&block, sizeof(struct crypto_kw_block));

	return ret;
}

static int crypto_kw_encrypt(struct skcipher_request *req)
{
	struct crypto_skcipher *tfm = crypto_skcipher_reqtfm(req);
	struct crypto_cipher *cipher = skcipher_cipher_simple(tfm);
	struct crypto_kw_block block;
	struct scatterlist *src, *dst;
	u64 t = 1;
	unsigned int i;

	 
	if (req->cryptlen < (2 * SEMIBSIZE) || req->cryptlen % SEMIBSIZE)
		return -EINVAL;

	 
	block.A = cpu_to_be64(0xa6a6a6a6a6a6a6a6ULL);

	 
	src = req->src;
	dst = req->dst;

	for (i = 0; i < 6; i++) {
		struct scatter_walk src_walk, dst_walk;
		unsigned int nbytes = req->cryptlen;

		scatterwalk_start(&src_walk, src);
		scatterwalk_start(&dst_walk, dst);

		while (nbytes) {
			 
			scatterwalk_copychunks(&block.R, &src_walk, SEMIBSIZE,
					       false);

			 
			crypto_cipher_encrypt_one(cipher, (u8 *)&block,
						  (u8 *)&block);
			 
			block.A ^= cpu_to_be64(t);
			t++;

			 
			scatterwalk_copychunks(&block.R, &dst_walk, SEMIBSIZE,
					       true);

			nbytes -= SEMIBSIZE;
		}

		 
		src = req->dst;
		dst = req->dst;
	}

	 
	memcpy(req->iv, &block.A, SEMIBSIZE);

	memzero_explicit(&block, sizeof(struct crypto_kw_block));

	return 0;
}

static int crypto_kw_create(struct crypto_template *tmpl, struct rtattr **tb)
{
	struct skcipher_instance *inst;
	struct crypto_alg *alg;
	int err;

	inst = skcipher_alloc_instance_simple(tmpl, tb);
	if (IS_ERR(inst))
		return PTR_ERR(inst);

	alg = skcipher_ialg_simple(inst);

	err = -EINVAL;
	 
	if (alg->cra_blocksize != sizeof(struct crypto_kw_block))
		goto out_free_inst;

	inst->alg.base.cra_blocksize = SEMIBSIZE;
	inst->alg.base.cra_alignmask = 0;
	inst->alg.ivsize = SEMIBSIZE;

	inst->alg.encrypt = crypto_kw_encrypt;
	inst->alg.decrypt = crypto_kw_decrypt;

	err = skcipher_register_instance(tmpl, inst);
	if (err) {
out_free_inst:
		inst->free(inst);
	}

	return err;
}

static struct crypto_template crypto_kw_tmpl = {
	.name = "kw",
	.create = crypto_kw_create,
	.module = THIS_MODULE,
};

static int __init crypto_kw_init(void)
{
	return crypto_register_template(&crypto_kw_tmpl);
}

static void __exit crypto_kw_exit(void)
{
	crypto_unregister_template(&crypto_kw_tmpl);
}

subsys_initcall(crypto_kw_init);
module_exit(crypto_kw_exit);

MODULE_LICENSE("Dual BSD/GPL");
MODULE_AUTHOR("Stephan Mueller <smueller@chronox.de>");
MODULE_DESCRIPTION("Key Wrapping (RFC3394 / NIST SP800-38F)");
MODULE_ALIAS_CRYPTO("kw");
MODULE_IMPORT_NS(CRYPTO_INTERNAL);
