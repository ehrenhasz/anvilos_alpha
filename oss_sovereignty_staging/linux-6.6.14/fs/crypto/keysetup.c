
 

#include <crypto/skcipher.h>
#include <linux/random.h>

#include "fscrypt_private.h"

struct fscrypt_mode fscrypt_modes[] = {
	[FSCRYPT_MODE_AES_256_XTS] = {
		.friendly_name = "AES-256-XTS",
		.cipher_str = "xts(aes)",
		.keysize = 64,
		.security_strength = 32,
		.ivsize = 16,
		.blk_crypto_mode = BLK_ENCRYPTION_MODE_AES_256_XTS,
	},
	[FSCRYPT_MODE_AES_256_CTS] = {
		.friendly_name = "AES-256-CTS-CBC",
		.cipher_str = "cts(cbc(aes))",
		.keysize = 32,
		.security_strength = 32,
		.ivsize = 16,
	},
	[FSCRYPT_MODE_AES_128_CBC] = {
		.friendly_name = "AES-128-CBC-ESSIV",
		.cipher_str = "essiv(cbc(aes),sha256)",
		.keysize = 16,
		.security_strength = 16,
		.ivsize = 16,
		.blk_crypto_mode = BLK_ENCRYPTION_MODE_AES_128_CBC_ESSIV,
	},
	[FSCRYPT_MODE_AES_128_CTS] = {
		.friendly_name = "AES-128-CTS-CBC",
		.cipher_str = "cts(cbc(aes))",
		.keysize = 16,
		.security_strength = 16,
		.ivsize = 16,
	},
	[FSCRYPT_MODE_SM4_XTS] = {
		.friendly_name = "SM4-XTS",
		.cipher_str = "xts(sm4)",
		.keysize = 32,
		.security_strength = 16,
		.ivsize = 16,
		.blk_crypto_mode = BLK_ENCRYPTION_MODE_SM4_XTS,
	},
	[FSCRYPT_MODE_SM4_CTS] = {
		.friendly_name = "SM4-CTS-CBC",
		.cipher_str = "cts(cbc(sm4))",
		.keysize = 16,
		.security_strength = 16,
		.ivsize = 16,
	},
	[FSCRYPT_MODE_ADIANTUM] = {
		.friendly_name = "Adiantum",
		.cipher_str = "adiantum(xchacha12,aes)",
		.keysize = 32,
		.security_strength = 32,
		.ivsize = 32,
		.blk_crypto_mode = BLK_ENCRYPTION_MODE_ADIANTUM,
	},
	[FSCRYPT_MODE_AES_256_HCTR2] = {
		.friendly_name = "AES-256-HCTR2",
		.cipher_str = "hctr2(aes)",
		.keysize = 32,
		.security_strength = 32,
		.ivsize = 32,
	},
};

static DEFINE_MUTEX(fscrypt_mode_key_setup_mutex);

