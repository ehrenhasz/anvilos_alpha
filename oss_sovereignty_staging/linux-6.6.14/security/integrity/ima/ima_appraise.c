
 
#include <linux/module.h>
#include <linux/init.h>
#include <linux/file.h>
#include <linux/fs.h>
#include <linux/xattr.h>
#include <linux/magic.h>
#include <linux/ima.h>
#include <linux/evm.h>
#include <linux/fsverity.h>
#include <keys/system_keyring.h>
#include <uapi/linux/fsverity.h>

#include "ima.h"

#ifdef CONFIG_IMA_APPRAISE_BOOTPARAM
static char *ima_appraise_cmdline_default __initdata;
core_param(ima_appraise, ima_appraise_cmdline_default, charp, 0);

void __init ima_appraise_parse_cmdline(void)
{
	const char *str = ima_appraise_cmdline_default;
	bool sb_state = arch_ima_get_secureboot();
	int appraisal_state = ima_appraise;

	if (!str)
		return;

	if (strncmp(str, "off", 3) == 0)
		appraisal_state = 0;
	else if (strncmp(str, "log", 3) == 0)
		appraisal_state = IMA_APPRAISE_LOG;
	else if (strncmp(str, "fix", 3) == 0)
		appraisal_state = IMA_APPRAISE_FIX;
	else if (strncmp(str, "enforce", 7) == 0)
		appraisal_state = IMA_APPRAISE_ENFORCE;
	else
		pr_err("invalid \"%s\" appraise option", str);

	 
	if (sb_state) {
		if (!(appraisal_state & IMA_APPRAISE_ENFORCE))
			pr_info("Secure boot enabled: ignoring ima_appraise=%s option",
				str);
	} else {
		ima_appraise = appraisal_state;
	}
}
#endif

 
bool is_ima_appraise_enabled(void)
{
	return ima_appraise & IMA_APPRAISE_ENFORCE;
}

 
int ima_must_appraise(struct mnt_idmap *idmap, struct inode *inode,
		      int mask, enum ima_hooks func)
{
	u32 secid;

	if (!ima_appraise)
		return 0;

	security_current_getsecid_subj(&secid);
	return ima_match_policy(idmap, inode, current_cred(), secid,
				func, mask, IMA_APPRAISE | IMA_HASH, NULL,
				NULL, NULL, NULL);
}

static int ima_fix_xattr(struct dentry *dentry,
			 struct integrity_iint_cache *iint)
{
	int rc, offset;
	u8 algo = iint->ima_hash->algo;

	if (algo <= HASH_ALGO_SHA1) {
		offset = 1;
		iint->ima_hash->xattr.sha1.type = IMA_XATTR_DIGEST;
	} else {
		offset = 0;
		iint->ima_hash->xattr.ng.type = IMA_XATTR_DIGEST_NG;
		iint->ima_hash->xattr.ng.algo = algo;
	}
	rc = __vfs_setxattr_noperm(&nop_mnt_idmap, dentry, XATTR_NAME_IMA,
				   &iint->ima_hash->xattr.data[offset],
				   (sizeof(iint->ima_hash->xattr) - offset) +
				   iint->ima_hash->length, 0);
	return rc;
}

 
enum integrity_status ima_get_cache_status(struct integrity_iint_cache *iint,
					   enum ima_hooks func)
{
	switch (func) {
	case MMAP_CHECK:
	case MMAP_CHECK_REQPROT:
		return iint->ima_mmap_status;
	case BPRM_CHECK:
		return iint->ima_bprm_status;
	case CREDS_CHECK:
		return iint->ima_creds_status;
	case FILE_CHECK:
	case POST_SETATTR:
		return iint->ima_file_status;
	case MODULE_CHECK ... MAX_CHECK - 1:
	default:
		return iint->ima_read_status;
	}
}

