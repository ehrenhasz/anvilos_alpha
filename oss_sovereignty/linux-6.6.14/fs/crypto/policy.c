
 

#include <linux/fs_context.h>
#include <linux/random.h>
#include <linux/seq_file.h>
#include <linux/string.h>
#include <linux/mount.h>
#include "fscrypt_private.h"

 
bool fscrypt_policies_equal(const union fscrypt_policy *policy1,
			    const union fscrypt_policy *policy2)
{
	if (policy1->version != policy2->version)
		return false;

	return !memcmp(policy1, policy2, fscrypt_policy_size(policy1));
}

int fscrypt_policy_to_key_spec(const union fscrypt_policy *policy,
			       struct fscrypt_key_specifier *key_spec)
{
	switch (policy->version) {
	case FSCRYPT_POLICY_V1:
		key_spec->type = FSCRYPT_KEY_SPEC_TYPE_DESCRIPTOR;
		memcpy(key_spec->u.descriptor, policy->v1.master_key_descriptor,
		       FSCRYPT_KEY_DESCRIPTOR_SIZE);
		return 0;
	case FSCRYPT_POLICY_V2:
		key_spec->type = FSCRYPT_KEY_SPEC_TYPE_IDENTIFIER;
		memcpy(key_spec->u.identifier, policy->v2.master_key_identifier,
		       FSCRYPT_KEY_IDENTIFIER_SIZE);
		return 0;
	default:
		WARN_ON_ONCE(1);
		return -EINVAL;
	}
}

const union fscrypt_policy *fscrypt_get_dummy_policy(struct super_block *sb)
{
	if (!sb->s_cop->get_dummy_policy)
		return NULL;
	return sb->s_cop->get_dummy_policy(sb);
}

 
static bool fscrypt_valid_enc_modes_v1(u32 contents_mode, u32 filenames_mode)
{
	if (contents_mode == FSCRYPT_MODE_AES_256_XTS &&
	    filenames_mode == FSCRYPT_MODE_AES_256_CTS)
		return true;

	if (contents_mode == FSCRYPT_MODE_AES_128_CBC &&
	    filenames_mode == FSCRYPT_MODE_AES_128_CTS)
		return true;

	if (contents_mode == FSCRYPT_MODE_ADIANTUM &&
	    filenames_mode == FSCRYPT_MODE_ADIANTUM)
		return true;

	return false;
}

static bool fscrypt_valid_enc_modes_v2(u32 contents_mode, u32 filenames_mode)
{
	if (contents_mode == FSCRYPT_MODE_AES_256_XTS &&
	    filenames_mode == FSCRYPT_MODE_AES_256_HCTR2)
		return true;

	if (contents_mode == FSCRYPT_MODE_SM4_XTS &&
	    filenames_mode == FSCRYPT_MODE_SM4_CTS)
		return true;

	return fscrypt_valid_enc_modes_v1(contents_mode, filenames_mode);
}

static bool supported_direct_key_modes(const struct inode *inode,
				       u32 contents_mode, u32 filenames_mode)
{
	const struct fscrypt_mode *mode;

	if (contents_mode != filenames_mode) {
		fscrypt_warn(inode,
			     "Direct key flag not allowed with different contents and filenames modes");
		return false;
	}
	mode = &fscrypt_modes[contents_mode];

	if (mode->ivsize < offsetofend(union fscrypt_iv, nonce)) {
		fscrypt_warn(inode, "Direct key flag not allowed with %s",
			     mode->friendly_name);
		return false;
	}
	return true;
}

