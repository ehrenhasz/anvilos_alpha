 

#include <crypto/hash.h>
#include <crypto/sha3.h>
#include <linux/fips.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/time.h>
#include <crypto/internal/rng.h>

#include "jitterentropy.h"

#define JENT_CONDITIONING_HASH	"sha3-256-generic"

 

void *jent_zalloc(unsigned int len)
{
	return kzalloc(len, GFP_KERNEL);
}

void jent_zfree(void *ptr)
{
	kfree_sensitive(ptr);
}

 
void jent_get_nstime(__u64 *out)
{
	__u64 tmp = 0;

	tmp = random_get_entropy();

	 
	if (tmp == 0)
		tmp = ktime_get_ns();

	*out = tmp;
	jent_raw_hires_entropy_store(tmp);
}

int jent_hash_time(void *hash_state, __u64 time, u8 *addtl,
		   unsigned int addtl_len, __u64 hash_loop_cnt,
		   unsigned int stuck)
{
	struct shash_desc *hash_state_desc = (struct shash_desc *)hash_state;
	SHASH_DESC_ON_STACK(desc, hash_state_desc->tfm);
	u8 intermediary[SHA3_256_DIGEST_SIZE];
	__u64 j = 0;
	int ret;

	desc->tfm = hash_state_desc->tfm;

	if (sizeof(intermediary) != crypto_shash_digestsize(desc->tfm)) {
		pr_warn_ratelimited("Unexpected digest size\n");
		return -EINVAL;
	}

	 
	for (j = 0; j < hash_loop_cnt; j++) {
		ret = crypto_shash_init(desc) ?:
		      crypto_shash_update(desc, intermediary,
					  sizeof(intermediary)) ?:
		      crypto_shash_finup(desc, addtl, addtl_len, intermediary);
		if (ret)
			goto err;
	}

	 
	ret = crypto_shash_update(desc, intermediary, sizeof(intermediary));
	if (ret)
		goto err;

	 
	if (!stuck) {
		ret = crypto_shash_update(hash_state_desc, (u8 *)&time,
					  sizeof(__u64));
	}

err:
	shash_desc_zero(desc);
	memzero_explicit(intermediary, sizeof(intermediary));

	return ret;
}

int jent_read_random_block(void *hash_state, char *dst, unsigned int dst_len)
{
	struct shash_desc *hash_state_desc = (struct shash_desc *)hash_state;
	u8 jent_block[SHA3_256_DIGEST_SIZE];
	 
	int ret = crypto_shash_final(hash_state_desc, jent_block) ?:
		  crypto_shash_init(hash_state_desc) ?:
		  crypto_shash_update(hash_state_desc, jent_block,
				      sizeof(jent_block));

	if (!ret && dst_len)
		memcpy(dst, jent_block, dst_len);

	memzero_explicit(jent_block, sizeof(jent_block));
	return ret;
}

 

struct jitterentropy {
	spinlock_t jent_lock;
	struct rand_data *entropy_collector;
	struct crypto_shash *tfm;
	struct shash_desc *sdesc;
};

static void jent_kcapi_cleanup(struct crypto_tfm *tfm)
{
	struct jitterentropy *rng = crypto_tfm_ctx(tfm);

	spin_lock(&rng->jent_lock);

	if (rng->sdesc) {
		shash_desc_zero(rng->sdesc);
		kfree(rng->sdesc);
	}
	rng->sdesc = NULL;

	if (rng->tfm)
		crypto_free_shash(rng->tfm);
	rng->tfm = NULL;

	if (rng->entropy_collector)
		jent_entropy_collector_free(rng->entropy_collector);
	rng->entropy_collector = NULL;
	spin_unlock(&rng->jent_lock);
}