static void ima_set_cache_status(struct integrity_iint_cache *iint,
				 enum ima_hooks func,
				 enum integrity_status status)
{
	switch (func) {
	case MMAP_CHECK:
	case MMAP_CHECK_REQPROT:
		iint->ima_mmap_status = status;
		break;
	case BPRM_CHECK:
		iint->ima_bprm_status = status;
		break;
	case CREDS_CHECK:
		iint->ima_creds_status = status;
		break;
	case FILE_CHECK:
	case POST_SETATTR:
		iint->ima_file_status = status;
		break;
	case MODULE_CHECK ... MAX_CHECK - 1:
	default:
		iint->ima_read_status = status;
		break;
	}
}

static void ima_cache_flags(struct integrity_iint_cache *iint,
			     enum ima_hooks func)
{
	switch (func) {
	case MMAP_CHECK:
	case MMAP_CHECK_REQPROT:
		iint->flags |= (IMA_MMAP_APPRAISED | IMA_APPRAISED);
		break;
	case BPRM_CHECK:
		iint->flags |= (IMA_BPRM_APPRAISED | IMA_APPRAISED);
		break;
	case CREDS_CHECK:
		iint->flags |= (IMA_CREDS_APPRAISED | IMA_APPRAISED);
		break;
	case FILE_CHECK:
	case POST_SETATTR:
		iint->flags |= (IMA_FILE_APPRAISED | IMA_APPRAISED);
		break;
	case MODULE_CHECK ... MAX_CHECK - 1:
	default:
		iint->flags |= (IMA_READ_APPRAISED | IMA_APPRAISED);
		break;
	}
}

enum hash_algo ima_get_hash_algo(const struct evm_ima_xattr_data *xattr_value,
				 int xattr_len)
{
	struct signature_v2_hdr *sig;
	enum hash_algo ret;

	if (!xattr_value || xattr_len < 2)
		 
		return ima_hash_algo;

	switch (xattr_value->type) {
	case IMA_VERITY_DIGSIG:
		sig = (typeof(sig))xattr_value;
		if (sig->version != 3 || xattr_len <= sizeof(*sig) ||
		    sig->hash_algo >= HASH_ALGO__LAST)
			return ima_hash_algo;
		return sig->hash_algo;
	case EVM_IMA_XATTR_DIGSIG:
		sig = (typeof(sig))xattr_value;
		if (sig->version != 2 || xattr_len <= sizeof(*sig)
		    || sig->hash_algo >= HASH_ALGO__LAST)
			return ima_hash_algo;
		return sig->hash_algo;
	case IMA_XATTR_DIGEST_NG:
		 
		ret = xattr_value->data[0];
		if (ret < HASH_ALGO__LAST)
			return ret;
		break;
	case IMA_XATTR_DIGEST:
		 
		if (xattr_len == 21) {
			unsigned int zero = 0;
			if (!memcmp(&xattr_value->data[16], &zero, 4))
				return HASH_ALGO_MD5;
			else
				return HASH_ALGO_SHA1;
		} else if (xattr_len == 17)
			return HASH_ALGO_MD5;
		break;
	}

	 
	return ima_hash_algo;
}

int ima_read_xattr(struct dentry *dentry,
		   struct evm_ima_xattr_data **xattr_value, int xattr_len)
{
	int ret;

	ret = vfs_getxattr_alloc(&nop_mnt_idmap, dentry, XATTR_NAME_IMA,
				 (char **)xattr_value, xattr_len, GFP_NOFS);
	if (ret == -EOPNOTSUPP)
		ret = 0;
	return ret;
}

 
static int calc_file_id_hash(enum evm_ima_xattr_type type,
			     enum hash_algo algo, const u8 *digest,
			     struct ima_digest_data *hash)
{
	struct ima_file_id file_id = {
		.hash_type = IMA_VERITY_DIGSIG, .hash_algorithm = algo};
	unsigned int unused = HASH_MAX_DIGESTSIZE - hash_digest_size[algo];

	if (type != IMA_VERITY_DIGSIG)
		return -EINVAL;

	memcpy(file_id.hash, digest, hash_digest_size[algo]);

	hash->algo = algo;
	hash->length = hash_digest_size[algo];