static struct fscrypt_mode *
select_encryption_mode(const union fscrypt_policy *policy,
		       const struct inode *inode)
{
	BUILD_BUG_ON(ARRAY_SIZE(fscrypt_modes) != FSCRYPT_MODE_MAX + 1);

	if (S_ISREG(inode->i_mode))
		return &fscrypt_modes[fscrypt_policy_contents_mode(policy)];

	if (S_ISDIR(inode->i_mode) || S_ISLNK(inode->i_mode))
		return &fscrypt_modes[fscrypt_policy_fnames_mode(policy)];

	WARN_ONCE(1, "fscrypt: filesystem tried to load encryption info for inode %lu, which is not encryptable (file type %d)\n",
		  inode->i_ino, (inode->i_mode & S_IFMT));
	return ERR_PTR(-EINVAL);
}

 
static struct crypto_skcipher *
fscrypt_allocate_skcipher(struct fscrypt_mode *mode, const u8 *raw_key,
			  const struct inode *inode)
{
	struct crypto_skcipher *tfm;
	int err;

	tfm = crypto_alloc_skcipher(mode->cipher_str, 0, 0);
	if (IS_ERR(tfm)) {
		if (PTR_ERR(tfm) == -ENOENT) {
			fscrypt_warn(inode,
				     "Missing crypto API support for %s (API name: \"%s\")",
				     mode->friendly_name, mode->cipher_str);
			return ERR_PTR(-ENOPKG);
		}
		fscrypt_err(inode, "Error allocating '%s' transform: %ld",
			    mode->cipher_str, PTR_ERR(tfm));
		return tfm;
	}
	if (!xchg(&mode->logged_cryptoapi_impl, 1)) {
		 
		pr_info("fscrypt: %s using implementation \"%s\"\n",
			mode->friendly_name, crypto_skcipher_driver_name(tfm));
	}
	if (WARN_ON_ONCE(crypto_skcipher_ivsize(tfm) != mode->ivsize)) {
		err = -EINVAL;
		goto err_free_tfm;
	}
	crypto_skcipher_set_flags(tfm, CRYPTO_TFM_REQ_FORBID_WEAK_KEYS);
	err = crypto_skcipher_setkey(tfm, raw_key, mode->keysize);
	if (err)
		goto err_free_tfm;

	return tfm;

err_free_tfm:
	crypto_free_skcipher(tfm);
	return ERR_PTR(err);
}

 
int fscrypt_prepare_key(struct fscrypt_prepared_key *prep_key,
			const u8 *raw_key, const struct fscrypt_info *ci)
{
	struct crypto_skcipher *tfm;

	if (fscrypt_using_inline_encryption(ci))
		return fscrypt_prepare_inline_crypt_key(prep_key, raw_key, ci);

	tfm = fscrypt_allocate_skcipher(ci->ci_mode, raw_key, ci->ci_inode);
	if (IS_ERR(tfm))
		return PTR_ERR(tfm);
	 
	smp_store_release(&prep_key->tfm, tfm);
	return 0;
}

 
void fscrypt_destroy_prepared_key(struct super_block *sb,
				  struct fscrypt_prepared_key *prep_key)
{
	crypto_free_skcipher(prep_key->tfm);
	fscrypt_destroy_inline_crypt_key(sb, prep_key);
	memzero_explicit(prep_key, sizeof(*prep_key));
}

 
int fscrypt_set_per_file_enc_key(struct fscrypt_info *ci, const u8 *raw_key)
{
	ci->ci_owns_key = true;
	return fscrypt_prepare_key(&ci->ci_enc_key, raw_key, ci);
}

static int setup_per_mode_enc_key(struct fscrypt_info *ci,
				  struct fscrypt_master_key *mk,
				  struct fscrypt_prepared_key *keys,
				  u8 hkdf_context, bool include_fs_uuid)
{
	const struct inode *inode = ci->ci_inode;
	const struct super_block *sb = inode->i_sb;
	struct fscrypt_mode *mode = ci->ci_mode;
	const u8 mode_num = mode - fscrypt_modes;
	struct fscrypt_prepared_key *prep_key;
	u8 mode_key[FSCRYPT_MAX_KEY_SIZE];
	u8 hkdf_info[sizeof(mode_num) + sizeof(sb->s_uuid)];
	unsigned int hkdf_infolen = 0;
	int err;

	if (WARN_ON_ONCE(mode_num > FSCRYPT_MODE_MAX))
		return -EINVAL;

	prep_key = &keys[mode_num];
	if (fscrypt_is_key_prepared(prep_key, ci)) {
		ci->ci_enc_key = *prep_key;
		return 0;
	}

	mutex_lock(&fscrypt_mode_key_setup_mutex);

	if (fscrypt_is_key_prepared(prep_key, ci))
		goto done_unlock;

	BUILD_BUG_ON(sizeof(mode_num) != 1);
	BUILD_BUG_ON(sizeof(sb->s_uuid) != 16);
	BUILD_BUG_ON(sizeof(hkdf_info) != 17);
	hkdf_info[hkdf_infolen++] = mode_num;
	if (include_fs_uuid) {
		memcpy(&hkdf_info[hkdf_infolen], &sb->s_uuid,
		       sizeof(sb->s_uuid));
		hkdf_infolen += sizeof(sb->s_uuid);
	}
	err = fscrypt_hkdf_expand(&mk->mk_secret.hkdf,
				  hkdf_context, hkdf_info, hkdf_infolen,
				  mode_key, mode->keysize);
	if (err)
		goto out_unlock;
	err = fscrypt_prepare_key(prep_key, mode_key, ci);
	memzero_explicit(mode_key, mode->keysize);
	if (err)
		goto out_unlock;
done_unlock:
	ci->ci_enc_key = *prep_key;
	err = 0;
out_unlock:
	mutex_unlock(&fscrypt_mode_key_setup_mutex);
	return err;
}

 
static int fscrypt_derive_siphash_key(const struct fscrypt_master_key *mk,
				      u8 context, const u8 *info,
				      unsigned int infolen, siphash_key_t *key)
{
	int err;

	err = fscrypt_hkdf_expand(&mk->mk_secret.hkdf, context, info, infolen,
				  (u8 *)key, sizeof(*key));
	if (err)
		return err;

	BUILD_BUG_ON(sizeof(*key) != 16);
	BUILD_BUG_ON(ARRAY_SIZE(key->key) != 2);
	le64_to_cpus(&key->key[0]);
	le64_to_cpus(&key->key[1]);
	return 0;
}

