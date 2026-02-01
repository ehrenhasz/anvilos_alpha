
 

 

#include <asm/unaligned.h>
#include <crypto/skcipher.h>
#include <linux/key-type.h>
#include <linux/random.h>
#include <linux/seq_file.h>

#include "fscrypt_private.h"

 
struct fscrypt_keyring {
	 
	spinlock_t lock;

	 
	struct hlist_head key_hashtable[128];
};

static void wipe_master_key_secret(struct fscrypt_master_key_secret *secret)
{
	fscrypt_destroy_hkdf(&secret->hkdf);
	memzero_explicit(secret, sizeof(*secret));
}

static void move_master_key_secret(struct fscrypt_master_key_secret *dst,
				   struct fscrypt_master_key_secret *src)
{
	memcpy(dst, src, sizeof(*dst));
	memzero_explicit(src, sizeof(*src));
}

static void fscrypt_free_master_key(struct rcu_head *head)
{
	struct fscrypt_master_key *mk =
		container_of(head, struct fscrypt_master_key, mk_rcu_head);
	 
	kfree_sensitive(mk);
}

void fscrypt_put_master_key(struct fscrypt_master_key *mk)
{
	if (!refcount_dec_and_test(&mk->mk_struct_refs))
		return;
	 
	WARN_ON_ONCE(refcount_read(&mk->mk_active_refs) != 0);
	key_put(mk->mk_users);
	mk->mk_users = NULL;
	call_rcu(&mk->mk_rcu_head, fscrypt_free_master_key);
}

void fscrypt_put_master_key_activeref(struct super_block *sb,
				      struct fscrypt_master_key *mk)
{
	size_t i;

	if (!refcount_dec_and_test(&mk->mk_active_refs))
		return;
	 

	if (WARN_ON_ONCE(!sb->s_master_keys))
		return;
	spin_lock(&sb->s_master_keys->lock);
	hlist_del_rcu(&mk->mk_node);
	spin_unlock(&sb->s_master_keys->lock);

	 
	WARN_ON_ONCE(is_master_key_secret_present(&mk->mk_secret));
	WARN_ON_ONCE(!list_empty(&mk->mk_decrypted_inodes));

	for (i = 0; i <= FSCRYPT_MODE_MAX; i++) {
		fscrypt_destroy_prepared_key(
				sb, &mk->mk_direct_keys[i]);
		fscrypt_destroy_prepared_key(
				sb, &mk->mk_iv_ino_lblk_64_keys[i]);
		fscrypt_destroy_prepared_key(
				sb, &mk->mk_iv_ino_lblk_32_keys[i]);
	}
	memzero_explicit(&mk->mk_ino_hash_key,
			 sizeof(mk->mk_ino_hash_key));
	mk->mk_ino_hash_key_initialized = false;

	 
	fscrypt_put_master_key(mk);
}

static inline bool valid_key_spec(const struct fscrypt_key_specifier *spec)
{
	if (spec->__reserved)
		return false;
	return master_key_spec_len(spec) != 0;
}

static int fscrypt_user_key_instantiate(struct key *key,
					struct key_preparsed_payload *prep)
{
	 
	return key_payload_reserve(key, FSCRYPT_MAX_KEY_SIZE);
}

static void fscrypt_user_key_describe(const struct key *key, struct seq_file *m)
{
	seq_puts(m, key->description);
}

 
static struct key_type key_type_fscrypt_user = {
	.name			= ".fscrypt",
	.instantiate		= fscrypt_user_key_instantiate,
	.describe		= fscrypt_user_key_describe,
};

#define FSCRYPT_MK_USERS_DESCRIPTION_SIZE	\
	(CONST_STRLEN("fscrypt-") + 2 * FSCRYPT_KEY_IDENTIFIER_SIZE + \
	 CONST_STRLEN("-users") + 1)

#define FSCRYPT_MK_USER_DESCRIPTION_SIZE	\
	(2 * FSCRYPT_KEY_IDENTIFIER_SIZE + CONST_STRLEN(".uid.") + 10 + 1)