	return ima_calc_buffer_hash(&file_id, sizeof(file_id) - unused, hash);
}

 
static int xattr_verify(enum ima_hooks func, struct integrity_iint_cache *iint,
			struct evm_ima_xattr_data *xattr_value, int xattr_len,
			enum integrity_status *status, const char **cause)
{
	struct ima_max_digest_data hash;
	struct signature_v2_hdr *sig;
	int rc = -EINVAL, hash_start = 0;
	int mask;

	switch (xattr_value->type) {
	case IMA_XATTR_DIGEST_NG:
		 
		hash_start = 1;
		fallthrough;
	case IMA_XATTR_DIGEST:
		if (*status != INTEGRITY_PASS_IMMUTABLE) {
			if (iint->flags & IMA_DIGSIG_REQUIRED) {
				if (iint->flags & IMA_VERITY_REQUIRED)
					*cause = "verity-signature-required";
				else
					*cause = "IMA-signature-required";
				*status = INTEGRITY_FAIL;
				break;
			}
			clear_bit(IMA_DIGSIG, &iint->atomic_flags);
		} else {
			set_bit(IMA_DIGSIG, &iint->atomic_flags);
		}
		if (xattr_len - sizeof(xattr_value->type) - hash_start >=
				iint->ima_hash->length)
			 
			rc = memcmp(&xattr_value->data[hash_start],
				    iint->ima_hash->digest,
				    iint->ima_hash->length);
		else
			rc = -EINVAL;
		if (rc) {
			*cause = "invalid-hash";
			*status = INTEGRITY_FAIL;
			break;
		}
		*status = INTEGRITY_PASS;
		break;
	case EVM_IMA_XATTR_DIGSIG:
		set_bit(IMA_DIGSIG, &iint->atomic_flags);

		mask = IMA_DIGSIG_REQUIRED | IMA_VERITY_REQUIRED;
		if ((iint->flags & mask) == mask) {
			*cause = "verity-signature-required";
			*status = INTEGRITY_FAIL;
			break;
		}

		sig = (typeof(sig))xattr_value;
		if (sig->version >= 3) {
			*cause = "invalid-signature-version";
			*status = INTEGRITY_FAIL;
			break;
		}
		rc = integrity_digsig_verify(INTEGRITY_KEYRING_IMA,
					     (const char *)xattr_value,
					     xattr_len,
					     iint->ima_hash->digest,
					     iint->ima_hash->length);
		if (rc == -EOPNOTSUPP) {
			*status = INTEGRITY_UNKNOWN;
			break;
		}
		if (IS_ENABLED(CONFIG_INTEGRITY_PLATFORM_KEYRING) && rc &&
		    func == KEXEC_KERNEL_CHECK)
			rc = integrity_digsig_verify(INTEGRITY_KEYRING_PLATFORM,
						     (const char *)xattr_value,
						     xattr_len,
						     iint->ima_hash->digest,
						     iint->ima_hash->length);
		if (rc) {
			*cause = "invalid-signature";
			*status = INTEGRITY_FAIL;
		} else {
			*status = INTEGRITY_PASS;
		}
		break;
	case IMA_VERITY_DIGSIG:
		set_bit(IMA_DIGSIG, &iint->atomic_flags);

		if (iint->flags & IMA_DIGSIG_REQUIRED) {
			if (!(iint->flags & IMA_VERITY_REQUIRED)) {
				*cause = "IMA-signature-required";
				*status = INTEGRITY_FAIL;
				break;
			}
		}

		sig = (typeof(sig))xattr_value;
		if (sig->version != 3) {
			*cause = "invalid-signature-version";
			*status = INTEGRITY_FAIL;
			break;
		}

		rc = calc_file_id_hash(IMA_VERITY_DIGSIG, iint->ima_hash->algo,
				       iint->ima_hash->digest, &hash.hdr);
		if (rc) {
			*cause = "sigv3-hashing-error";
			*status = INTEGRITY_FAIL;
			break;
		}

		rc = integrity_digsig_verify(INTEGRITY_KEYRING_IMA,
					     (const char *)xattr_value,
					     xattr_len, hash.digest,
					     hash.hdr.length);
		if (rc) {
			*cause = "invalid-verity-signature";
			*status = INTEGRITY_FAIL;
		} else {
			*status = INTEGRITY_PASS;
		}

		break;
	default:
		*status = INTEGRITY_UNKNOWN;
		*cause = "unknown-ima-data";
		break;
	}

