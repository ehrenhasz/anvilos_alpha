
 

 

#include <crypto/algapi.h>
#include <crypto/skcipher.h>
#include <keys/user-type.h>
#include <linux/hashtable.h>
#include <linux/scatterlist.h>

#include "fscrypt_private.h"

 
static DEFINE_HASHTABLE(fscrypt_direct_keys, 6);  
static DEFINE_SPINLOCK(fscrypt_direct_keys_lock);

 
static int derive_key_aes(const u8 *master_key,
			  const u8 nonce[FSCRYPT_FILE_NONCE_SIZE],
			  u8 *derived_key, unsigned int derived_keysize)
{
	int res = 0;
	struct skcipher_request *req = NULL;
	DECLARE_CRYPTO_WAIT(wait);
	struct scatterlist src_sg, dst_sg;
	struct crypto_skcipher *tfm = crypto_alloc_skcipher("ecb(aes)", 0, 0);

	if (IS_ERR(tfm)) {
		res = PTR_ERR(tfm);
		tfm = NULL;
		goto out;
	}
	crypto_skcipher_set_flags(tfm, CRYPTO_TFM_REQ_FORBID_WEAK_KEYS);
	req = skcipher_request_alloc(tfm, GFP_KERNEL);
	if (!req) {
		res = -ENOMEM;
		goto out;
	}
	skcipher_request_set_callback(req,
			CRYPTO_TFM_REQ_MAY_BACKLOG | CRYPTO_TFM_REQ_MAY_SLEEP,
			crypto_req_done, &wait);
	res = crypto_skcipher_setkey(tfm, nonce, FSCRYPT_FILE_NONCE_SIZE);
	if (res < 0)
		goto out;

	sg_init_one(&src_sg, master_key, derived_keysize);
	sg_init_one(&dst_sg, derived_key, derived_keysize);
	skcipher_request_set_crypt(req, &src_sg, &dst_sg, derived_keysize,
				   NULL);
	res = crypto_wait_req(crypto_skcipher_encrypt(req), &wait);
out:
	skcipher_request_free(req);
	crypto_free_skcipher(tfm);
	return res;
}

 
static struct key *
find_and_lock_process_key(const char *prefix,
			  const u8 descriptor[FSCRYPT_KEY_DESCRIPTOR_SIZE],
			  unsigned int min_keysize,
			  const struct fscrypt_key **payload_ret)
{
	char *description;
	struct key *key;
	const struct user_key_payload *ukp;
	const struct fscrypt_key *payload;

	description = kasprintf(GFP_KERNEL, "%s%*phN", prefix,
				FSCRYPT_KEY_DESCRIPTOR_SIZE, descriptor);
	if (!description)
		return ERR_PTR(-ENOMEM);

	key = request_key(&key_type_logon, description, NULL);
	kfree(description);
	if (IS_ERR(key))
		return key;

	down_read(&key->sem);
	ukp = user_key_payload_locked(key);

	if (!ukp)  
		goto invalid;

	payload = (const struct fscrypt_key *)ukp->data;

	if (ukp->datalen != sizeof(struct fscrypt_key) ||
	    payload->size < 1 || payload->size > FSCRYPT_MAX_KEY_SIZE) {
		fscrypt_warn(NULL,
			     "key with description '%s' has invalid payload",
			     key->description);
		goto invalid;
	}

	if (payload->size < min_keysize) {
		fscrypt_warn(NULL,
			     "key with description '%s' is too short (got %u bytes, need %u+ bytes)",
			     key->description, payload->size, min_keysize);
		goto invalid;
	}

	*payload_ret = payload;
	return key;

invalid:
	up_read(&key->sem);
	key_put(key);
	return ERR_PTR(-ENOKEY);
}

 
struct fscrypt_direct_key {
	struct super_block		*dk_sb;
	struct hlist_node		dk_node;
	refcount_t			dk_refcount;
	const struct fscrypt_mode	*dk_mode;
	struct fscrypt_prepared_key	dk_key;
	u8				dk_descriptor[FSCRYPT_KEY_DESCRIPTOR_SIZE];
	u8				dk_raw[FSCRYPT_MAX_KEY_SIZE];
};

static void free_direct_key(struct fscrypt_direct_key *dk)
{
	if (dk) {
		fscrypt_destroy_prepared_key(dk->dk_sb, &dk->dk_key);
		kfree_sensitive(dk);
	}
}

