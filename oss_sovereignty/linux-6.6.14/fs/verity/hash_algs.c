
 

#include "fsverity_private.h"

#include <crypto/hash.h>

 
struct fsverity_hash_alg fsverity_hash_algs[] = {
	[FS_VERITY_HASH_ALG_SHA256] = {
		.name = "sha256",
		.digest_size = SHA256_DIGEST_SIZE,
		.block_size = SHA256_BLOCK_SIZE,
		.algo_id = HASH_ALGO_SHA256,
	},
	[FS_VERITY_HASH_ALG_SHA512] = {
		.name = "sha512",
		.digest_size = SHA512_DIGEST_SIZE,
		.block_size = SHA512_BLOCK_SIZE,
		.algo_id = HASH_ALGO_SHA512,
	},
};

static DEFINE_MUTEX(fsverity_hash_alg_init_mutex);

 
const struct fsverity_hash_alg *fsverity_get_hash_alg(const struct inode *inode,
						      unsigned int num)
{
	struct fsverity_hash_alg *alg;
	struct crypto_shash *tfm;
	int err;

	if (num >= ARRAY_SIZE(fsverity_hash_algs) ||
	    !fsverity_hash_algs[num].name) {
		fsverity_warn(inode, "Unknown hash algorithm number: %u", num);
		return ERR_PTR(-EINVAL);
	}
	alg = &fsverity_hash_algs[num];

	 
	if (likely(smp_load_acquire(&alg->tfm) != NULL))
		return alg;

	mutex_lock(&fsverity_hash_alg_init_mutex);

	if (alg->tfm != NULL)
		goto out_unlock;

	tfm = crypto_alloc_shash(alg->name, 0, 0);
	if (IS_ERR(tfm)) {
		if (PTR_ERR(tfm) == -ENOENT) {
			fsverity_warn(inode,
				      "Missing crypto API support for hash algorithm \"%s\"",
				      alg->name);
			alg = ERR_PTR(-ENOPKG);
			goto out_unlock;
		}
		fsverity_err(inode,
			     "Error allocating hash algorithm \"%s\": %ld",
			     alg->name, PTR_ERR(tfm));
		alg = ERR_CAST(tfm);
		goto out_unlock;
	}

	err = -EINVAL;
	if (WARN_ON_ONCE(alg->digest_size != crypto_shash_digestsize(tfm)))
		goto err_free_tfm;
	if (WARN_ON_ONCE(alg->block_size != crypto_shash_blocksize(tfm)))
		goto err_free_tfm;

	pr_info("%s using implementation \"%s\"\n",
		alg->name, crypto_shash_driver_name(tfm));

	 
	smp_store_release(&alg->tfm, tfm);
	goto out_unlock;

err_free_tfm:
	crypto_free_shash(tfm);
	alg = ERR_PTR(err);
out_unlock:
	mutex_unlock(&fsverity_hash_alg_init_mutex);
	return alg;
}

 
const u8 *fsverity_prepare_hash_state(const struct fsverity_hash_alg *alg,
				      const u8 *salt, size_t salt_size)
{
	u8 *hashstate = NULL;
	SHASH_DESC_ON_STACK(desc, alg->tfm);
	u8 *padded_salt = NULL;
	size_t padded_salt_size;
	int err;

	desc->tfm = alg->tfm;

	if (salt_size == 0)
		return NULL;

	hashstate = kmalloc(crypto_shash_statesize(alg->tfm), GFP_KERNEL);
	if (!hashstate)
		return ERR_PTR(-ENOMEM);

	 
	padded_salt_size = round_up(salt_size, alg->block_size);
	padded_salt = kzalloc(padded_salt_size, GFP_KERNEL);
	if (!padded_salt) {
		err = -ENOMEM;
		goto err_free;
	}
	memcpy(padded_salt, salt, salt_size);
	err = crypto_shash_init(desc);
	if (err)
		goto err_free;

	err = crypto_shash_update(desc, padded_salt, padded_salt_size);
	if (err)
		goto err_free;

	err = crypto_shash_export(desc, hashstate);
	if (err)
		goto err_free;
out:
	kfree(padded_salt);
	return hashstate;

err_free:
	kfree(hashstate);
	hashstate = ERR_PTR(err);
	goto out;
}

 
int fsverity_hash_block(const struct merkle_tree_params *params,
			const struct inode *inode, const void *data, u8 *out)
{
	SHASH_DESC_ON_STACK(desc, params->hash_alg->tfm);
	int err;

	desc->tfm = params->hash_alg->tfm;

	if (params->hashstate) {
		err = crypto_shash_import(desc, params->hashstate);
		if (err) {
			fsverity_err(inode,
				     "Error %d importing hash state", err);
			return err;
		}
		err = crypto_shash_finup(desc, data, params->block_size, out);
	} else {
		err = crypto_shash_digest(desc, data, params->block_size, out);
	}
	if (err)
		fsverity_err(inode, "Error %d computing block hash", err);
	return err;
}

 
int fsverity_hash_buffer(const struct fsverity_hash_alg *alg,
			 const void *data, size_t size, u8 *out)
{
	return crypto_shash_tfm_digest(alg->tfm, data, size, out);
}

void __init fsverity_check_hash_algs(void)
{
	size_t i;

	 
	for (i = 0; i < ARRAY_SIZE(fsverity_hash_algs); i++) {
		const struct fsverity_hash_alg *alg = &fsverity_hash_algs[i];

		if (!alg->name)
			continue;

		 
		BUG_ON(i == 0);

		BUG_ON(alg->digest_size > FS_VERITY_MAX_DIGEST_SIZE);

		 
		BUG_ON(!is_power_of_2(alg->digest_size));
		BUG_ON(!is_power_of_2(alg->block_size));

		 
		BUG_ON(alg->algo_id == 0);
		BUG_ON(alg->digest_size != hash_digest_size[alg->algo_id]);
	}
}