	return rc;
}

 
static int modsig_verify(enum ima_hooks func, const struct modsig *modsig,
			 enum integrity_status *status, const char **cause)
{
	int rc;

	rc = integrity_modsig_verify(INTEGRITY_KEYRING_IMA, modsig);
	if (IS_ENABLED(CONFIG_INTEGRITY_PLATFORM_KEYRING) && rc &&
	    func == KEXEC_KERNEL_CHECK)
		rc = integrity_modsig_verify(INTEGRITY_KEYRING_PLATFORM,
					     modsig);
	if (rc) {
		*cause = "invalid-signature";
		*status = INTEGRITY_FAIL;
	} else {
		*status = INTEGRITY_PASS;
	}

	return rc;
}

 
int ima_check_blacklist(struct integrity_iint_cache *iint,
			const struct modsig *modsig, int pcr)
{
	enum hash_algo hash_algo;
	const u8 *digest = NULL;
	u32 digestsize = 0;
	int rc = 0;

	if (!(iint->flags & IMA_CHECK_BLACKLIST))
		return 0;

	if (iint->flags & IMA_MODSIG_ALLOWED && modsig) {
		ima_get_modsig_digest(modsig, &hash_algo, &digest, &digestsize);

		rc = is_binary_blacklisted(digest, digestsize);
	} else if (iint->flags & IMA_DIGSIG_REQUIRED && iint->ima_hash)
		rc = is_binary_blacklisted(iint->ima_hash->digest, iint->ima_hash->length);

	if ((rc == -EPERM) && (iint->flags & IMA_MEASURE))
		process_buffer_measurement(&nop_mnt_idmap, NULL, digest, digestsize,
					   "blacklisted-hash", NONE,
					   pcr, NULL, false, NULL, 0);

	return rc;
}

 
int ima_appraise_measurement(enum ima_hooks func,
			     struct integrity_iint_cache *iint,
			     struct file *file, const unsigned char *filename,
			     struct evm_ima_xattr_data *xattr_value,
			     int xattr_len, const struct modsig *modsig)
{
	static const char op[] = "appraise_data";
	const char *cause = "unknown";
	struct dentry *dentry = file_dentry(file);
	struct inode *inode = d_backing_inode(dentry);
	enum integrity_status status = INTEGRITY_UNKNOWN;
	int rc = xattr_len;
	bool try_modsig = iint->flags & IMA_MODSIG_ALLOWED && modsig;

	 
	if (!(inode->i_opflags & IOP_XATTR) && !try_modsig)
		return INTEGRITY_UNKNOWN;

	 
	if (rc <= 0 && !try_modsig) {
		if (rc && rc != -ENODATA)
			goto out;

		if (iint->flags & IMA_DIGSIG_REQUIRED) {
			if (iint->flags & IMA_VERITY_REQUIRED)
				cause = "verity-signature-required";
			else
				cause = "IMA-signature-required";
		} else {
			cause = "missing-hash";
		}

		status = INTEGRITY_NOLABEL;
		if (file->f_mode & FMODE_CREATED)
			iint->flags |= IMA_NEW_FILE;
		if ((iint->flags & IMA_NEW_FILE) &&
		    (!(iint->flags & IMA_DIGSIG_REQUIRED) ||
		     (inode->i_size == 0)))
			status = INTEGRITY_PASS;
		goto out;
	}

	status = evm_verifyxattr(dentry, XATTR_NAME_IMA, xattr_value,
				 rc < 0 ? 0 : rc, iint);
	switch (status) {
	case INTEGRITY_PASS:
	case INTEGRITY_PASS_IMMUTABLE:
	case INTEGRITY_UNKNOWN:
		break;
	case INTEGRITY_NOXATTRS:	 
		 
		if (try_modsig)
			break;
		fallthrough;
	case INTEGRITY_NOLABEL:		 
		cause = "missing-HMAC";
		goto out;
	case INTEGRITY_FAIL_IMMUTABLE:
		set_bit(IMA_DIGSIG, &iint->atomic_flags);
		cause = "invalid-fail-immutable";
		goto out;
	case INTEGRITY_FAIL:		 
		cause = "invalid-HMAC";
		goto out;
	default:
		WARN_ONCE(true, "Unexpected integrity status %d\n", status);
	}

	if (xattr_value)
		rc = xattr_verify(func, iint, xattr_value, xattr_len, &status,
				  &cause);

	 
	if (try_modsig &&
	    (!xattr_value || xattr_value->type == IMA_XATTR_DIGEST_NG ||
	     rc == -ENOKEY))
		rc = modsig_verify(func, modsig, &status, &cause);

out:
	 
	if ((inode->i_sb->s_iflags & SB_I_IMA_UNVERIFIABLE_SIGNATURE) &&
	    ((inode->i_sb->s_iflags & SB_I_UNTRUSTED_MOUNTER) ||
	     (iint->flags & IMA_FAIL_UNVERIFIABLE_SIGS))) {
		status = INTEGRITY_FAIL;
		cause = "unverifiable-signature";
		integrity_audit_msg(AUDIT_INTEGRITY_DATA, inode, filename,
				    op, cause, rc, 0);
	} else if (status != INTEGRITY_PASS) {
		 
		if ((ima_appraise & IMA_APPRAISE_FIX) && !try_modsig &&
		    (!xattr_value ||
		     xattr_value->type != EVM_IMA_XATTR_DIGSIG)) {
			if (!ima_fix_xattr(dentry, iint))
				status = INTEGRITY_PASS;
		}

		 
		if (inode->i_size == 0 && iint->flags & IMA_NEW_FILE &&
		    test_bit(IMA_DIGSIG, &iint->atomic_flags)) {
			status = INTEGRITY_PASS;
		}

		integrity_audit_msg(AUDIT_INTEGRITY_DATA, inode, filename,
				    op, cause, rc, 0);
	} else {
		ima_cache_flags(iint, func);
	}

	ima_set_cache_status(iint, func, status);
	return status;
}

 
void ima_update_xattr(struct integrity_iint_cache *iint, struct file *file)
{
	struct dentry *dentry = file_dentry(file);
	int rc = 0;

	 
	if (test_bit(IMA_DIGSIG, &iint->atomic_flags))
		return;

	if ((iint->ima_file_status != INTEGRITY_PASS) &&
	    !(iint->flags & IMA_HASH))
		return;

	rc = ima_collect_measurement(iint, file, NULL, 0, ima_hash_algo, NULL);
	if (rc < 0)
		return;

	inode_lock(file_inode(file));
	ima_fix_xattr(dentry, iint);
	inode_unlock(file_inode(file));
}

 
void ima_inode_post_setattr(struct mnt_idmap *idmap,
			    struct dentry *dentry)
{
	struct inode *inode = d_backing_inode(dentry);
	struct integrity_iint_cache *iint;
	int action;