static void format_mk_users_keyring_description(
			char description[FSCRYPT_MK_USERS_DESCRIPTION_SIZE],
			const u8 mk_identifier[FSCRYPT_KEY_IDENTIFIER_SIZE])
{
	sprintf(description, "fscrypt-%*phN-users",
		FSCRYPT_KEY_IDENTIFIER_SIZE, mk_identifier);
}

static void format_mk_user_description(
			char description[FSCRYPT_MK_USER_DESCRIPTION_SIZE],
			const u8 mk_identifier[FSCRYPT_KEY_IDENTIFIER_SIZE])
{

	sprintf(description, "%*phN.uid.%u", FSCRYPT_KEY_IDENTIFIER_SIZE,
		mk_identifier, __kuid_val(current_fsuid()));
}

 
static int allocate_filesystem_keyring(struct super_block *sb)
{
	struct fscrypt_keyring *keyring;

	if (sb->s_master_keys)
		return 0;

	keyring = kzalloc(sizeof(*keyring), GFP_KERNEL);
	if (!keyring)
		return -ENOMEM;
	spin_lock_init(&keyring->lock);
	 
	smp_store_release(&sb->s_master_keys, keyring);
	return 0;
}

 
void fscrypt_destroy_keyring(struct super_block *sb)
{
	struct fscrypt_keyring *keyring = sb->s_master_keys;
	size_t i;

	if (!keyring)
		return;

	for (i = 0; i < ARRAY_SIZE(keyring->key_hashtable); i++) {
		struct hlist_head *bucket = &keyring->key_hashtable[i];
		struct fscrypt_master_key *mk;
		struct hlist_node *tmp;

		hlist_for_each_entry_safe(mk, tmp, bucket, mk_node) {
			 
			WARN_ON_ONCE(refcount_read(&mk->mk_active_refs) != 1);
			WARN_ON_ONCE(refcount_read(&mk->mk_struct_refs) != 1);
			WARN_ON_ONCE(!is_master_key_secret_present(&mk->mk_secret));
			wipe_master_key_secret(&mk->mk_secret);
			fscrypt_put_master_key_activeref(sb, mk);
		}
	}
	kfree_sensitive(keyring);
	sb->s_master_keys = NULL;
}

static struct hlist_head *
fscrypt_mk_hash_bucket(struct fscrypt_keyring *keyring,
		       const struct fscrypt_key_specifier *mk_spec)
{
	 
	unsigned long i = get_unaligned((unsigned long *)&mk_spec->u);

	return &keyring->key_hashtable[i % ARRAY_SIZE(keyring->key_hashtable)];
}

 
struct fscrypt_master_key *
fscrypt_find_master_key(struct super_block *sb,
			const struct fscrypt_key_specifier *mk_spec)
{
	struct fscrypt_keyring *keyring;
	struct hlist_head *bucket;
	struct fscrypt_master_key *mk;

	 
	keyring = smp_load_acquire(&sb->s_master_keys);
	if (keyring == NULL)
		return NULL;  

	bucket = fscrypt_mk_hash_bucket(keyring, mk_spec);
	rcu_read_lock();
	switch (mk_spec->type) {
	case FSCRYPT_KEY_SPEC_TYPE_DESCRIPTOR:
		hlist_for_each_entry_rcu(mk, bucket, mk_node) {
			if (mk->mk_spec.type ==
				FSCRYPT_KEY_SPEC_TYPE_DESCRIPTOR &&
			    memcmp(mk->mk_spec.u.descriptor,
				   mk_spec->u.descriptor,
				   FSCRYPT_KEY_DESCRIPTOR_SIZE) == 0 &&
			    refcount_inc_not_zero(&mk->mk_struct_refs))
				goto out;
		}
		break;
	case FSCRYPT_KEY_SPEC_TYPE_IDENTIFIER:
		hlist_for_each_entry_rcu(mk, bucket, mk_node) {
			if (mk->mk_spec.type ==
				FSCRYPT_KEY_SPEC_TYPE_IDENTIFIER &&
			    memcmp(mk->mk_spec.u.identifier,
				   mk_spec->u.identifier,
				   FSCRYPT_KEY_IDENTIFIER_SIZE) == 0 &&
			    refcount_inc_not_zero(&mk->mk_struct_refs))
				goto out;
		}
		break;
	}
	mk = NULL;
out:
	rcu_read_unlock();
	return mk;
}

