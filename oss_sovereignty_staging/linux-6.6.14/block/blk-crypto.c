
 

 

#define pr_fmt(fmt) "blk-crypto: " fmt

#include <linux/bio.h>
#include <linux/blkdev.h>
#include <linux/blk-crypto-profile.h>
#include <linux/module.h>
#include <linux/ratelimit.h>
#include <linux/slab.h>

#include "blk-crypto-internal.h"

const struct blk_crypto_mode blk_crypto_modes[] = {
	[BLK_ENCRYPTION_MODE_AES_256_XTS] = {
		.name = "AES-256-XTS",
		.cipher_str = "xts(aes)",
		.keysize = 64,
		.ivsize = 16,
	},
	[BLK_ENCRYPTION_MODE_AES_128_CBC_ESSIV] = {
		.name = "AES-128-CBC-ESSIV",
		.cipher_str = "essiv(cbc(aes),sha256)",
		.keysize = 16,
		.ivsize = 16,
	},
	[BLK_ENCRYPTION_MODE_ADIANTUM] = {
		.name = "Adiantum",
		.cipher_str = "adiantum(xchacha12,aes)",
		.keysize = 32,
		.ivsize = 32,
	},
	[BLK_ENCRYPTION_MODE_SM4_XTS] = {
		.name = "SM4-XTS",
		.cipher_str = "xts(sm4)",
		.keysize = 32,
		.ivsize = 16,
	},
};

 
static int num_prealloc_crypt_ctxs = 128;

module_param(num_prealloc_crypt_ctxs, int, 0444);
MODULE_PARM_DESC(num_prealloc_crypt_ctxs,
		"Number of bio crypto contexts to preallocate");

static struct kmem_cache *bio_crypt_ctx_cache;
static mempool_t *bio_crypt_ctx_pool;

static int __init bio_crypt_ctx_init(void)
{
	size_t i;

	bio_crypt_ctx_cache = KMEM_CACHE(bio_crypt_ctx, 0);
	if (!bio_crypt_ctx_cache)
		goto out_no_mem;

	bio_crypt_ctx_pool = mempool_create_slab_pool(num_prealloc_crypt_ctxs,
						      bio_crypt_ctx_cache);
	if (!bio_crypt_ctx_pool)
		goto out_no_mem;

	 
	BUILD_BUG_ON(BLK_ENCRYPTION_MODE_INVALID != 0);

	 
	for (i = 0; i < BLK_ENCRYPTION_MODE_MAX; i++) {
		BUG_ON(blk_crypto_modes[i].keysize > BLK_CRYPTO_MAX_KEY_SIZE);
		BUG_ON(blk_crypto_modes[i].ivsize > BLK_CRYPTO_MAX_IV_SIZE);
	}

	return 0;
out_no_mem:
	panic("Failed to allocate mem for bio crypt ctxs\n");
}
subsys_initcall(bio_crypt_ctx_init);

void bio_crypt_set_ctx(struct bio *bio, const struct blk_crypto_key *key,
		       const u64 dun[BLK_CRYPTO_DUN_ARRAY_SIZE], gfp_t gfp_mask)
{
	struct bio_crypt_ctx *bc;

	 
	WARN_ON_ONCE(!(gfp_mask & __GFP_DIRECT_RECLAIM));

	bc = mempool_alloc(bio_crypt_ctx_pool, gfp_mask);

	bc->bc_key = key;
	memcpy(bc->bc_dun, dun, sizeof(bc->bc_dun));

	bio->bi_crypt_context = bc;
}

void __bio_crypt_free_ctx(struct bio *bio)
{
	mempool_free(bio->bi_crypt_context, bio_crypt_ctx_pool);
	bio->bi_crypt_context = NULL;
}

int __bio_crypt_clone(struct bio *dst, struct bio *src, gfp_t gfp_mask)
{
	dst->bi_crypt_context = mempool_alloc(bio_crypt_ctx_pool, gfp_mask);
	if (!dst->bi_crypt_context)
		return -ENOMEM;
	*dst->bi_crypt_context = *src->bi_crypt_context;
	return 0;
}

 
void bio_crypt_dun_increment(u64 dun[BLK_CRYPTO_DUN_ARRAY_SIZE],
			     unsigned int inc)
{
	int i;

	for (i = 0; inc && i < BLK_CRYPTO_DUN_ARRAY_SIZE; i++) {
		dun[i] += inc;
		 
		if (dun[i] < inc)
			inc = 1;
		else
			inc = 0;
	}
}

void __bio_crypt_advance(struct bio *bio, unsigned int bytes)
{
	struct bio_crypt_ctx *bc = bio->bi_crypt_context;

	bio_crypt_dun_increment(bc->bc_dun,
				bytes >> bc->bc_key->data_unit_size_bits);
}

 
bool bio_crypt_dun_is_contiguous(const struct bio_crypt_ctx *bc,
				 unsigned int bytes,
				 const u64 next_dun[BLK_CRYPTO_DUN_ARRAY_SIZE])
{
	int i;
	unsigned int carry = bytes >> bc->bc_key->data_unit_size_bits;

	for (i = 0; i < BLK_CRYPTO_DUN_ARRAY_SIZE; i++) {
		if (bc->bc_dun[i] + carry != next_dun[i])
			return false;
		 
		if ((bc->bc_dun[i] + carry) < carry)
			carry = 1;
		else
			carry = 0;
	}

	 
	return carry == 0;
}

 
static bool bio_crypt_ctx_compatible(struct bio_crypt_ctx *bc1,
				     struct bio_crypt_ctx *bc2)
{
	if (!bc1)
		return !bc2;

	return bc2 && bc1->bc_key == bc2->bc_key;
}