	if (!(ima_policy_flag & IMA_APPRAISE) || !S_ISREG(inode->i_mode)
	    || !(inode->i_opflags & IOP_XATTR))
		return;

	action = ima_must_appraise(idmap, inode, MAY_ACCESS, POST_SETATTR);
	iint = integrity_iint_find(inode);
	if (iint) {
		set_bit(IMA_CHANGE_ATTR, &iint->atomic_flags);
		if (!action)
			clear_bit(IMA_UPDATE_XATTR, &iint->atomic_flags);
	}
}

 
static int ima_protect_xattr(struct dentry *dentry, const char *xattr_name,
			     const void *xattr_value, size_t xattr_value_len)
{
	if (strcmp(xattr_name, XATTR_NAME_IMA) == 0) {
		if (!capable(CAP_SYS_ADMIN))
			return -EPERM;
		return 1;
	}
	return 0;
}

static void ima_reset_appraise_flags(struct inode *inode, int digsig)
{
	struct integrity_iint_cache *iint;

	if (!(ima_policy_flag & IMA_APPRAISE) || !S_ISREG(inode->i_mode))
		return;

	iint = integrity_iint_find(inode);
	if (!iint)
		return;
	iint->measured_pcrs = 0;
	set_bit(IMA_CHANGE_XATTR, &iint->atomic_flags);
	if (digsig)
		set_bit(IMA_DIGSIG, &iint->atomic_flags);
	else
		clear_bit(IMA_DIGSIG, &iint->atomic_flags);
}

 
static int validate_hash_algo(struct dentry *dentry,
			      const struct evm_ima_xattr_data *xattr_value,
			      size_t xattr_value_len)
{
	char *path = NULL, *pathbuf = NULL;
	enum hash_algo xattr_hash_algo;
	const char *errmsg = "unavailable-hash-algorithm";
	unsigned int allowed_hashes;

	xattr_hash_algo = ima_get_hash_algo(xattr_value, xattr_value_len);

	allowed_hashes = atomic_read(&ima_setxattr_allowed_hash_algorithms);

	if (allowed_hashes) {
		 
		if (allowed_hashes & (1U << xattr_hash_algo))
			return 0;

		 
		errmsg = "denied-hash-algorithm";
	} else {
		if (likely(xattr_hash_algo == ima_hash_algo))
			return 0;

		 
		if (crypto_has_alg(hash_algo_name[xattr_hash_algo], 0, 0))
			return 0;
	}

	pathbuf = kmalloc(PATH_MAX, GFP_KERNEL);
	if (!pathbuf)
		return -EACCES;

	path = dentry_path(dentry, pathbuf, PATH_MAX);

	integrity_audit_msg(AUDIT_INTEGRITY_DATA, d_inode(dentry), path,
			    "set_data", errmsg, -EACCES, 0);

	kfree(pathbuf);

	return -EACCES;
}