static bool supported_iv_ino_lblk_policy(const struct fscrypt_policy_v2 *policy,
					 const struct inode *inode,
					 const char *type,
					 int max_ino_bits, int max_lblk_bits)
{
	struct super_block *sb = inode->i_sb;
	int ino_bits = 64, lblk_bits = 64;

	 
	if (policy->contents_encryption_mode != FSCRYPT_MODE_AES_256_XTS) {
		fscrypt_warn(inode,
			     "Can't use %s policy with contents mode other than AES-256-XTS",
			     type);
		return false;
	}

	 
	if (!sb->s_cop->has_stable_inodes ||
	    !sb->s_cop->has_stable_inodes(sb)) {
		fscrypt_warn(inode,
			     "Can't use %s policy on filesystem '%s' because it doesn't have stable inode numbers",
			     type, sb->s_id);
		return false;
	}
	if (sb->s_cop->get_ino_and_lblk_bits)
		sb->s_cop->get_ino_and_lblk_bits(sb, &ino_bits, &lblk_bits);
	if (ino_bits > max_ino_bits) {
		fscrypt_warn(inode,
			     "Can't use %s policy on filesystem '%s' because its inode numbers are too long",
			     type, sb->s_id);
		return false;
	}
	if (lblk_bits > max_lblk_bits) {
		fscrypt_warn(inode,
			     "Can't use %s policy on filesystem '%s' because its block numbers are too long",
			     type, sb->s_id);
		return false;
	}
	return true;
}

static bool fscrypt_supported_v1_policy(const struct fscrypt_policy_v1 *policy,
					const struct inode *inode)
{
	if (!fscrypt_valid_enc_modes_v1(policy->contents_encryption_mode,
				     policy->filenames_encryption_mode)) {
		fscrypt_warn(inode,
			     "Unsupported encryption modes (contents %d, filenames %d)",
			     policy->contents_encryption_mode,
			     policy->filenames_encryption_mode);
		return false;
	}

	if (policy->flags & ~(FSCRYPT_POLICY_FLAGS_PAD_MASK |
			      FSCRYPT_POLICY_FLAG_DIRECT_KEY)) {
		fscrypt_warn(inode, "Unsupported encryption flags (0x%02x)",
			     policy->flags);
		return false;
	}

	if ((policy->flags & FSCRYPT_POLICY_FLAG_DIRECT_KEY) &&
	    !supported_direct_key_modes(inode, policy->contents_encryption_mode,
					policy->filenames_encryption_mode))
		return false;

	if (IS_CASEFOLDED(inode)) {
		 
		fscrypt_warn(inode,
			     "v1 policies can't be used on casefolded directories");
		return false;
	}

	return true;
}