int fscrypt_derive_dirhash_key(struct fscrypt_info *ci,
			       const struct fscrypt_master_key *mk)
{
	int err;

	err = fscrypt_derive_siphash_key(mk, HKDF_CONTEXT_DIRHASH_KEY,
					 ci->ci_nonce, FSCRYPT_FILE_NONCE_SIZE,
					 &ci->ci_dirhash_key);
	if (err)
		return err;
	ci->ci_dirhash_key_initialized = true;
	return 0;
}

void fscrypt_hash_inode_number(struct fscrypt_info *ci,
			       const struct fscrypt_master_key *mk)
{
	WARN_ON_ONCE(ci->ci_inode->i_ino == 0);
	WARN_ON_ONCE(!mk->mk_ino_hash_key_initialized);

	ci->ci_hashed_ino = (u32)siphash_1u64(ci->ci_inode->i_ino,
					      &mk->mk_ino_hash_key);
}

static int fscrypt_setup_iv_ino_lblk_32_key(struct fscrypt_info *ci,
					    struct fscrypt_master_key *mk)
{
	int err;

	err = setup_per_mode_enc_key(ci, mk, mk->mk_iv_ino_lblk_32_keys,
				     HKDF_CONTEXT_IV_INO_LBLK_32_KEY, true);
	if (err)
		return err;

	 
	if (!smp_load_acquire(&mk->mk_ino_hash_key_initialized)) {

		mutex_lock(&fscrypt_mode_key_setup_mutex);

		if (mk->mk_ino_hash_key_initialized)
			goto unlock;

		err = fscrypt_derive_siphash_key(mk,
						 HKDF_CONTEXT_INODE_HASH_KEY,
						 NULL, 0, &mk->mk_ino_hash_key);
		if (err)
			goto unlock;
		 
		smp_store_release(&mk->mk_ino_hash_key_initialized, true);
unlock:
		mutex_unlock(&fscrypt_mode_key_setup_mutex);
		if (err)
			return err;
	}

	 
	if (ci->ci_inode->i_ino)
		fscrypt_hash_inode_number(ci, mk);
	return 0;
}

static int fscrypt_setup_v2_file_key(struct fscrypt_info *ci,
				     struct fscrypt_master_key *mk,
				     bool need_dirhash_key)
{
	int err;

	if (ci->ci_policy.v2.flags & FSCRYPT_POLICY_FLAG_DIRECT_KEY) {
		 
		err = setup_per_mode_enc_key(ci, mk, mk->mk_direct_keys,
					     HKDF_CONTEXT_DIRECT_KEY, false);
	} else if (ci->ci_policy.v2.flags &
		   FSCRYPT_POLICY_FLAG_IV_INO_LBLK_64) {
		 
		err = setup_per_mode_enc_key(ci, mk, mk->mk_iv_ino_lblk_64_keys,
					     HKDF_CONTEXT_IV_INO_LBLK_64_KEY,
					     true);
	} else if (ci->ci_policy.v2.flags &
		   FSCRYPT_POLICY_FLAG_IV_INO_LBLK_32) {
		err = fscrypt_setup_iv_ino_lblk_32_key(ci, mk);
	} else {
		u8 derived_key[FSCRYPT_MAX_KEY_SIZE];

		err = fscrypt_hkdf_expand(&mk->mk_secret.hkdf,
					  HKDF_CONTEXT_PER_FILE_ENC_KEY,
					  ci->ci_nonce, FSCRYPT_FILE_NONCE_SIZE,
					  derived_key, ci->ci_mode->keysize);
		if (err)
			return err;

		err = fscrypt_set_per_file_enc_key(ci, derived_key);
		memzero_explicit(derived_key, ci->ci_mode->keysize);
	}
	if (err)
		return err;

	 
	if (need_dirhash_key) {
		err = fscrypt_derive_dirhash_key(ci, mk);
		if (err)
			return err;
	}

	return 0;
}

 
static bool fscrypt_valid_master_key_size(const struct fscrypt_master_key *mk,
					  const struct fscrypt_info *ci)
{
	unsigned int min_keysize;

	if (ci->ci_policy.version == FSCRYPT_POLICY_V1)
		min_keysize = ci->ci_mode->keysize;
	else
		min_keysize = ci->ci_mode->security_strength;

	if (mk->mk_secret.size < min_keysize) {
		fscrypt_warn(NULL,
			     "key with %s %*phN is too short (got %u bytes, need %u+ bytes)",
			     master_key_spec_type(&mk->mk_spec),
			     master_key_spec_len(&mk->mk_spec),
			     (u8 *)&mk->mk_spec.u,
			     mk->mk_secret.size, min_keysize);
		return false;
	}
	return true;
}

 
static int setup_file_encryption_key(struct fscrypt_info *ci,
				     bool need_dirhash_key,
				     struct fscrypt_master_key **mk_ret)
{
	struct super_block *sb = ci->ci_inode->i_sb;
	struct fscrypt_key_specifier mk_spec;
	struct fscrypt_master_key *mk;
	int err;

