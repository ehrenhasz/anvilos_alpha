 
 

#ifndef _FSCRYPT_PRIVATE_H
#define _FSCRYPT_PRIVATE_H

#include <linux/fscrypt.h>
#include <linux/siphash.h>
#include <crypto/hash.h>
#include <linux/blk-crypto.h>

#define CONST_STRLEN(str)	(sizeof(str) - 1)

#define FSCRYPT_FILE_NONCE_SIZE	16

 
#define FSCRYPT_MIN_KEY_SIZE	16

#define FSCRYPT_CONTEXT_V1	1
#define FSCRYPT_CONTEXT_V2	2

 
#define FSCRYPT_MODE_MAX	FSCRYPT_MODE_AES_256_HCTR2

struct fscrypt_context_v1 {
	u8 version;  
	u8 contents_encryption_mode;
	u8 filenames_encryption_mode;
	u8 flags;
	u8 master_key_descriptor[FSCRYPT_KEY_DESCRIPTOR_SIZE];
	u8 nonce[FSCRYPT_FILE_NONCE_SIZE];
};

struct fscrypt_context_v2 {
	u8 version;  
	u8 contents_encryption_mode;
	u8 filenames_encryption_mode;
	u8 flags;
	u8 __reserved[4];
	u8 master_key_identifier[FSCRYPT_KEY_IDENTIFIER_SIZE];
	u8 nonce[FSCRYPT_FILE_NONCE_SIZE];
};

 
union fscrypt_context {
	u8 version;
	struct fscrypt_context_v1 v1;
	struct fscrypt_context_v2 v2;
};

 
static inline int fscrypt_context_size(const union fscrypt_context *ctx)
{
	switch (ctx->version) {
	case FSCRYPT_CONTEXT_V1:
		BUILD_BUG_ON(sizeof(ctx->v1) != 28);
		return sizeof(ctx->v1);
	case FSCRYPT_CONTEXT_V2:
		BUILD_BUG_ON(sizeof(ctx->v2) != 40);
		return sizeof(ctx->v2);
	}
	return 0;
}

 
static inline bool fscrypt_context_is_valid(const union fscrypt_context *ctx,
					    int ctx_size)
{
	return ctx_size >= 1 && ctx_size == fscrypt_context_size(ctx);
}

 
static inline const u8 *fscrypt_context_nonce(const union fscrypt_context *ctx)
{
	switch (ctx->version) {
	case FSCRYPT_CONTEXT_V1:
		return ctx->v1.nonce;
	case FSCRYPT_CONTEXT_V2:
		return ctx->v2.nonce;
	}
	WARN_ON_ONCE(1);
	return NULL;
}

union fscrypt_policy {
	u8 version;
	struct fscrypt_policy_v1 v1;
	struct fscrypt_policy_v2 v2;
};

 
static inline int fscrypt_policy_size(const union fscrypt_policy *policy)
{
	switch (policy->version) {
	case FSCRYPT_POLICY_V1:
		return sizeof(policy->v1);
	case FSCRYPT_POLICY_V2:
		return sizeof(policy->v2);
	}
	return 0;
}

 
static inline u8
fscrypt_policy_contents_mode(const union fscrypt_policy *policy)
{
	switch (policy->version) {
	case FSCRYPT_POLICY_V1:
		return policy->v1.contents_encryption_mode;
	case FSCRYPT_POLICY_V2:
		return policy->v2.contents_encryption_mode;
	}
	BUG();
}

 
static inline u8
fscrypt_policy_fnames_mode(const union fscrypt_policy *policy)
{
	switch (policy->version) {
	case FSCRYPT_POLICY_V1:
		return policy->v1.filenames_encryption_mode;
	case FSCRYPT_POLICY_V2:
		return policy->v2.filenames_encryption_mode;
	}
	BUG();
}

 
static inline u8
fscrypt_policy_flags(const union fscrypt_policy *policy)
{
	switch (policy->version) {
	case FSCRYPT_POLICY_V1:
		return policy->v1.flags;
	case FSCRYPT_POLICY_V2:
		return policy->v2.flags;
	}
	BUG();
}

 
struct fscrypt_symlink_data {
	__le16 len;
	char encrypted_path[];
} __packed;

 
struct fscrypt_prepared_key {
	struct crypto_skcipher *tfm;
#ifdef CONFIG_FS_ENCRYPTION_INLINE_CRYPT
	struct blk_crypto_key *blk_key;
#endif
};

 
struct fscrypt_info {

	 
	struct fscrypt_prepared_key ci_enc_key;

	 
	bool ci_owns_key;

#ifdef CONFIG_FS_ENCRYPTION_INLINE_CRYPT
	 