static int allocate_master_key_users_keyring(struct fscrypt_master_key *mk)
{
	char description[FSCRYPT_MK_USERS_DESCRIPTION_SIZE];
	struct key *keyring;

	format_mk_users_keyring_description(description,
					    mk->mk_spec.u.identifier);
	keyring = keyring_alloc(description, GLOBAL_ROOT_UID, GLOBAL_ROOT_GID,
				current_cred(), KEY_POS_SEARCH |
				  KEY_USR_SEARCH | KEY_USR_READ | KEY_USR_VIEW,
				KEY_ALLOC_NOT_IN_QUOTA, NULL, NULL);
	if (IS_ERR(keyring))
		return PTR_ERR(keyring);

	mk->mk_users = keyring;
	return 0;
}

 
static struct key *find_master_key_user(struct fscrypt_master_key *mk)
{
	char description[FSCRYPT_MK_USER_DESCRIPTION_SIZE];
	key_ref_t keyref;

	format_mk_user_description(description, mk->mk_spec.u.identifier);

	 
	keyref = keyring_search(make_key_ref(mk->mk_users, true  ),
				&key_type_fscrypt_user, description, false);
	if (IS_ERR(keyref)) {
		if (PTR_ERR(keyref) == -EAGAIN ||  
		    PTR_ERR(keyref) == -EKEYREVOKED)  
			keyref = ERR_PTR(-ENOKEY);
		return ERR_CAST(keyref);
	}
	return key_ref_to_ptr(keyref);
}

 
static int add_master_key_user(struct fscrypt_master_key *mk)
{
	char description[FSCRYPT_MK_USER_DESCRIPTION_SIZE];
	struct key *mk_user;
	int err;

	format_mk_user_description(description, mk->mk_spec.u.identifier);
	mk_user = key_alloc(&key_type_fscrypt_user, description,
			    current_fsuid(), current_gid(), current_cred(),
			    KEY_POS_SEARCH | KEY_USR_VIEW, 0, NULL);
	if (IS_ERR(mk_user))
		return PTR_ERR(mk_user);

	err = key_instantiate_and_link(mk_user, NULL, 0, mk->mk_users, NULL);
	key_put(mk_user);
	return err;
}

 
static int remove_master_key_user(struct fscrypt_master_key *mk)
{
	struct key *mk_user;
	int err;

	mk_user = find_master_key_user(mk);
	if (IS_ERR(mk_user))
		return PTR_ERR(mk_user);
	err = key_unlink(mk->mk_users, mk_user);
	key_put(mk_user);
	return err;
}

 
static int add_new_master_key(struct super_block *sb,
			      struct fscrypt_master_key_secret *secret,
			      const struct fscrypt_key_specifier *mk_spec)
{
	struct fscrypt_keyring *keyring = sb->s_master_keys;
	struct fscrypt_master_key *mk;
	int err;

	mk = kzalloc(sizeof(*mk), GFP_KERNEL);
	if (!mk)
		return -ENOMEM;

	init_rwsem(&mk->mk_sem);
	refcount_set(&mk->mk_struct_refs, 1);
	mk->mk_spec = *mk_spec;

	INIT_LIST_HEAD(&mk->mk_decrypted_inodes);
	spin_lock_init(&mk->mk_decrypted_inodes_lock);

	if (mk_spec->type == FSCRYPT_KEY_SPEC_TYPE_IDENTIFIER) {
		err = allocate_master_key_users_keyring(mk);
		if (err)
			goto out_put;
		err = add_master_key_user(mk);
		if (err)
			goto out_put;
	}

	move_master_key_secret(&mk->mk_secret, secret);
	refcount_set(&mk->mk_active_refs, 1);  

	spin_lock(&keyring->lock);
	hlist_add_head_rcu(&mk->mk_node,
			   fscrypt_mk_hash_bucket(keyring, mk_spec));
	spin_unlock(&keyring->lock);
	return 0;

out_put:
	fscrypt_put_master_key(mk);
	return err;
}

