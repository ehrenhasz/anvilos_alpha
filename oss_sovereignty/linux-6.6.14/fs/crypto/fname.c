
 

#include <linux/namei.h>
#include <linux/scatterlist.h>
#include <crypto/hash.h>
#include <crypto/sha2.h>
#include <crypto/skcipher.h>
#include "fscrypt_private.h"

 
#define FSCRYPT_FNAME_MIN_MSG_LEN 16

 
struct fscrypt_nokey_name {
	u32 dirhash[2];
	u8 bytes[149];
	u8 sha256[SHA256_DIGEST_SIZE];
};  

 
#define FSCRYPT_NOKEY_NAME_MAX	offsetofend(struct fscrypt_nokey_name, sha256)

 
#define FSCRYPT_NOKEY_NAME_MAX_ENCODED \
		FSCRYPT_BASE64URL_CHARS(FSCRYPT_NOKEY_NAME_MAX)

static inline bool fscrypt_is_dot_dotdot(const struct qstr *str)
{
	if (str->len == 1 && str->name[0] == '.')
		return true;

	if (str->len == 2 && str->name[0] == '.' && str->name[1] == '.')
		return true;

	return false;
}

 
int fscrypt_fname_encrypt(const struct inode *inode, const struct qstr *iname,
			  u8 *out, unsigned int olen)
{
	struct skcipher_request *req = NULL;
	DECLARE_CRYPTO_WAIT(wait);
	const struct fscrypt_info *ci = inode->i_crypt_info;
	struct crypto_skcipher *tfm = ci->ci_enc_key.tfm;
	union fscrypt_iv iv;
	struct scatterlist sg;
	int res;

	 
	if (WARN_ON_ONCE(olen < iname->len))
		return -ENOBUFS;
	memcpy(out, iname->name, iname->len);
	memset(out + iname->len, 0, olen - iname->len);

	 
	fscrypt_generate_iv(&iv, 0, ci);

	 
	req = skcipher_request_alloc(tfm, GFP_NOFS);
	if (!req)
		return -ENOMEM;
	skcipher_request_set_callback(req,
			CRYPTO_TFM_REQ_MAY_BACKLOG | CRYPTO_TFM_REQ_MAY_SLEEP,
			crypto_req_done, &wait);
	sg_init_one(&sg, out, olen);
	skcipher_request_set_crypt(req, &sg, &sg, olen, &iv);

	 
	res = crypto_wait_req(crypto_skcipher_encrypt(req), &wait);
	skcipher_request_free(req);
	if (res < 0) {
		fscrypt_err(inode, "Filename encryption failed: %d", res);
		return res;
	}

	return 0;
}
EXPORT_SYMBOL_GPL(fscrypt_fname_encrypt);

 
static int fname_decrypt(const struct inode *inode,
			 const struct fscrypt_str *iname,
			 struct fscrypt_str *oname)
{
	struct skcipher_request *req = NULL;
	DECLARE_CRYPTO_WAIT(wait);
	struct scatterlist src_sg, dst_sg;
	const struct fscrypt_info *ci = inode->i_crypt_info;
	struct crypto_skcipher *tfm = ci->ci_enc_key.tfm;
	union fscrypt_iv iv;
	int res;

	 
	req = skcipher_request_alloc(tfm, GFP_NOFS);
	if (!req)
		return -ENOMEM;
	skcipher_request_set_callback(req,
		CRYPTO_TFM_REQ_MAY_BACKLOG | CRYPTO_TFM_REQ_MAY_SLEEP,
		crypto_req_done, &wait);

	 
	fscrypt_generate_iv(&iv, 0, ci);

	 
	sg_init_one(&src_sg, iname->name, iname->len);
	sg_init_one(&dst_sg, oname->name, oname->len);
	skcipher_request_set_crypt(req, &src_sg, &dst_sg, iname->len, &iv);
	res = crypto_wait_req(crypto_skcipher_decrypt(req), &wait);
	skcipher_request_free(req);
	if (res < 0) {
		fscrypt_err(inode, "Filename decryption failed: %d", res);
		return res;
	}

	oname->len = strnlen(oname->name, iname->len);
	return 0;
}

static const char base64url_table[65] =
	"ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789-_";