static bool fscrypt_supported_v2_policy(const struct fscrypt_policy_v2 *policy,
					const struct inode *inode)
{
	int count = 0;

	if (!fscrypt_valid_enc_modes_v2(policy->contents_encryption_mode,
				     policy->filenames_encryption_mode)) {
		fscrypt_warn(inode,
			     "Unsupported encryption modes (contents %d, filenames %d)",
			     policy->contents_encryption_mode,
			     policy->filenames_encryption_mode);
		return false;
	}

	if (policy->flags & ~(FSCRYPT_POLICY_FLAGS_PAD_MASK |
			      FSCRYPT_POLICY_FLAG_DIRECT_KEY |
			      FSCRYPT_POLICY_FLAG_IV_INO_LBLK_64 |
			      FSCRYPT_POLICY_FLAG_IV_INO_LBLK_32)) {
		fscrypt_warn(inode, "Unsupported encryption flags (0x%02x)",
			     policy->flags);
		return false;
	}

	count += !!(policy->flags & FSCRYPT_POLICY_FLAG_DIRECT_KEY);
	count += !!(policy->flags & FSCRYPT_POLICY_FLAG_IV_INO_LBLK_64);
	count += !!(policy->flags & FSCRYPT_POLICY_FLAG_IV_INO_LBLK_32);
	if (count > 1) {
		fscrypt_warn(inode, "Mutually exclusive encryption flags (0x%02x)",
			     policy->flags);
		return false;
	}

	if ((policy->flags & FSCRYPT_POLICY_FLAG_DIRECT_KEY) &&
	    !supported_direct_key_modes(inode, policy->contents_encryption_mode,
					policy->filenames_encryption_mode))
		return false;

	if ((policy->flags & FSCRYPT_POLICY_FLAG_IV_INO_LBLK_64) &&
	    !supported_iv_ino_lblk_policy(policy, inode, "IV_INO_LBLK_64",
					  32, 32))
		return false;

	 
	if ((policy->flags & FSCRYPT_POLICY_FLAG_IV_INO_LBLK_32) &&
	    !supported_iv_ino_lblk_policy(policy, inode, "IV_INO_LBLK_32",
					  32, 32))
		return false;

	if (memchr_inv(policy->__reserved, 0, sizeof(policy->__reserved))) {
		fscrypt_warn(inode, "Reserved bits set in encryption policy");
		return false;
	}

	return true;
}

 
bool fscrypt_supported_policy(const union fscrypt_policy *policy_u,
			      const struct inode *inode)
{
	switch (policy_u->version) {
	case FSCRYPT_POLICY_V1:
		return fscrypt_supported_v1_policy(&policy_u->v1, inode);
	case FSCRYPT_POLICY_V2:
		return fscrypt_supported_v2_policy(&policy_u->v2, inode);
	}
	return false;
}

 
static int fscrypt_new_context(union fscrypt_context *ctx_u,
			       const union fscrypt_policy *policy_u,
			       const u8 nonce[FSCRYPT_FILE_NONCE_SIZE])
{
	memset(ctx_u, 0, sizeof(*ctx_u));

	switch (policy_u->version) {
	case FSCRYPT_POLICY_V1: {
		const struct fscrypt_policy_v1 *policy = &policy_u->v1;
		struct fscrypt_context_v1 *ctx = &ctx_u->v1;

		ctx->version = FSCRYPT_CONTEXT_V1;
		ctx->contents_encryption_mode =
			policy->contents_encryption_mode;
		ctx->filenames_encryption_mode =
			policy->filenames_encryption_mode;
		ctx->flags = policy->flags;
		memcpy(ctx->master_key_descriptor,
		       policy->master_key_descriptor,
		       sizeof(ctx->master_key_descriptor));
		memcpy(ctx->nonce, nonce, FSCRYPT_FILE_NONCE_SIZE);
		return sizeof(*ctx);
	}
	case FSCRYPT_POLICY_V2: {
		const struct fscrypt_policy_v2 *policy = &policy_u->v2;
		struct fscrypt_context_v2 *ctx = &ctx_u->v2;

		ctx->version = FSCRYPT_CONTEXT_V2;
		ctx->contents_encryption_mode =
			policy->contents_encryption_mode;
		ctx->filenames_encryption_mode =
			policy->filenames_encryption_mode;
		ctx->flags = policy->flags;
		memcpy(ctx->master_key_identifier,
		       policy->master_key_identifier,
		       sizeof(ctx->master_key_identifier));
		memcpy(ctx->nonce, nonce, FSCRYPT_FILE_NONCE_SIZE);
		return sizeof(*ctx);
	}
	}
	BUG();
}

 
int fscrypt_policy_from_context(union fscrypt_policy *policy_u,
				const union fscrypt_context *ctx_u,
				int ctx_size)
{
	memset(policy_u, 0, sizeof(*policy_u));

	if (!fscrypt_context_is_valid(ctx_u, ctx_size))
		return -EINVAL;

	switch (ctx_u->version) {
	case FSCRYPT_CONTEXT_V1: {
		const struct fscrypt_context_v1 *ctx = &ctx_u->v1;
		struct fscrypt_policy_v1 *policy = &policy_u->v1;

		policy->version = FSCRYPT_POLICY_V1;
		policy->contents_encryption_mode =
			ctx->contents_encryption_mode;
		policy->filenames_encryption_mode =
			ctx->filenames_encryption_mode;
		policy->flags = ctx->flags;
		memcpy(policy->master_key_descriptor,
		       ctx->master_key_descriptor,
		       sizeof(policy->master_key_descriptor));
		return 0;
	}
	case FSCRYPT_CONTEXT_V2: {
		const struct fscrypt_context_v2 *ctx = &ctx_u->v2;
		struct fscrypt_policy_v2 *policy = &policy_u->v2;

		policy->version = FSCRYPT_POLICY_V2;
		policy->contents_encryption_mode =
			ctx->contents_encryption_mode;
		policy->filenames_encryption_mode =
			ctx->filenames_encryption_mode;
		policy->flags = ctx->flags;
		memcpy(policy->__reserved, ctx->__reserved,
		       sizeof(policy->__reserved));
		memcpy(policy->master_key_identifier,
		       ctx->master_key_identifier,
		       sizeof(policy->master_key_identifier));
		return 0;
	}
	}
	 
	return -EINVAL;
}

 
static int fscrypt_get_policy(struct inode *inode, union fscrypt_policy *policy)
{
	const struct fscrypt_info *ci;
	union fscrypt_context ctx;
	int ret;

	ci = fscrypt_get_info(inode);
	if (ci) {
		 
		*policy = ci->ci_policy;
		return 0;
	}

	if (!IS_ENCRYPTED(inode))
		return -ENODATA;

	ret = inode->i_sb->s_cop->get_context(inode, &ctx, sizeof(ctx));
	if (ret < 0)
		return (ret == -ERANGE) ? -EINVAL : ret;

	return fscrypt_policy_from_context(policy, &ctx, ret);
}