#define KEY_DEAD	1

static int add_existing_master_key(struct fscrypt_master_key *mk,
				   struct fscrypt_master_key_secret *secret)
{
	int err;

	 
	if (mk->mk_users) {
		struct key *mk_user = find_master_key_user(mk);

		if (mk_user != ERR_PTR(-ENOKEY)) {
			if (IS_ERR(mk_user))
				return PTR_ERR(mk_user);
			key_put(mk_user);
			return 0;
		}
		err = add_master_key_user(mk);
		if (err)
			return err;
	}

	 
	if (!is_master_key_secret_present(&mk->mk_secret)) {
		if (!refcount_inc_not_zero(&mk->mk_active_refs))
			return KEY_DEAD;
		move_master_key_secret(&mk->mk_secret, secret);
	}

	return 0;
}

static int do_add_master_key(struct super_block *sb,
			     struct fscrypt_master_key_secret *secret,
			     const struct fscrypt_key_specifier *mk_spec)
{
	static DEFINE_MUTEX(fscrypt_add_key_mutex);
	struct fscrypt_master_key *mk;
	int err;

	mutex_lock(&fscrypt_add_key_mutex);  

	mk = fscrypt_find_master_key(sb, mk_spec);
	if (!mk) {
		 
		err = allocate_filesystem_keyring(sb);
		if (!err)
			err = add_new_master_key(sb, secret, mk_spec);
	} else {
		 
		down_write(&mk->mk_sem);
		err = add_existing_master_key(mk, secret);
		up_write(&mk->mk_sem);
		if (err == KEY_DEAD) {
			 
			err = add_new_master_key(sb, secret, mk_spec);
		}
		fscrypt_put_master_key(mk);
	}
	mutex_unlock(&fscrypt_add_key_mutex);
	return err;
}

static int add_master_key(struct super_block *sb,
			  struct fscrypt_master_key_secret *secret,
			  struct fscrypt_key_specifier *key_spec)
{
	int err;

	if (key_spec->type == FSCRYPT_KEY_SPEC_TYPE_IDENTIFIER) {
		err = fscrypt_init_hkdf(&secret->hkdf, secret->raw,
					secret->size);
		if (err)
			return err;

		 
		memzero_explicit(secret->raw, secret->size);

		 
		err = fscrypt_hkdf_expand(&secret->hkdf,
					  HKDF_CONTEXT_KEY_IDENTIFIER, NULL, 0,
					  key_spec->u.identifier,
					  FSCRYPT_KEY_IDENTIFIER_SIZE);
		if (err)
			return err;
	}
	return do_add_master_key(sb, secret, key_spec);
}

static int fscrypt_provisioning_key_preparse(struct key_preparsed_payload *prep)
{
	const struct fscrypt_provisioning_key_payload *payload = prep->data;

	if (prep->datalen < sizeof(*payload) + FSCRYPT_MIN_KEY_SIZE ||
	    prep->datalen > sizeof(*payload) + FSCRYPT_MAX_KEY_SIZE)
		return -EINVAL;

	if (payload->type != FSCRYPT_KEY_SPEC_TYPE_DESCRIPTOR &&
	    payload->type != FSCRYPT_KEY_SPEC_TYPE_IDENTIFIER)
		return -EINVAL;

	if (payload->__reserved)
		return -EINVAL;

	prep->payload.data[0] = kmemdup(payload, prep->datalen, GFP_KERNEL);
	if (!prep->payload.data[0])
		return -ENOMEM;

	prep->quotalen = prep->datalen;
	return 0;
}