	err = fscrypt_select_encryption_impl(ci);
	if (err)
		return err;

	err = fscrypt_policy_to_key_spec(&ci->ci_policy, &mk_spec);
	if (err)
		return err;

	mk = fscrypt_find_master_key(sb, &mk_spec);
	if (unlikely(!mk)) {
		const union fscrypt_policy *dummy_policy =
			fscrypt_get_dummy_policy(sb);

		 
		if (dummy_policy &&
		    fscrypt_policies_equal(dummy_policy, &ci->ci_policy)) {
			err = fscrypt_add_test_dummy_key(sb, &mk_spec);
			if (err)
				return err;
			mk = fscrypt_find_master_key(sb, &mk_spec);
		}
	}
	if (unlikely(!mk)) {
		if (ci->ci_policy.version != FSCRYPT_POLICY_V1)
			return -ENOKEY;

		 
		return fscrypt_setup_v1_file_key_via_subscribed_keyrings(ci);
	}
	down_read(&mk->mk_sem);

	 
	if (!is_master_key_secret_present(&mk->mk_secret)) {
		err = -ENOKEY;
		goto out_release_key;
	}

	if (!fscrypt_valid_master_key_size(mk, ci)) {
		err = -ENOKEY;
		goto out_release_key;
	}

	switch (ci->ci_policy.version) {
	case FSCRYPT_POLICY_V1:
		err = fscrypt_setup_v1_file_key(ci, mk->mk_secret.raw);
		break;
	case FSCRYPT_POLICY_V2:
		err = fscrypt_setup_v2_file_key(ci, mk, need_dirhash_key);
		break;
	default:
		WARN_ON_ONCE(1);
		err = -EINVAL;
		break;
	}
	if (err)
		goto out_release_key;

	*mk_ret = mk;
	return 0;

out_release_key:
	up_read(&mk->mk_sem);
	fscrypt_put_master_key(mk);
	return err;
}

static void put_crypt_info(struct fscrypt_info *ci)
{
	struct fscrypt_master_key *mk;

	if (!ci)
		return;

	if (ci->ci_direct_key)
		fscrypt_put_direct_key(ci->ci_direct_key);
	else if (ci->ci_owns_key)
		fscrypt_destroy_prepared_key(ci->ci_inode->i_sb,
					     &ci->ci_enc_key);

	mk = ci->ci_master_key;
	if (mk) {
		 
		spin_lock(&mk->mk_decrypted_inodes_lock);
		list_del(&ci->ci_master_key_link);
		spin_unlock(&mk->mk_decrypted_inodes_lock);
		fscrypt_put_master_key_activeref(ci->ci_inode->i_sb, mk);
	}
	memzero_explicit(ci, sizeof(*ci));
	kmem_cache_free(fscrypt_info_cachep, ci);
}