static int set_encryption_policy(struct inode *inode,
				 const union fscrypt_policy *policy)
{
	u8 nonce[FSCRYPT_FILE_NONCE_SIZE];
	union fscrypt_context ctx;
	int ctxsize;
	int err;

	if (!fscrypt_supported_policy(policy, inode))
		return -EINVAL;

	switch (policy->version) {
	case FSCRYPT_POLICY_V1:
		 
		pr_warn_once("%s (pid %d) is setting deprecated v1 encryption policy; recommend upgrading to v2.\n",
			     current->comm, current->pid);
		break;
	case FSCRYPT_POLICY_V2:
		err = fscrypt_verify_key_added(inode->i_sb,
					       policy->v2.master_key_identifier);
		if (err)
			return err;
		if (policy->v2.flags & FSCRYPT_POLICY_FLAG_IV_INO_LBLK_32)
			pr_warn_once("%s (pid %d) is setting an IV_INO_LBLK_32 encryption policy.  This should only be used if there are certain hardware limitations.\n",
				     current->comm, current->pid);
		break;
	default:
		WARN_ON_ONCE(1);
		return -EINVAL;
	}

	get_random_bytes(nonce, FSCRYPT_FILE_NONCE_SIZE);
	ctxsize = fscrypt_new_context(&ctx, policy, nonce);

	return inode->i_sb->s_cop->set_context(inode, &ctx, ctxsize, NULL);
}