static void fscrypt_provisioning_key_free_preparse(
					struct key_preparsed_payload *prep)
{
	kfree_sensitive(prep->payload.data[0]);
}

static void fscrypt_provisioning_key_describe(const struct key *key,
					      struct seq_file *m)
{
	seq_puts(m, key->description);
	if (key_is_positive(key)) {
		const struct fscrypt_provisioning_key_payload *payload =
			key->payload.data[0];

		seq_printf(m, ": %u [%u]", key->datalen, payload->type);
	}
}

static void fscrypt_provisioning_key_destroy(struct key *key)
{
	kfree_sensitive(key->payload.data[0]);
}

static struct key_type key_type_fscrypt_provisioning = {
	.name			= "fscrypt-provisioning",
	.preparse		= fscrypt_provisioning_key_preparse,
	.free_preparse		= fscrypt_provisioning_key_free_preparse,
	.instantiate		= generic_key_instantiate,
	.describe		= fscrypt_provisioning_key_describe,
	.destroy		= fscrypt_provisioning_key_destroy,
};

 
static int get_keyring_key(u32 key_id, u32 type,
			   struct fscrypt_master_key_secret *secret)
{
	key_ref_t ref;
	struct key *key;
	const struct fscrypt_provisioning_key_payload *payload;
	int err;

	ref = lookup_user_key(key_id, 0, KEY_NEED_SEARCH);
	if (IS_ERR(ref))
		return PTR_ERR(ref);
	key = key_ref_to_ptr(ref);

	if (key->type != &key_type_fscrypt_provisioning)
		goto bad_key;
	payload = key->payload.data[0];

	 
	if (payload->type != type)
		goto bad_key;

	secret->size = key->datalen - sizeof(*payload);
	memcpy(secret->raw, payload->raw, secret->size);
	err = 0;
	goto out_put;

bad_key:
	err = -EKEYREJECTED;
out_put:
	key_ref_put(ref);
	return err;
}

 
int fscrypt_ioctl_add_key(struct file *filp, void __user *_uarg)
{
	struct super_block *sb = file_inode(filp)->i_sb;
	struct fscrypt_add_key_arg __user *uarg = _uarg;
	struct fscrypt_add_key_arg arg;
	struct fscrypt_master_key_secret secret;
	int err;

	if (copy_from_user(&arg, uarg, sizeof(arg)))
		return -EFAULT;

	if (!valid_key_spec(&arg.key_spec))
		return -EINVAL;

	if (memchr_inv(arg.__reserved, 0, sizeof(arg.__reserved)))
		return -EINVAL;

	 
	if (arg.key_spec.type == FSCRYPT_KEY_SPEC_TYPE_DESCRIPTOR &&
	    !capable(CAP_SYS_ADMIN))
		return -EACCES;

	memset(&secret, 0, sizeof(secret));
	if (arg.key_id) {
		if (arg.raw_size != 0)
			return -EINVAL;
		err = get_keyring_key(arg.key_id, arg.key_spec.type, &secret);
		if (err)
			goto out_wipe_secret;
	} else {
		if (arg.raw_size < FSCRYPT_MIN_KEY_SIZE ||
		    arg.raw_size > FSCRYPT_MAX_KEY_SIZE)
			return -EINVAL;
		secret.size = arg.raw_size;
		err = -EFAULT;
		if (copy_from_user(secret.raw, uarg->raw, secret.size))
			goto out_wipe_secret;
	}

	err = add_master_key(sb, &secret, &arg.key_spec);
	if (err)
		goto out_wipe_secret;

	 
	err = -EFAULT;
	if (arg.key_spec.type == FSCRYPT_KEY_SPEC_TYPE_IDENTIFIER &&
	    copy_to_user(uarg->key_spec.u.identifier, arg.key_spec.u.identifier,
			 FSCRYPT_KEY_IDENTIFIER_SIZE))
		goto out_wipe_secret;
	err = 0;
out_wipe_secret:
	wipe_master_key_secret(&secret);
	return err;
}
EXPORT_SYMBOL_GPL(fscrypt_ioctl_add_key);