static int jent_kcapi_init(struct crypto_tfm *tfm)
{
	struct jitterentropy *rng = crypto_tfm_ctx(tfm);
	struct crypto_shash *hash;
	struct shash_desc *sdesc;
	int size, ret = 0;

	spin_lock_init(&rng->jent_lock);

	 
	hash = crypto_alloc_shash(JENT_CONDITIONING_HASH, 0, 0);
	if (IS_ERR(hash)) {
		pr_err("Cannot allocate conditioning digest\n");
		return PTR_ERR(hash);
	}
	rng->tfm = hash;

	size = sizeof(struct shash_desc) + crypto_shash_descsize(hash);
	sdesc = kmalloc(size, GFP_KERNEL);
	if (!sdesc) {
		ret = -ENOMEM;
		goto err;
	}

	sdesc->tfm = hash;
	crypto_shash_init(sdesc);
	rng->sdesc = sdesc;

	rng->entropy_collector = jent_entropy_collector_alloc(1, 0, sdesc);
	if (!rng->entropy_collector) {
		ret = -ENOMEM;
		goto err;
	}

	spin_lock_init(&rng->jent_lock);
	return 0;

err:
	jent_kcapi_cleanup(tfm);
	return ret;
}

static int jent_kcapi_random(struct crypto_rng *tfm,
			     const u8 *src, unsigned int slen,
			     u8 *rdata, unsigned int dlen)
{
	struct jitterentropy *rng = crypto_rng_ctx(tfm);
	int ret = 0;

	spin_lock(&rng->jent_lock);

	ret = jent_read_entropy(rng->entropy_collector, rdata, dlen);

	if (ret == -3) {
		 
		 
		if (fips_enabled)
			panic("Jitter RNG permanent health test failure\n");

		pr_err("Jitter RNG permanent health test failure\n");
		ret = -EFAULT;
	} else if (ret == -2) {
		 
		pr_warn_ratelimited("Reset Jitter RNG due to intermittent health test failure\n");
		ret = -EAGAIN;
	} else if (ret == -1) {
		 
		ret = -EINVAL;
	}

	spin_unlock(&rng->jent_lock);

	return ret;
}

static int jent_kcapi_reset(struct crypto_rng *tfm,
			    const u8 *seed, unsigned int slen)
{
	return 0;
}

static struct rng_alg jent_alg = {
	.generate		= jent_kcapi_random,
	.seed			= jent_kcapi_reset,
	.seedsize		= 0,
	.base			= {
		.cra_name               = "jitterentropy_rng",
		.cra_driver_name        = "jitterentropy_rng",
		.cra_priority           = 100,
		.cra_ctxsize            = sizeof(struct jitterentropy),
		.cra_module             = THIS_MODULE,
		.cra_init               = jent_kcapi_init,
		.cra_exit               = jent_kcapi_cleanup,
	}
};

static int __init jent_mod_init(void)
{
	SHASH_DESC_ON_STACK(desc, tfm);
	struct crypto_shash *tfm;
	int ret = 0;

	jent_testing_init();

	tfm = crypto_alloc_shash(JENT_CONDITIONING_HASH, 0, 0);
	if (IS_ERR(tfm)) {
		jent_testing_exit();
		return PTR_ERR(tfm);
	}

	desc->tfm = tfm;
	crypto_shash_init(desc);
	ret = jent_entropy_init(desc);
	shash_desc_zero(desc);
	crypto_free_shash(tfm);
	if (ret) {
		 
		if (fips_enabled)
			panic("jitterentropy: Initialization failed with host not compliant with requirements: %d\n", ret);

		jent_testing_exit();
		pr_info("jitterentropy: Initialization failed with host not compliant with requirements: %d\n", ret);
		return -EFAULT;
	}
	return crypto_register_rng(&jent_alg);
}

static void __exit jent_mod_exit(void)
{
	jent_testing_exit();
	crypto_unregister_rng(&jent_alg);
}

module_init(jent_mod_init);
module_exit(jent_mod_exit);

MODULE_LICENSE("Dual BSD/GPL");
MODULE_AUTHOR("Stephan Mueller <smueller@chronox.de>");
MODULE_DESCRIPTION("Non-physical True Random Number Generator based on CPU Jitter");
MODULE_ALIAS_CRYPTO("jitterentropy_rng");