int fscrypt_ioctl_set_policy(struct file *filp, const void __user *arg)
{
	union fscrypt_policy policy;
	union fscrypt_policy existing_policy;
	struct inode *inode = file_inode(filp);
	u8 version;
	int size;
	int ret;

	if (get_user(policy.version, (const u8 __user *)arg))
		return -EFAULT;

	size = fscrypt_policy_size(&policy);
	if (size <= 0)
		return -EINVAL;

	 
	version = policy.version;
	if (copy_from_user(&policy, arg, size))
		return -EFAULT;
	policy.version = version;

	if (!inode_owner_or_capable(&nop_mnt_idmap, inode))
		return -EACCES;

	ret = mnt_want_write_file(filp);
	if (ret)
		return ret;

	inode_lock(inode);

	ret = fscrypt_get_policy(inode, &existing_policy);
	if (ret == -ENODATA) {
		if (!S_ISDIR(inode->i_mode))
			ret = -ENOTDIR;
		else if (IS_DEADDIR(inode))
			ret = -ENOENT;
		else if (!inode->i_sb->s_cop->empty_dir(inode))
			ret = -ENOTEMPTY;
		else
			ret = set_encryption_policy(inode, &policy);
	} else if (ret == -EINVAL ||
		   (ret == 0 && !fscrypt_policies_equal(&policy,
							&existing_policy))) {
		 
		ret = -EEXIST;
	}

	inode_unlock(inode);

	mnt_drop_write_file(filp);
	return ret;
}
EXPORT_SYMBOL(fscrypt_ioctl_set_policy);

 
int fscrypt_ioctl_get_policy(struct file *filp, void __user *arg)
{
	union fscrypt_policy policy;
	int err;

	err = fscrypt_get_policy(file_inode(filp), &policy);
	if (err)
		return err;

	if (policy.version != FSCRYPT_POLICY_V1)
		return -EINVAL;

	if (copy_to_user(arg, &policy, sizeof(policy.v1)))
		return -EFAULT;
	return 0;
}
EXPORT_SYMBOL(fscrypt_ioctl_get_policy);

 
int fscrypt_ioctl_get_policy_ex(struct file *filp, void __user *uarg)
{
	struct fscrypt_get_policy_ex_arg arg;
	union fscrypt_policy *policy = (union fscrypt_policy *)&arg.policy;
	size_t policy_size;
	int err;

	 
	BUILD_BUG_ON(offsetof(typeof(arg), policy_size) != 0);
	BUILD_BUG_ON(offsetofend(typeof(arg), policy_size) !=
		     offsetof(typeof(arg), policy));
	BUILD_BUG_ON(sizeof(arg.policy) != sizeof(*policy));

	err = fscrypt_get_policy(file_inode(filp), policy);
	if (err)
		return err;
	policy_size = fscrypt_policy_size(policy);

	if (copy_from_user(&arg, uarg, sizeof(arg.policy_size)))
		return -EFAULT;

	if (policy_size > arg.policy_size)
		return -EOVERFLOW;
	arg.policy_size = policy_size;

	if (copy_to_user(uarg, &arg, sizeof(arg.policy_size) + policy_size))
		return -EFAULT;
	return 0;
}
EXPORT_SYMBOL_GPL(fscrypt_ioctl_get_policy_ex);

 
int fscrypt_ioctl_get_nonce(struct file *filp, void __user *arg)
{
	struct inode *inode = file_inode(filp);
	union fscrypt_context ctx;
	int ret;

	ret = inode->i_sb->s_cop->get_context(inode, &ctx, sizeof(ctx));
	if (ret < 0)
		return ret;
	if (!fscrypt_context_is_valid(&ctx, ret))
		return -EINVAL;
	if (copy_to_user(arg, fscrypt_context_nonce(&ctx),
			 FSCRYPT_FILE_NONCE_SIZE))
		return -EFAULT;
	return 0;
}
EXPORT_SYMBOL_GPL(fscrypt_ioctl_get_nonce);

 
int fscrypt_has_permitted_context(struct inode *parent, struct inode *child)
{
	union fscrypt_policy parent_policy, child_policy;
	int err, err1, err2;

	 
	if (!S_ISREG(child->i_mode) && !S_ISDIR(child->i_mode) &&
	    !S_ISLNK(child->i_mode))
		return 1;

	 
	if (!IS_ENCRYPTED(parent))
		return 1;

	 
	if (!IS_ENCRYPTED(child))
		return 0;

	 

	err = fscrypt_get_encryption_info(parent, true);
	if (err)
		return 0;
	err = fscrypt_get_encryption_info(child, true);
	if (err)
		return 0;

	err1 = fscrypt_get_policy(parent, &parent_policy);
	err2 = fscrypt_get_policy(child, &child_policy);

	 
	if (err1 == -EINVAL && err2 == -EINVAL)
		return 1;

	if (err1 || err2)
		return 0;

	return fscrypt_policies_equal(&parent_policy, &child_policy);
}
EXPORT_SYMBOL(fscrypt_has_permitted_context);

 
const union fscrypt_policy *fscrypt_policy_to_inherit(struct inode *dir)
{
	int err;

	if (IS_ENCRYPTED(dir)) {
		err = fscrypt_require_key(dir);
		if (err)
			return ERR_PTR(err);
		return &dir->i_crypt_info->ci_policy;
	}

	return fscrypt_get_dummy_policy(dir->i_sb);
}

 
int fscrypt_context_for_new_inode(void *ctx, struct inode *inode)
{
	struct fscrypt_info *ci = inode->i_crypt_info;

	BUILD_BUG_ON(sizeof(union fscrypt_context) !=
			FSCRYPT_SET_CONTEXT_MAX_SIZE);

	 
	if (WARN_ON_ONCE(!ci))
		return -ENOKEY;

	return fscrypt_new_context(ctx, &ci->ci_policy, ci->ci_nonce);
}
EXPORT_SYMBOL_GPL(fscrypt_context_for_new_inode);

 
int fscrypt_set_context(struct inode *inode, void *fs_data)
{
	struct fscrypt_info *ci = inode->i_crypt_info;
	union fscrypt_context ctx;
	int ctxsize;

	ctxsize = fscrypt_context_for_new_inode(&ctx, inode);
	if (ctxsize < 0)
		return ctxsize;

	 
	if (ci->ci_policy.version == FSCRYPT_POLICY_V2 &&
	    (ci->ci_policy.v2.flags & FSCRYPT_POLICY_FLAG_IV_INO_LBLK_32))
		fscrypt_hash_inode_number(ci, ci->ci_master_key);

	return inode->i_sb->s_cop->set_context(inode, &ctx, ctxsize, fs_data);
}
EXPORT_SYMBOL_GPL(fscrypt_set_context);

 
int fscrypt_parse_test_dummy_encryption(const struct fs_parameter *param,
				struct fscrypt_dummy_policy *dummy_policy)
{
	const char *arg = "v2";
	union fscrypt_policy *policy;
	int err;