static void
fscrypt_get_test_dummy_secret(struct fscrypt_master_key_secret *secret)
{
	static u8 test_key[FSCRYPT_MAX_KEY_SIZE];

	get_random_once(test_key, FSCRYPT_MAX_KEY_SIZE);

	memset(secret, 0, sizeof(*secret));
	secret->size = FSCRYPT_MAX_KEY_SIZE;
	memcpy(secret->raw, test_key, FSCRYPT_MAX_KEY_SIZE);
}

int fscrypt_get_test_dummy_key_identifier(
				u8 key_identifier[FSCRYPT_KEY_IDENTIFIER_SIZE])
{
	struct fscrypt_master_key_secret secret;
	int err;

	fscrypt_get_test_dummy_secret(&secret);

	err = fscrypt_init_hkdf(&secret.hkdf, secret.raw, secret.size);
	if (err)
		goto out;
	err = fscrypt_hkdf_expand(&secret.hkdf, HKDF_CONTEXT_KEY_IDENTIFIER,
				  NULL, 0, key_identifier,
				  FSCRYPT_KEY_IDENTIFIER_SIZE);
out:
	wipe_master_key_secret(&secret);
	return err;
}

 
int fscrypt_add_test_dummy_key(struct super_block *sb,
			       struct fscrypt_key_specifier *key_spec)
{
	struct fscrypt_master_key_secret secret;
	int err;

	fscrypt_get_test_dummy_secret(&secret);
	err = add_master_key(sb, &secret, key_spec);
	wipe_master_key_secret(&secret);
	return err;
}

 
int fscrypt_verify_key_added(struct super_block *sb,
			     const u8 identifier[FSCRYPT_KEY_IDENTIFIER_SIZE])
{
	struct fscrypt_key_specifier mk_spec;
	struct fscrypt_master_key *mk;
	struct key *mk_user;
	int err;

	mk_spec.type = FSCRYPT_KEY_SPEC_TYPE_IDENTIFIER;
	memcpy(mk_spec.u.identifier, identifier, FSCRYPT_KEY_IDENTIFIER_SIZE);

	mk = fscrypt_find_master_key(sb, &mk_spec);
	if (!mk) {
		err = -ENOKEY;
		goto out;
	}
	down_read(&mk->mk_sem);
	mk_user = find_master_key_user(mk);
	if (IS_ERR(mk_user)) {
		err = PTR_ERR(mk_user);
	} else {
		key_put(mk_user);
		err = 0;
	}
	up_read(&mk->mk_sem);
	fscrypt_put_master_key(mk);
out:
	if (err == -ENOKEY && capable(CAP_FOWNER))
		err = 0;
	return err;
}

 
static void shrink_dcache_inode(struct inode *inode)
{
	struct dentry *dentry;

	if (S_ISDIR(inode->i_mode)) {
		dentry = d_find_any_alias(inode);
		if (dentry) {
			shrink_dcache_parent(dentry);
			dput(dentry);
		}
	}
	d_prune_aliases(inode);
}

static void evict_dentries_for_decrypted_inodes(struct fscrypt_master_key *mk)
{
	struct fscrypt_info *ci;
	struct inode *inode;
	struct inode *toput_inode = NULL;

	spin_lock(&mk->mk_decrypted_inodes_lock);

	list_for_each_entry(ci, &mk->mk_decrypted_inodes, ci_master_key_link) {
		inode = ci->ci_inode;
		spin_lock(&inode->i_lock);
		if (inode->i_state & (I_FREEING | I_WILL_FREE | I_NEW)) {
			spin_unlock(&inode->i_lock);
			continue;
		}
		__iget(inode);
		spin_unlock(&inode->i_lock);
		spin_unlock(&mk->mk_decrypted_inodes_lock);

		shrink_dcache_inode(inode);
		iput(toput_inode);
		toput_inode = inode;

		spin_lock(&mk->mk_decrypted_inodes_lock);
	}

	spin_unlock(&mk->mk_decrypted_inodes_lock);
	iput(toput_inode);
}