	bool ci_inlinecrypt;
#endif

	 
	struct fscrypt_mode *ci_mode;

	 
	struct inode *ci_inode;

	 
	struct fscrypt_master_key *ci_master_key;

	 
	struct list_head ci_master_key_link;

	 
	struct fscrypt_direct_key *ci_direct_key;

	 
	siphash_key_t ci_dirhash_key;
	bool ci_dirhash_key_initialized;

	 
	union fscrypt_policy ci_policy;

	 
	u8 ci_nonce[FSCRYPT_FILE_NONCE_SIZE];

	 
	u32 ci_hashed_ino;
};

typedef enum {
	FS_DECRYPT = 0,
	FS_ENCRYPT,
} fscrypt_direction_t;

 
extern struct kmem_cache *fscrypt_info_cachep;
int fscrypt_initialize(struct super_block *sb);
int fscrypt_crypt_block(const struct inode *inode, fscrypt_direction_t rw,
			u64 lblk_num, struct page *src_page,
			struct page *dest_page, unsigned int len,
			unsigned int offs, gfp_t gfp_flags);
struct page *fscrypt_alloc_bounce_page(gfp_t gfp_flags);

void __printf(3, 4) __cold
fscrypt_msg(const struct inode *inode, const char *level, const char *fmt, ...);