static int
fscrypt_setup_encryption_info(struct inode *inode,
			      const union fscrypt_policy *policy,
			      const u8 nonce[FSCRYPT_FILE_NONCE_SIZE],
			      bool need_dirhash_key)
{
	struct fscrypt_info *crypt_info;
	struct fscrypt_mode *mode;
	struct fscrypt_master_key *mk = NULL;
	int res;

	res = fscrypt_initialize(inode->i_sb);
	if (res)
		return res;

	crypt_info = kmem_cache_zalloc(fscrypt_info_cachep, GFP_KERNEL);
	if (!crypt_info)
		return -ENOMEM;

	crypt_info->ci_inode = inode;
	crypt_info->ci_policy = *policy;
	memcpy(crypt_info->ci_nonce, nonce, FSCRYPT_FILE_NONCE_SIZE);

	mode = select_encryption_mode(&crypt_info->ci_policy, inode);
	if (IS_ERR(mode)) {
		res = PTR_ERR(mode);
		goto out;
	}
	WARN_ON_ONCE(mode->ivsize > FSCRYPT_MAX_IV_SIZE);
	crypt_info->ci_mode = mode;

	res = setup_file_encryption_key(crypt_info, need_dirhash_key, &mk);
	if (res)
		goto out;

	 
	if (cmpxchg_release(&inode->i_crypt_info, NULL, crypt_info) == NULL) {
		 
		if (mk) {
			crypt_info->ci_master_key = mk;
			refcount_inc(&mk->mk_active_refs);
			spin_lock(&mk->mk_decrypted_inodes_lock);
			list_add(&crypt_info->ci_master_key_link,
				 &mk->mk_decrypted_inodes);
			spin_unlock(&mk->mk_decrypted_inodes_lock);
		}
		crypt_info = NULL;
	}
	res = 0;
out:
	if (mk) {
		up_read(&mk->mk_sem);
		fscrypt_put_master_key(mk);
	}
	put_crypt_info(crypt_info);
	return res;
}

 
int fscrypt_get_encryption_info(struct inode *inode, bool allow_unsupported)
{
	int res;
	union fscrypt_context ctx;
	union fscrypt_policy policy;

	if (fscrypt_has_encryption_key(inode))
		return 0;

	res = inode->i_sb->s_cop->get_context(inode, &ctx, sizeof(ctx));
	if (res < 0) {
		if (res == -ERANGE && allow_unsupported)
			return 0;
		fscrypt_warn(inode, "Error %d getting encryption context", res);
		return res;
	}

	res = fscrypt_policy_from_context(&policy, &ctx, res);
	if (res) {
		if (allow_unsupported)
			return 0;
		fscrypt_warn(inode,
			     "Unrecognized or corrupt encryption context");
		return res;
	}

	if (!fscrypt_supported_policy(&policy, inode)) {
		if (allow_unsupported)
			return 0;
		return -EINVAL;
	}

	res = fscrypt_setup_encryption_info(inode, &policy,
					    fscrypt_context_nonce(&ctx),
					    IS_CASEFOLDED(inode) &&
					    S_ISDIR(inode->i_mode));

	if (res == -ENOPKG && allow_unsupported)  
		res = 0;
	if (res == -ENOKEY)
		res = 0;
	return res;
}

 
int fscrypt_prepare_new_inode(struct inode *dir, struct inode *inode,
			      bool *encrypt_ret)
{
	const union fscrypt_policy *policy;
	u8 nonce[FSCRYPT_FILE_NONCE_SIZE];

	policy = fscrypt_policy_to_inherit(dir);
	if (policy == NULL)
		return 0;
	if (IS_ERR(policy))
		return PTR_ERR(policy);

	if (WARN_ON_ONCE(inode->i_mode == 0))
		return -EINVAL;

	 
	if (!S_ISREG(inode->i_mode) &&
	    !S_ISDIR(inode->i_mode) &&
	    !S_ISLNK(inode->i_mode))
		return 0;

	*encrypt_ret = true;

	get_random_bytes(nonce, FSCRYPT_FILE_NONCE_SIZE);
	return fscrypt_setup_encryption_info(inode, policy, nonce,
					     IS_CASEFOLDED(dir) &&
					     S_ISDIR(inode->i_mode));
}
EXPORT_SYMBOL_GPL(fscrypt_prepare_new_inode);

 
void fscrypt_put_encryption_info(struct inode *inode)
{
	put_crypt_info(inode->i_crypt_info);
	inode->i_crypt_info = NULL;
}
EXPORT_SYMBOL(fscrypt_put_encryption_info);

 
void fscrypt_free_inode(struct inode *inode)
{
	if (IS_ENCRYPTED(inode) && S_ISLNK(inode->i_mode)) {
		kfree(inode->i_link);
		inode->i_link = NULL;
	}
}
EXPORT_SYMBOL(fscrypt_free_inode);

 
int fscrypt_drop_inode(struct inode *inode)
{
	const struct fscrypt_info *ci = fscrypt_get_info(inode);

	 
	if (!ci || !ci->ci_master_key)
		return 0;

	 
	if (inode->i_state & I_DIRTY_ALL)
		return 0;

	 
	return !is_master_key_secret_present(&ci->ci_master_key->mk_secret);
}
EXPORT_SYMBOL_GPL(fscrypt_drop_inode);