static int check_for_busy_inodes(struct super_block *sb,
				 struct fscrypt_master_key *mk)
{
	struct list_head *pos;
	size_t busy_count = 0;
	unsigned long ino;
	char ino_str[50] = "";

	spin_lock(&mk->mk_decrypted_inodes_lock);

	list_for_each(pos, &mk->mk_decrypted_inodes)
		busy_count++;

	if (busy_count == 0) {
		spin_unlock(&mk->mk_decrypted_inodes_lock);
		return 0;
	}

	{
		 
		struct inode *inode =
			list_first_entry(&mk->mk_decrypted_inodes,
					 struct fscrypt_info,
					 ci_master_key_link)->ci_inode;
		ino = inode->i_ino;
	}
	spin_unlock(&mk->mk_decrypted_inodes_lock);

	 
	if (ino)
		snprintf(ino_str, sizeof(ino_str), ", including ino %lu", ino);

	fscrypt_warn(NULL,
		     "%s: %zu inode(s) still busy after removing key with %s %*phN%s",
		     sb->s_id, busy_count, master_key_spec_type(&mk->mk_spec),
		     master_key_spec_len(&mk->mk_spec), (u8 *)&mk->mk_spec.u,
		     ino_str);
	return -EBUSY;
}

static int try_to_lock_encrypted_files(struct super_block *sb,
				       struct fscrypt_master_key *mk)
{
	int err1;
	int err2;

	 
	down_read(&sb->s_umount);
	err1 = sync_filesystem(sb);
	up_read(&sb->s_umount);
	 

	 
	evict_dentries_for_decrypted_inodes(mk);

	 
	err2 = check_for_busy_inodes(sb, mk);

	return err1 ?: err2;
}

 
static int do_remove_key(struct file *filp, void __user *_uarg, bool all_users)
{
	struct super_block *sb = file_inode(filp)->i_sb;
	struct fscrypt_remove_key_arg __user *uarg = _uarg;
	struct fscrypt_remove_key_arg arg;
	struct fscrypt_master_key *mk;
	u32 status_flags = 0;
	int err;
	bool inodes_remain;

	if (copy_from_user(&arg, uarg, sizeof(arg)))
		return -EFAULT;

	if (!valid_key_spec(&arg.key_spec))
		return -EINVAL;

	if (memchr_inv(arg.__reserved, 0, sizeof(arg.__reserved)))
		return -EINVAL;

	 
	if (arg.key_spec.type == FSCRYPT_KEY_SPEC_TYPE_DESCRIPTOR &&
	    !capable(CAP_SYS_ADMIN))
		return -EACCES;

	 
	mk = fscrypt_find_master_key(sb, &arg.key_spec);
	if (!mk)
		return -ENOKEY;
	down_write(&mk->mk_sem);

	 
	if (mk->mk_users && mk->mk_users->keys.nr_leaves_on_tree != 0) {
		if (all_users)
			err = keyring_clear(mk->mk_users);
		else
			err = remove_master_key_user(mk);
		if (err) {
			up_write(&mk->mk_sem);
			goto out_put_key;
		}
		if (mk->mk_users->keys.nr_leaves_on_tree != 0) {
			 
			status_flags |=
				FSCRYPT_KEY_REMOVAL_STATUS_FLAG_OTHER_USERS;
			err = 0;
			up_write(&mk->mk_sem);
			goto out_put_key;
		}
	}

	 
	err = -ENOKEY;
	if (is_master_key_secret_present(&mk->mk_secret)) {
		wipe_master_key_secret(&mk->mk_secret);
		fscrypt_put_master_key_activeref(sb, mk);
		err = 0;
	}
	inodes_remain = refcount_read(&mk->mk_active_refs) > 0;
	up_write(&mk->mk_sem);

	if (inodes_remain) {
		 
		err = try_to_lock_encrypted_files(sb, mk);
		if (err == -EBUSY) {
			status_flags |=
				FSCRYPT_KEY_REMOVAL_STATUS_FLAG_FILES_BUSY;
			err = 0;
		}
	}
	 
out_put_key:
	fscrypt_put_master_key(mk);
	if (err == 0)
		err = put_user(status_flags, &uarg->removal_status_flags);
	return err;
}