void fscrypt_put_direct_key(struct fscrypt_direct_key *dk)
{
	if (!refcount_dec_and_lock(&dk->dk_refcount, &fscrypt_direct_keys_lock))
		return;
	hash_del(&dk->dk_node);
	spin_unlock(&fscrypt_direct_keys_lock);

	free_direct_key(dk);
}

 
static struct fscrypt_direct_key *
find_or_insert_direct_key(struct fscrypt_direct_key *to_insert,
			  const u8 *raw_key, const struct fscrypt_info *ci)
{
	unsigned long hash_key;
	struct fscrypt_direct_key *dk;

	 

	BUILD_BUG_ON(sizeof(hash_key) > FSCRYPT_KEY_DESCRIPTOR_SIZE);
	memcpy(&hash_key, ci->ci_policy.v1.master_key_descriptor,
	       sizeof(hash_key));

	spin_lock(&fscrypt_direct_keys_lock);
	hash_for_each_possible(fscrypt_direct_keys, dk, dk_node, hash_key) {
		if (memcmp(ci->ci_policy.v1.master_key_descriptor,
			   dk->dk_descriptor, FSCRYPT_KEY_DESCRIPTOR_SIZE) != 0)
			continue;
		if (ci->ci_mode != dk->dk_mode)
			continue;
		if (!fscrypt_is_key_prepared(&dk->dk_key, ci))
			continue;
		if (crypto_memneq(raw_key, dk->dk_raw, ci->ci_mode->keysize))
			continue;
		 
		refcount_inc(&dk->dk_refcount);
		spin_unlock(&fscrypt_direct_keys_lock);
		free_direct_key(to_insert);
		return dk;
	}
	if (to_insert)
		hash_add(fscrypt_direct_keys, &to_insert->dk_node, hash_key);
	spin_unlock(&fscrypt_direct_keys_lock);
	return to_insert;
}

 
static struct fscrypt_direct_key *
fscrypt_get_direct_key(const struct fscrypt_info *ci, const u8 *raw_key)
{
	struct fscrypt_direct_key *dk;
	int err;

	 
	dk = find_or_insert_direct_key(NULL, raw_key, ci);
	if (dk)
		return dk;

	 
	dk = kzalloc(sizeof(*dk), GFP_KERNEL);
	if (!dk)
		return ERR_PTR(-ENOMEM);
	dk->dk_sb = ci->ci_inode->i_sb;
	refcount_set(&dk->dk_refcount, 1);
	dk->dk_mode = ci->ci_mode;
	err = fscrypt_prepare_key(&dk->dk_key, raw_key, ci);
	if (err)
		goto err_free_dk;
	memcpy(dk->dk_descriptor, ci->ci_policy.v1.master_key_descriptor,
	       FSCRYPT_KEY_DESCRIPTOR_SIZE);
	memcpy(dk->dk_raw, raw_key, ci->ci_mode->keysize);

	return find_or_insert_direct_key(dk, raw_key, ci);

err_free_dk:
	free_direct_key(dk);
	return ERR_PTR(err);
}

 
static int setup_v1_file_key_direct(struct fscrypt_info *ci,
				    const u8 *raw_master_key)
{
	struct fscrypt_direct_key *dk;

	dk = fscrypt_get_direct_key(ci, raw_master_key);
	if (IS_ERR(dk))
		return PTR_ERR(dk);
	ci->ci_direct_key = dk;
	ci->ci_enc_key = dk->dk_key;
	return 0;
}

 
static int setup_v1_file_key_derived(struct fscrypt_info *ci,
				     const u8 *raw_master_key)
{
	u8 *derived_key;
	int err;

	 
	derived_key = kmalloc(ci->ci_mode->keysize, GFP_KERNEL);
	if (!derived_key)
		return -ENOMEM;

	err = derive_key_aes(raw_master_key, ci->ci_nonce,
			     derived_key, ci->ci_mode->keysize);
	if (err)
		goto out;

	err = fscrypt_set_per_file_enc_key(ci, derived_key);
out:
	kfree_sensitive(derived_key);
	return err;
}

int fscrypt_setup_v1_file_key(struct fscrypt_info *ci, const u8 *raw_master_key)
{
	if (ci->ci_policy.v1.flags & FSCRYPT_POLICY_FLAG_DIRECT_KEY)
		return setup_v1_file_key_direct(ci, raw_master_key);
	else
		return setup_v1_file_key_derived(ci, raw_master_key);
}

int fscrypt_setup_v1_file_key_via_subscribed_keyrings(struct fscrypt_info *ci)
{
	struct key *key;
	const struct fscrypt_key *payload;
	int err;

	key = find_and_lock_process_key(FSCRYPT_KEY_DESC_PREFIX,
					ci->ci_policy.v1.master_key_descriptor,
					ci->ci_mode->keysize, &payload);
	if (key == ERR_PTR(-ENOKEY) && ci->ci_inode->i_sb->s_cop->key_prefix) {
		key = find_and_lock_process_key(ci->ci_inode->i_sb->s_cop->key_prefix,
						ci->ci_policy.v1.master_key_descriptor,
						ci->ci_mode->keysize, &payload);
	}
	if (IS_ERR(key))
		return PTR_ERR(key);

	err = fscrypt_setup_v1_file_key(ci, payload->raw);
	up_read(&key->sem);
	key_put(key);
	return err;
}