	if (param->type == fs_value_is_string && *param->string)
		arg = param->string;

	policy = kzalloc(sizeof(*policy), GFP_KERNEL);
	if (!policy)
		return -ENOMEM;

	if (!strcmp(arg, "v1")) {
		policy->version = FSCRYPT_POLICY_V1;
		policy->v1.contents_encryption_mode = FSCRYPT_MODE_AES_256_XTS;
		policy->v1.filenames_encryption_mode = FSCRYPT_MODE_AES_256_CTS;
		memset(policy->v1.master_key_descriptor, 0x42,
		       FSCRYPT_KEY_DESCRIPTOR_SIZE);
	} else if (!strcmp(arg, "v2")) {
		policy->version = FSCRYPT_POLICY_V2;
		policy->v2.contents_encryption_mode = FSCRYPT_MODE_AES_256_XTS;
		policy->v2.filenames_encryption_mode = FSCRYPT_MODE_AES_256_CTS;
		err = fscrypt_get_test_dummy_key_identifier(
				policy->v2.master_key_identifier);
		if (err)
			goto out;
	} else {
		err = -EINVAL;
		goto out;
	}

	if (dummy_policy->policy) {
		if (fscrypt_policies_equal(policy, dummy_policy->policy))
			err = 0;
		else
			err = -EEXIST;
		goto out;
	}
	dummy_policy->policy = policy;
	policy = NULL;
	err = 0;
out:
	kfree(policy);
	return err;
}
EXPORT_SYMBOL_GPL(fscrypt_parse_test_dummy_encryption);

 
bool fscrypt_dummy_policies_equal(const struct fscrypt_dummy_policy *p1,
				  const struct fscrypt_dummy_policy *p2)
{
	if (!p1->policy && !p2->policy)
		return true;
	if (!p1->policy || !p2->policy)
		return false;
	return fscrypt_policies_equal(p1->policy, p2->policy);
}
EXPORT_SYMBOL_GPL(fscrypt_dummy_policies_equal);

 
void fscrypt_show_test_dummy_encryption(struct seq_file *seq, char sep,
					struct super_block *sb)
{
	const union fscrypt_policy *policy = fscrypt_get_dummy_policy(sb);
	int vers;

	if (!policy)
		return;

	vers = policy->version;
	if (vers == FSCRYPT_POLICY_V1)  
		vers = 1;

	seq_printf(seq, "%ctest_dummy_encryption=v%d", sep, vers);
}
EXPORT_SYMBOL_GPL(fscrypt_show_test_dummy_encryption);