bool bio_crypt_rq_ctx_compatible(struct request *rq, struct bio *bio)
{
	return bio_crypt_ctx_compatible(rq->crypt_ctx, bio->bi_crypt_context);
}

 
bool bio_crypt_ctx_mergeable(struct bio_crypt_ctx *bc1, unsigned int bc1_bytes,
			     struct bio_crypt_ctx *bc2)
{
	if (!bio_crypt_ctx_compatible(bc1, bc2))
		return false;

	return !bc1 || bio_crypt_dun_is_contiguous(bc1, bc1_bytes, bc2->bc_dun);
}

 
static bool bio_crypt_check_alignment(struct bio *bio)
{
	const unsigned int data_unit_size =
		bio->bi_crypt_context->bc_key->crypto_cfg.data_unit_size;
	struct bvec_iter iter;
	struct bio_vec bv;

	bio_for_each_segment(bv, bio, iter) {
		if (!IS_ALIGNED(bv.bv_len | bv.bv_offset, data_unit_size))
			return false;
	}

	return true;
}

blk_status_t __blk_crypto_rq_get_keyslot(struct request *rq)
{
	return blk_crypto_get_keyslot(rq->q->crypto_profile,
				      rq->crypt_ctx->bc_key,
				      &rq->crypt_keyslot);
}

void __blk_crypto_rq_put_keyslot(struct request *rq)
{
	blk_crypto_put_keyslot(rq->crypt_keyslot);
	rq->crypt_keyslot = NULL;
}

void __blk_crypto_free_request(struct request *rq)
{
	 
	if (WARN_ON_ONCE(rq->crypt_keyslot))
		__blk_crypto_rq_put_keyslot(rq);

	mempool_free(rq->crypt_ctx, bio_crypt_ctx_pool);
	rq->crypt_ctx = NULL;
}

 
bool __blk_crypto_bio_prep(struct bio **bio_ptr)
{
	struct bio *bio = *bio_ptr;
	const struct blk_crypto_key *bc_key = bio->bi_crypt_context->bc_key;

	 
	if (WARN_ON_ONCE(!bio_has_data(bio))) {
		bio->bi_status = BLK_STS_IOERR;
		goto fail;
	}

	if (!bio_crypt_check_alignment(bio)) {
		bio->bi_status = BLK_STS_IOERR;
		goto fail;
	}

	 
	if (blk_crypto_config_supported_natively(bio->bi_bdev,
						 &bc_key->crypto_cfg))
		return true;
	if (blk_crypto_fallback_bio_prep(bio_ptr))
		return true;
fail:
	bio_endio(*bio_ptr);
	return false;
}

int __blk_crypto_rq_bio_prep(struct request *rq, struct bio *bio,
			     gfp_t gfp_mask)
{
	if (!rq->crypt_ctx) {
		rq->crypt_ctx = mempool_alloc(bio_crypt_ctx_pool, gfp_mask);
		if (!rq->crypt_ctx)
			return -ENOMEM;
	}
	*rq->crypt_ctx = *bio->bi_crypt_context;
	return 0;
}

 
int blk_crypto_init_key(struct blk_crypto_key *blk_key, const u8 *raw_key,
			enum blk_crypto_mode_num crypto_mode,
			unsigned int dun_bytes,
			unsigned int data_unit_size)
{
	const struct blk_crypto_mode *mode;

	memset(blk_key, 0, sizeof(*blk_key));

	if (crypto_mode >= ARRAY_SIZE(blk_crypto_modes))
		return -EINVAL;

	mode = &blk_crypto_modes[crypto_mode];
	if (mode->keysize == 0)
		return -EINVAL;

	if (dun_bytes == 0 || dun_bytes > mode->ivsize)
		return -EINVAL;

	if (!is_power_of_2(data_unit_size))
		return -EINVAL;

	blk_key->crypto_cfg.crypto_mode = crypto_mode;
	blk_key->crypto_cfg.dun_bytes = dun_bytes;
	blk_key->crypto_cfg.data_unit_size = data_unit_size;
	blk_key->data_unit_size_bits = ilog2(data_unit_size);
	blk_key->size = mode->keysize;
	memcpy(blk_key->raw, raw_key, mode->keysize);

	return 0;
}

bool blk_crypto_config_supported_natively(struct block_device *bdev,
					  const struct blk_crypto_config *cfg)
{
	return __blk_crypto_cfg_supported(bdev_get_queue(bdev)->crypto_profile,
					  cfg);
}

 
bool blk_crypto_config_supported(struct block_device *bdev,
				 const struct blk_crypto_config *cfg)
{
	return IS_ENABLED(CONFIG_BLK_INLINE_ENCRYPTION_FALLBACK) ||
	       blk_crypto_config_supported_natively(bdev, cfg);
}

 
int blk_crypto_start_using_key(struct block_device *bdev,
			       const struct blk_crypto_key *key)
{
	if (blk_crypto_config_supported_natively(bdev, &key->crypto_cfg))
		return 0;
	return blk_crypto_fallback_start_using_mode(key->crypto_cfg.crypto_mode);
}

 
void blk_crypto_evict_key(struct block_device *bdev,
			  const struct blk_crypto_key *key)
{
	struct request_queue *q = bdev_get_queue(bdev);
	int err;

	if (blk_crypto_config_supported_natively(bdev, &key->crypto_cfg))
		err = __blk_crypto_evict_key(q->crypto_profile, key);
	else
		err = blk_crypto_fallback_evict_key(key);
	 
	if (err)
		pr_warn_ratelimited("%pg: error %d evicting key\n", bdev, err);
}
EXPORT_SYMBOL_GPL(blk_crypto_evict_key);