#define FSCRYPT_BASE64URL_CHARS(nbytes)	DIV_ROUND_UP((nbytes) * 4, 3)

 
static int fscrypt_base64url_encode(const u8 *src, int srclen, char *dst)
{
	u32 ac = 0;
	int bits = 0;
	int i;
	char *cp = dst;

	for (i = 0; i < srclen; i++) {
		ac = (ac << 8) | src[i];
		bits += 8;
		do {
			bits -= 6;
			*cp++ = base64url_table[(ac >> bits) & 0x3f];
		} while (bits >= 6);
	}
	if (bits)
		*cp++ = base64url_table[(ac << (6 - bits)) & 0x3f];
	return cp - dst;
}

 
static int fscrypt_base64url_decode(const char *src, int srclen, u8 *dst)
{
	u32 ac = 0;
	int bits = 0;
	int i;
	u8 *bp = dst;

	for (i = 0; i < srclen; i++) {
		const char *p = strchr(base64url_table, src[i]);

		if (p == NULL || src[i] == 0)
			return -1;
		ac = (ac << 6) | (p - base64url_table);
		bits += 6;
		if (bits >= 8) {
			bits -= 8;
			*bp++ = (u8)(ac >> bits);
		}
	}
	if (ac & ((1 << bits) - 1))
		return -1;
	return bp - dst;
}

bool __fscrypt_fname_encrypted_size(const union fscrypt_policy *policy,
				    u32 orig_len, u32 max_len,
				    u32 *encrypted_len_ret)
{
	int padding = 4 << (fscrypt_policy_flags(policy) &
			    FSCRYPT_POLICY_FLAGS_PAD_MASK);
	u32 encrypted_len;

	if (orig_len > max_len)
		return false;
	encrypted_len = max_t(u32, orig_len, FSCRYPT_FNAME_MIN_MSG_LEN);
	encrypted_len = round_up(encrypted_len, padding);
	*encrypted_len_ret = min(encrypted_len, max_len);
	return true;
}

 
bool fscrypt_fname_encrypted_size(const struct inode *inode, u32 orig_len,
				  u32 max_len, u32 *encrypted_len_ret)
{
	return __fscrypt_fname_encrypted_size(&inode->i_crypt_info->ci_policy,
					      orig_len, max_len,
					      encrypted_len_ret);
}
EXPORT_SYMBOL_GPL(fscrypt_fname_encrypted_size);

 
int fscrypt_fname_alloc_buffer(u32 max_encrypted_len,
			       struct fscrypt_str *crypto_str)
{
	u32 max_presented_len = max_t(u32, FSCRYPT_NOKEY_NAME_MAX_ENCODED,
				      max_encrypted_len);

	crypto_str->name = kmalloc(max_presented_len + 1, GFP_NOFS);
	if (!crypto_str->name)
		return -ENOMEM;
	crypto_str->len = max_presented_len;
	return 0;
}
EXPORT_SYMBOL(fscrypt_fname_alloc_buffer);

 
void fscrypt_fname_free_buffer(struct fscrypt_str *crypto_str)
{
	if (!crypto_str)
		return;
	kfree(crypto_str->name);
	crypto_str->name = NULL;
}
EXPORT_SYMBOL(fscrypt_fname_free_buffer);

 
int fscrypt_fname_disk_to_usr(const struct inode *inode,
			      u32 hash, u32 minor_hash,
			      const struct fscrypt_str *iname,
			      struct fscrypt_str *oname)
{
	const struct qstr qname = FSTR_TO_QSTR(iname);
	struct fscrypt_nokey_name nokey_name;
	u32 size;  

	if (fscrypt_is_dot_dotdot(&qname)) {
		oname->name[0] = '.';
		oname->name[iname->len - 1] = '.';
		oname->len = iname->len;
		return 0;
	}

	if (iname->len < FSCRYPT_FNAME_MIN_MSG_LEN)
		return -EUCLEAN;

	if (fscrypt_has_encryption_key(inode))
		return fname_decrypt(inode, iname, oname);

	 
	BUILD_BUG_ON(offsetofend(struct fscrypt_nokey_name, dirhash) !=
		     offsetof(struct fscrypt_nokey_name, bytes));
	BUILD_BUG_ON(offsetofend(struct fscrypt_nokey_name, bytes) !=
		     offsetof(struct fscrypt_nokey_name, sha256));
	BUILD_BUG_ON(FSCRYPT_NOKEY_NAME_MAX_ENCODED > NAME_MAX);

	nokey_name.dirhash[0] = hash;
	nokey_name.dirhash[1] = minor_hash;