int ima_inode_setxattr(struct dentry *dentry, const char *xattr_name,
		       const void *xattr_value, size_t xattr_value_len)
{
	const struct evm_ima_xattr_data *xvalue = xattr_value;
	int digsig = 0;
	int result;
	int err;

	result = ima_protect_xattr(dentry, xattr_name, xattr_value,
				   xattr_value_len);
	if (result == 1) {
		if (!xattr_value_len || (xvalue->type >= IMA_XATTR_LAST))
			return -EINVAL;

		err = validate_hash_algo(dentry, xvalue, xattr_value_len);
		if (err)
			return err;

		digsig = (xvalue->type == EVM_IMA_XATTR_DIGSIG);
	} else if (!strcmp(xattr_name, XATTR_NAME_EVM) && xattr_value_len > 0) {
		digsig = (xvalue->type == EVM_XATTR_PORTABLE_DIGSIG);
	}
	if (result == 1 || evm_revalidate_status(xattr_name)) {
		ima_reset_appraise_flags(d_backing_inode(dentry), digsig);
		if (result == 1)
			result = 0;
	}
	return result;
}

int ima_inode_set_acl(struct mnt_idmap *idmap, struct dentry *dentry,
		      const char *acl_name, struct posix_acl *kacl)
{
	if (evm_revalidate_status(acl_name))
		ima_reset_appraise_flags(d_backing_inode(dentry), 0);

	return 0;
}

int ima_inode_removexattr(struct dentry *dentry, const char *xattr_name)
{
	int result;

	result = ima_protect_xattr(dentry, xattr_name, NULL, 0);
	if (result == 1 || evm_revalidate_status(xattr_name)) {
		ima_reset_appraise_flags(d_backing_inode(dentry), 0);
		if (result == 1)
			result = 0;
	}
	return result;
}