int fscrypt_ioctl_remove_key(struct file *filp, void __user *uarg)
{
	return do_remove_key(filp, uarg, false);
}
EXPORT_SYMBOL_GPL(fscrypt_ioctl_remove_key);

int fscrypt_ioctl_remove_key_all_users(struct file *filp, void __user *uarg)
{
	if (!capable(CAP_SYS_ADMIN))
		return -EACCES;
	return do_remove_key(filp, uarg, true);
}
EXPORT_SYMBOL_GPL(fscrypt_ioctl_remove_key_all_users);

 
int fscrypt_ioctl_get_key_status(struct file *filp, void __user *uarg)
{
	struct super_block *sb = file_inode(filp)->i_sb;
	struct fscrypt_get_key_status_arg arg;
	struct fscrypt_master_key *mk;
	int err;

	if (copy_from_user(&arg, uarg, sizeof(arg)))
		return -EFAULT;

	if (!valid_key_spec(&arg.key_spec))
		return -EINVAL;

	if (memchr_inv(arg.__reserved, 0, sizeof(arg.__reserved)))
		return -EINVAL;

	arg.status_flags = 0;
	arg.user_count = 0;
	memset(arg.__out_reserved, 0, sizeof(arg.__out_reserved));

	mk = fscrypt_find_master_key(sb, &arg.key_spec);
	if (!mk) {
		arg.status = FSCRYPT_KEY_STATUS_ABSENT;
		err = 0;
		goto out;
	}
	down_read(&mk->mk_sem);

	if (!is_master_key_secret_present(&mk->mk_secret)) {
		arg.status = refcount_read(&mk->mk_active_refs) > 0 ?
			FSCRYPT_KEY_STATUS_INCOMPLETELY_REMOVED :
			FSCRYPT_KEY_STATUS_ABSENT  ;
		err = 0;
		goto out_release_key;
	}

	arg.status = FSCRYPT_KEY_STATUS_PRESENT;
	if (mk->mk_users) {
		struct key *mk_user;

		arg.user_count = mk->mk_users->keys.nr_leaves_on_tree;
		mk_user = find_master_key_user(mk);
		if (!IS_ERR(mk_user)) {
			arg.status_flags |=
				FSCRYPT_KEY_STATUS_FLAG_ADDED_BY_SELF;
			key_put(mk_user);
		} else if (mk_user != ERR_PTR(-ENOKEY)) {
			err = PTR_ERR(mk_user);
			goto out_release_key;
		}
	}
	err = 0;
out_release_key:
	up_read(&mk->mk_sem);
	fscrypt_put_master_key(mk);
out:
	if (!err && copy_to_user(uarg, &arg, sizeof(arg)))
		err = -EFAULT;
	return err;
}
EXPORT_SYMBOL_GPL(fscrypt_ioctl_get_key_status);

int __init fscrypt_init_keyring(void)
{
	int err;

	err = register_key_type(&key_type_fscrypt_user);
	if (err)
		return err;

	err = register_key_type(&key_type_fscrypt_provisioning);
	if (err)
		goto err_unregister_fscrypt_user;

	return 0;

err_unregister_fscrypt_user:
	unregister_key_type(&key_type_fscrypt_user);
	return err;
}