#define fscrypt_warn(inode, fmt, ...)		\
	fscrypt_msg((inode), KERN_WARNING, fmt, ##__VA_ARGS__)
#define fscrypt_err(inode, fmt, ...)		\
	fscrypt_msg((inode), KERN_ERR, fmt, ##__VA_ARGS__)

#define FSCRYPT_MAX_IV_SIZE	32

union fscrypt_iv {
	struct {
		 
		__le64 lblk_num;

		 
		u8 nonce[FSCRYPT_FILE_NONCE_SIZE];
	};
	u8 raw[FSCRYPT_MAX_IV_SIZE];
	__le64 dun[FSCRYPT_MAX_IV_SIZE / sizeof(__le64)];
};

void fscrypt_generate_iv(union fscrypt_iv *iv, u64 lblk_num,
			 const struct fscrypt_info *ci);

 
bool __fscrypt_fname_encrypted_size(const union fscrypt_policy *policy,
				    u32 orig_len, u32 max_len,
				    u32 *encrypted_len_ret);

 
struct fscrypt_hkdf {
	struct crypto_shash *hmac_tfm;
};

int fscrypt_init_hkdf(struct fscrypt_hkdf *hkdf, const u8 *master_key,
		      unsigned int master_key_size);

 
#define HKDF_CONTEXT_KEY_IDENTIFIER	1  
#define HKDF_CONTEXT_PER_FILE_ENC_KEY	2  
#define HKDF_CONTEXT_DIRECT_KEY		3  
#define HKDF_CONTEXT_IV_INO_LBLK_64_KEY	4  
#define HKDF_CONTEXT_DIRHASH_KEY	5  
#define HKDF_CONTEXT_IV_INO_LBLK_32_KEY	6  
#define HKDF_CONTEXT_INODE_HASH_KEY	7  

int fscrypt_hkdf_expand(const struct fscrypt_hkdf *hkdf, u8 context,
			const u8 *info, unsigned int infolen,
			u8 *okm, unsigned int okmlen);

void fscrypt_destroy_hkdf(struct fscrypt_hkdf *hkdf);

 
#ifdef CONFIG_FS_ENCRYPTION_INLINE_CRYPT
int fscrypt_select_encryption_impl(struct fscrypt_info *ci);

static inline bool
fscrypt_using_inline_encryption(const struct fscrypt_info *ci)
{
	return ci->ci_inlinecrypt;
}

int fscrypt_prepare_inline_crypt_key(struct fscrypt_prepared_key *prep_key,
				     const u8 *raw_key,
				     const struct fscrypt_info *ci);

void fscrypt_destroy_inline_crypt_key(struct super_block *sb,
				      struct fscrypt_prepared_key *prep_key);

 
static inline bool
fscrypt_is_key_prepared(struct fscrypt_prepared_key *prep_key,
			const struct fscrypt_info *ci)
{
	 
	if (fscrypt_using_inline_encryption(ci))
		return smp_load_acquire(&prep_key->blk_key) != NULL;
	return smp_load_acquire(&prep_key->tfm) != NULL;
}

#else  

static inline int fscrypt_select_encryption_impl(struct fscrypt_info *ci)
{
	return 0;
}

static inline bool
fscrypt_using_inline_encryption(const struct fscrypt_info *ci)
{
	return false;
}

static inline int
fscrypt_prepare_inline_crypt_key(struct fscrypt_prepared_key *prep_key,
				 const u8 *raw_key,
				 const struct fscrypt_info *ci)
{
	WARN_ON_ONCE(1);
	return -EOPNOTSUPP;
}

static inline void
fscrypt_destroy_inline_crypt_key(struct super_block *sb,
				 struct fscrypt_prepared_key *prep_key)
{
}

static inline bool
fscrypt_is_key_prepared(struct fscrypt_prepared_key *prep_key,
			const struct fscrypt_info *ci)
{
	return smp_load_acquire(&prep_key->tfm) != NULL;
}
#endif  

 

 
struct fscrypt_master_key_secret {

	 
	struct fscrypt_hkdf	hkdf;

	 
	u32			size;

	 
	u8			raw[FSCRYPT_MAX_KEY_SIZE];

} __randomize_layout;

 
struct fscrypt_master_key {

	 
	struct hlist_node			mk_node;

	 
	struct rw_semaphore			mk_sem;

	 
	refcount_t				mk_active_refs;
	refcount_t				mk_struct_refs;

	struct rcu_head				mk_rcu_head;

	 
	struct fscrypt_master_key_secret	mk_secret;

	 
	struct fscrypt_key_specifier		mk_spec;

	 
	struct key		*mk_users;

	 
	struct list_head	mk_decrypted_inodes;
	spinlock_t		mk_decrypted_inodes_lock;

	 
	struct fscrypt_prepared_key mk_direct_keys[FSCRYPT_MODE_MAX + 1];
	struct fscrypt_prepared_key mk_iv_ino_lblk_64_keys[FSCRYPT_MODE_MAX + 1];
	struct fscrypt_prepared_key mk_iv_ino_lblk_32_keys[FSCRYPT_MODE_MAX + 1];

	 
	siphash_key_t		mk_ino_hash_key;
	bool			mk_ino_hash_key_initialized;

} __randomize_layout;

static inline bool
is_master_key_secret_present(const struct fscrypt_master_key_secret *secret)
{
	 
	return READ_ONCE(secret->size) != 0;
}

static inline const char *master_key_spec_type(
				const struct fscrypt_key_specifier *spec)
{
	switch (spec->type) {
	case FSCRYPT_KEY_SPEC_TYPE_DESCRIPTOR:
		return "descriptor";
	case FSCRYPT_KEY_SPEC_TYPE_IDENTIFIER:
		return "identifier";
	}
	return "[unknown]";
}

static inline int master_key_spec_len(const struct fscrypt_key_specifier *spec)
{
	switch (spec->type) {
	case FSCRYPT_KEY_SPEC_TYPE_DESCRIPTOR:
		return FSCRYPT_KEY_DESCRIPTOR_SIZE;
	case FSCRYPT_KEY_SPEC_TYPE_IDENTIFIER:
		return FSCRYPT_KEY_IDENTIFIER_SIZE;
	}
	return 0;
}

void fscrypt_put_master_key(struct fscrypt_master_key *mk);

void fscrypt_put_master_key_activeref(struct super_block *sb,
				      struct fscrypt_master_key *mk);

struct fscrypt_master_key *
fscrypt_find_master_key(struct super_block *sb,
			const struct fscrypt_key_specifier *mk_spec);

int fscrypt_get_test_dummy_key_identifier(
			  u8 key_identifier[FSCRYPT_KEY_IDENTIFIER_SIZE]);

int fscrypt_add_test_dummy_key(struct super_block *sb,
			       struct fscrypt_key_specifier *key_spec);

int fscrypt_verify_key_added(struct super_block *sb,
			     const u8 identifier[FSCRYPT_KEY_IDENTIFIER_SIZE]);

int __init fscrypt_init_keyring(void);

 

struct fscrypt_mode {
	const char *friendly_name;
	const char *cipher_str;
	int keysize;		 
	int security_strength;	 
	int ivsize;		 
	int logged_cryptoapi_impl;
	int logged_blk_crypto_native;
	int logged_blk_crypto_fallback;
	enum blk_crypto_mode_num blk_crypto_mode;
};

extern struct fscrypt_mode fscrypt_modes[];

int fscrypt_prepare_key(struct fscrypt_prepared_key *prep_key,
			const u8 *raw_key, const struct fscrypt_info *ci);

void fscrypt_destroy_prepared_key(struct super_block *sb,
				  struct fscrypt_prepared_key *prep_key);

int fscrypt_set_per_file_enc_key(struct fscrypt_info *ci, const u8 *raw_key);

int fscrypt_derive_dirhash_key(struct fscrypt_info *ci,
			       const struct fscrypt_master_key *mk);

void fscrypt_hash_inode_number(struct fscrypt_info *ci,
			       const struct fscrypt_master_key *mk);

int fscrypt_get_encryption_info(struct inode *inode, bool allow_unsupported);

 
static inline int fscrypt_require_key(struct inode *inode)
{
	if (IS_ENCRYPTED(inode)) {
		int err = fscrypt_get_encryption_info(inode, false);

		if (err)
			return err;
		if (!fscrypt_has_encryption_key(inode))
			return -ENOKEY;
	}
	return 0;
}

 

void fscrypt_put_direct_key(struct fscrypt_direct_key *dk);

int fscrypt_setup_v1_file_key(struct fscrypt_info *ci,
			      const u8 *raw_master_key);

int fscrypt_setup_v1_file_key_via_subscribed_keyrings(struct fscrypt_info *ci);

 

bool fscrypt_policies_equal(const union fscrypt_policy *policy1,
			    const union fscrypt_policy *policy2);
int fscrypt_policy_to_key_spec(const union fscrypt_policy *policy,
			       struct fscrypt_key_specifier *key_spec);
const union fscrypt_policy *fscrypt_get_dummy_policy(struct super_block *sb);
bool fscrypt_supported_policy(const union fscrypt_policy *policy_u,
			      const struct inode *inode);
int fscrypt_policy_from_context(union fscrypt_policy *policy_u,
				const union fscrypt_context *ctx_u,
				int ctx_size);
const union fscrypt_policy *fscrypt_policy_to_inherit(struct inode *dir);

#endif  