	if (iname->len <= sizeof(nokey_name.bytes)) {
		memcpy(nokey_name.bytes, iname->name, iname->len);
		size = offsetof(struct fscrypt_nokey_name, bytes[iname->len]);
	} else {
		memcpy(nokey_name.bytes, iname->name, sizeof(nokey_name.bytes));
		 
		sha256(&iname->name[sizeof(nokey_name.bytes)],
		       iname->len - sizeof(nokey_name.bytes),
		       nokey_name.sha256);
		size = FSCRYPT_NOKEY_NAME_MAX;
	}
	oname->len = fscrypt_base64url_encode((const u8 *)&nokey_name, size,
					      oname->name);
	return 0;
}
EXPORT_SYMBOL(fscrypt_fname_disk_to_usr);

 
int fscrypt_setup_filename(struct inode *dir, const struct qstr *iname,
			      int lookup, struct fscrypt_name *fname)
{
	struct fscrypt_nokey_name *nokey_name;
	int ret;

	memset(fname, 0, sizeof(struct fscrypt_name));
	fname->usr_fname = iname;

	if (!IS_ENCRYPTED(dir) || fscrypt_is_dot_dotdot(iname)) {
		fname->disk_name.name = (unsigned char *)iname->name;
		fname->disk_name.len = iname->len;
		return 0;
	}
	ret = fscrypt_get_encryption_info(dir, lookup);
	if (ret)
		return ret;

	if (fscrypt_has_encryption_key(dir)) {
		if (!fscrypt_fname_encrypted_size(dir, iname->len, NAME_MAX,
						  &fname->crypto_buf.len))
			return -ENAMETOOLONG;
		fname->crypto_buf.name = kmalloc(fname->crypto_buf.len,
						 GFP_NOFS);
		if (!fname->crypto_buf.name)
			return -ENOMEM;

		ret = fscrypt_fname_encrypt(dir, iname, fname->crypto_buf.name,
					    fname->crypto_buf.len);
		if (ret)
			goto errout;
		fname->disk_name.name = fname->crypto_buf.name;
		fname->disk_name.len = fname->crypto_buf.len;
		return 0;
	}
	if (!lookup)
		return -ENOKEY;
	fname->is_nokey_name = true;

	 

	if (iname->len > FSCRYPT_NOKEY_NAME_MAX_ENCODED)
		return -ENOENT;

	fname->crypto_buf.name = kmalloc(FSCRYPT_NOKEY_NAME_MAX, GFP_KERNEL);
	if (fname->crypto_buf.name == NULL)
		return -ENOMEM;

	ret = fscrypt_base64url_decode(iname->name, iname->len,
				       fname->crypto_buf.name);
	if (ret < (int)offsetof(struct fscrypt_nokey_name, bytes[1]) ||
	    (ret > offsetof(struct fscrypt_nokey_name, sha256) &&
	     ret != FSCRYPT_NOKEY_NAME_MAX)) {
		ret = -ENOENT;
		goto errout;
	}
	fname->crypto_buf.len = ret;

	nokey_name = (void *)fname->crypto_buf.name;
	fname->hash = nokey_name->dirhash[0];
	fname->minor_hash = nokey_name->dirhash[1];
	if (ret != FSCRYPT_NOKEY_NAME_MAX) {
		 
		fname->disk_name.name = nokey_name->bytes;
		fname->disk_name.len =
			ret - offsetof(struct fscrypt_nokey_name, bytes);
	}
	return 0;

errout:
	kfree(fname->crypto_buf.name);
	return ret;
}
EXPORT_SYMBOL(fscrypt_setup_filename);

 
bool fscrypt_match_name(const struct fscrypt_name *fname,
			const u8 *de_name, u32 de_name_len)
{
	const struct fscrypt_nokey_name *nokey_name =
		(const void *)fname->crypto_buf.name;
	u8 digest[SHA256_DIGEST_SIZE];

	if (likely(fname->disk_name.name)) {
		if (de_name_len != fname->disk_name.len)
			return false;
		return !memcmp(de_name, fname->disk_name.name, de_name_len);
	}
	if (de_name_len <= sizeof(nokey_name->bytes))
		return false;
	if (memcmp(de_name, nokey_name->bytes, sizeof(nokey_name->bytes)))
		return false;
	sha256(&de_name[sizeof(nokey_name->bytes)],
	       de_name_len - sizeof(nokey_name->bytes), digest);
	return !memcmp(digest, nokey_name->sha256, sizeof(digest));
}
EXPORT_SYMBOL_GPL(fscrypt_match_name);

 
u64 fscrypt_fname_siphash(const struct inode *dir, const struct qstr *name)
{
	const struct fscrypt_info *ci = dir->i_crypt_info;

	WARN_ON_ONCE(!ci->ci_dirhash_key_initialized);

	return siphash(name->name, name->len, &ci->ci_dirhash_key);
}
EXPORT_SYMBOL_GPL(fscrypt_fname_siphash);

 
int fscrypt_d_revalidate(struct dentry *dentry, unsigned int flags)
{
	struct dentry *dir;
	int err;
	int valid;

	 
	if (!(dentry->d_flags & DCACHE_NOKEY_NAME))
		return 1;

	 

	if (flags & LOOKUP_RCU)
		return -ECHILD;

	dir = dget_parent(dentry);
	 
	err = fscrypt_get_encryption_info(d_inode(dir), true);
	valid = !fscrypt_has_encryption_key(d_inode(dir));
	dput(dir);

	if (err < 0)
		return err;

	return valid;
}
EXPORT_SYMBOL_GPL(fscrypt_d_revalidate);
