
 
#include <linux/ceph/ceph_debug.h>
#include <linux/xattr.h>
#include <linux/fscrypt.h>
#include <linux/ceph/striper.h>

#include "super.h"
#include "mds_client.h"
#include "crypto.h"

 
static const char base64_table[65] =
	"ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+,";

int ceph_base64_encode(const u8 *src, int srclen, char *dst)
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
			*cp++ = base64_table[(ac >> bits) & 0x3f];
		} while (bits >= 6);
	}
	if (bits)
		*cp++ = base64_table[(ac << (6 - bits)) & 0x3f];
	return cp - dst;
}

int ceph_base64_decode(const char *src, int srclen, u8 *dst)
{
	u32 ac = 0;
	int bits = 0;
	int i;
	u8 *bp = dst;

	for (i = 0; i < srclen; i++) {
		const char *p = strchr(base64_table, src[i]);

		if (p == NULL || src[i] == 0)
			return -1;
		ac = (ac << 6) | (p - base64_table);
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

static int ceph_crypt_get_context(struct inode *inode, void *ctx, size_t len)
{
	struct ceph_inode_info *ci = ceph_inode(inode);
	struct ceph_fscrypt_auth *cfa = (struct ceph_fscrypt_auth *)ci->fscrypt_auth;
	u32 ctxlen;

	 
	if (!cfa || (ci->fscrypt_auth_len < (offsetof(struct ceph_fscrypt_auth, cfa_blob) + 1)))
		return -ENOBUFS;

	 
	if (le32_to_cpu(cfa->cfa_version) != CEPH_FSCRYPT_AUTH_VERSION)
		return -ENOBUFS;

	ctxlen = le32_to_cpu(cfa->cfa_blob_len);
	if (len < ctxlen)
		return -ERANGE;

	memcpy(ctx, cfa->cfa_blob, ctxlen);
	return ctxlen;
}

static int ceph_crypt_set_context(struct inode *inode, const void *ctx,
				  size_t len, void *fs_data)
{
	int ret;
	struct iattr attr = { };
	struct ceph_iattr cia = { };
	struct ceph_fscrypt_auth *cfa;

	WARN_ON_ONCE(fs_data);

	if (len > FSCRYPT_SET_CONTEXT_MAX_SIZE)
		return -EINVAL;

	cfa = kzalloc(sizeof(*cfa), GFP_KERNEL);
	if (!cfa)
		return -ENOMEM;

	cfa->cfa_version = cpu_to_le32(CEPH_FSCRYPT_AUTH_VERSION);
	cfa->cfa_blob_len = cpu_to_le32(len);
	memcpy(cfa->cfa_blob, ctx, len);

	cia.fscrypt_auth = cfa;

	ret = __ceph_setattr(inode, &attr, &cia);
	if (ret == 0)
		inode_set_flags(inode, S_ENCRYPTED, S_ENCRYPTED);
	kfree(cia.fscrypt_auth);
	return ret;
}

static bool ceph_crypt_empty_dir(struct inode *inode)
{
	struct ceph_inode_info *ci = ceph_inode(inode);

	return ci->i_rsubdirs + ci->i_rfiles == 1;
}

static const union fscrypt_policy *ceph_get_dummy_policy(struct super_block *sb)
{
	return ceph_sb_to_client(sb)->fsc_dummy_enc_policy.policy;
}

static struct fscrypt_operations ceph_fscrypt_ops = {
	.get_context		= ceph_crypt_get_context,
	.set_context		= ceph_crypt_set_context,
	.get_dummy_policy	= ceph_get_dummy_policy,
	.empty_dir		= ceph_crypt_empty_dir,
};

void ceph_fscrypt_set_ops(struct super_block *sb)
{
	fscrypt_set_ops(sb, &ceph_fscrypt_ops);
}

void ceph_fscrypt_free_dummy_policy(struct ceph_fs_client *fsc)
{
	fscrypt_free_dummy_policy(&fsc->fsc_dummy_enc_policy);
}

int ceph_fscrypt_prepare_context(struct inode *dir, struct inode *inode,
				 struct ceph_acl_sec_ctx *as)
{
	int ret, ctxsize;
	bool encrypted = false;
	struct ceph_inode_info *ci = ceph_inode(inode);

	ret = fscrypt_prepare_new_inode(dir, inode, &encrypted);
	if (ret)
		return ret;
	if (!encrypted)
		return 0;

	as->fscrypt_auth = kzalloc(sizeof(*as->fscrypt_auth), GFP_KERNEL);
	if (!as->fscrypt_auth)
		return -ENOMEM;

	ctxsize = fscrypt_context_for_new_inode(as->fscrypt_auth->cfa_blob,
						inode);
	if (ctxsize < 0)
		return ctxsize;

	as->fscrypt_auth->cfa_version = cpu_to_le32(CEPH_FSCRYPT_AUTH_VERSION);
	as->fscrypt_auth->cfa_blob_len = cpu_to_le32(ctxsize);

	WARN_ON_ONCE(ci->fscrypt_auth);
	kfree(ci->fscrypt_auth);
	ci->fscrypt_auth_len = ceph_fscrypt_auth_len(as->fscrypt_auth);
	ci->fscrypt_auth = kmemdup(as->fscrypt_auth, ci->fscrypt_auth_len,
				   GFP_KERNEL);
	if (!ci->fscrypt_auth)
		return -ENOMEM;

	inode->i_flags |= S_ENCRYPTED;

	return 0;
}

void ceph_fscrypt_as_ctx_to_req(struct ceph_mds_request *req,
				struct ceph_acl_sec_ctx *as)
{
	swap(req->r_fscrypt_auth, as->fscrypt_auth);
}

 
static struct inode *parse_longname(const struct inode *parent,
				    const char *name, int *name_len)
{
	struct inode *dir = NULL;
	struct ceph_vino vino = { .snap = CEPH_NOSNAP };
	char *inode_number;
	char *name_end;
	int orig_len = *name_len;
	int ret = -EIO;

	 
	name++;
	name_end = strrchr(name, '_');
	if (!name_end) {
		dout("Failed to parse long snapshot name: %s\n", name);
		return ERR_PTR(-EIO);
	}
	*name_len = (name_end - name);
	if (*name_len <= 0) {
		pr_err("Failed to parse long snapshot name\n");
		return ERR_PTR(-EIO);
	}

	 
	inode_number = kmemdup_nul(name_end + 1,
				   orig_len - *name_len - 2,
				   GFP_KERNEL);
	if (!inode_number)
		return ERR_PTR(-ENOMEM);
	ret = kstrtou64(inode_number, 10, &vino.ino);
	if (ret) {
		dout("Failed to parse inode number: %s\n", name);
		dir = ERR_PTR(ret);
		goto out;
	}

	 
	dir = ceph_find_inode(parent->i_sb, vino);
	if (!dir) {
		 
		dir = ceph_get_inode(parent->i_sb, vino, NULL);
		if (IS_ERR(dir))
			dout("Can't find inode %s (%s)\n", inode_number, name);
	}

out:
	kfree(inode_number);
	return dir;
}

int ceph_encode_encrypted_dname(struct inode *parent, struct qstr *d_name,
				char *buf)
{
	struct inode *dir = parent;
	struct qstr iname;
	u32 len;
	int name_len;
	int elen;
	int ret;
	u8 *cryptbuf = NULL;

	iname.name = d_name->name;
	name_len = d_name->len;

	 
	if ((ceph_snap(dir) == CEPH_SNAPDIR) && (name_len > 0) &&
	    (iname.name[0] == '_')) {
		dir = parse_longname(parent, iname.name, &name_len);
		if (IS_ERR(dir))
			return PTR_ERR(dir);
		iname.name++;  
	}
	iname.len = name_len;

	if (!fscrypt_has_encryption_key(dir)) {
		memcpy(buf, d_name->name, d_name->len);
		elen = d_name->len;
		goto out;
	}

	 
	if (!fscrypt_fname_encrypted_size(dir, iname.len, NAME_MAX, &len)) {
		elen = -ENAMETOOLONG;
		goto out;
	}

	 
	cryptbuf = kmalloc(len > CEPH_NOHASH_NAME_MAX ? NAME_MAX : len,
			   GFP_KERNEL);
	if (!cryptbuf) {
		elen = -ENOMEM;
		goto out;
	}

	ret = fscrypt_fname_encrypt(dir, &iname, cryptbuf, len);
	if (ret) {
		elen = ret;
		goto out;
	}

	 
	if (len > CEPH_NOHASH_NAME_MAX) {
		u8 hash[SHA256_DIGEST_SIZE];
		u8 *extra = cryptbuf + CEPH_NOHASH_NAME_MAX;

		 
		sha256(extra, len - CEPH_NOHASH_NAME_MAX, hash);
		memcpy(extra, hash, SHA256_DIGEST_SIZE);
		len = CEPH_NOHASH_NAME_MAX + SHA256_DIGEST_SIZE;
	}

	 
	elen = ceph_base64_encode(cryptbuf, len, buf);
	dout("base64-encoded ciphertext name = %.*s\n", elen, buf);

	 
	WARN_ON(elen > 240);
	if ((elen > 0) && (dir != parent)) {
		char tmp_buf[NAME_MAX];

		elen = snprintf(tmp_buf, sizeof(tmp_buf), "_%.*s_%ld",
				elen, buf, dir->i_ino);
		memcpy(buf, tmp_buf, elen);
	}

out:
	kfree(cryptbuf);
	if (dir != parent) {
		if ((dir->i_state & I_NEW))
			discard_new_inode(dir);
		else
			iput(dir);
	}
	return elen;
}

int ceph_encode_encrypted_fname(struct inode *parent, struct dentry *dentry,
				char *buf)
{
	WARN_ON_ONCE(!fscrypt_has_encryption_key(parent));

	return ceph_encode_encrypted_dname(parent, &dentry->d_name, buf);
}

 
int ceph_fname_to_usr(const struct ceph_fname *fname, struct fscrypt_str *tname,
		      struct fscrypt_str *oname, bool *is_nokey)
{
	struct inode *dir = fname->dir;
	struct fscrypt_str _tname = FSTR_INIT(NULL, 0);
	struct fscrypt_str iname;
	char *name = fname->name;
	int name_len = fname->name_len;
	int ret;

	 
	if (fname->name_len > NAME_MAX || fname->ctext_len > NAME_MAX)
		return -EIO;

	 
	if ((ceph_snap(dir) == CEPH_SNAPDIR) && (name_len > 0) &&
	    (name[0] == '_')) {
		dir = parse_longname(dir, name, &name_len);
		if (IS_ERR(dir))
			return PTR_ERR(dir);
		name++;  
	}

	if (!IS_ENCRYPTED(dir)) {
		oname->name = fname->name;
		oname->len = fname->name_len;
		ret = 0;
		goto out_inode;
	}

	ret = ceph_fscrypt_prepare_readdir(dir);
	if (ret)
		goto out_inode;

	 
	if (!fscrypt_has_encryption_key(dir)) {
		if (fname->no_copy)
			oname->name = fname->name;
		else
			memcpy(oname->name, fname->name, fname->name_len);
		oname->len = fname->name_len;
		if (is_nokey)
			*is_nokey = true;
		ret = 0;
		goto out_inode;
	}

	if (fname->ctext_len == 0) {
		int declen;

		if (!tname) {
			ret = fscrypt_fname_alloc_buffer(NAME_MAX, &_tname);
			if (ret)
				goto out_inode;
			tname = &_tname;
		}

		declen = ceph_base64_decode(name, name_len, tname->name);
		if (declen <= 0) {
			ret = -EIO;
			goto out;
		}
		iname.name = tname->name;
		iname.len = declen;
	} else {
		iname.name = fname->ctext;
		iname.len = fname->ctext_len;
	}

	ret = fscrypt_fname_disk_to_usr(dir, 0, 0, &iname, oname);
	if (!ret && (dir != fname->dir)) {
		char tmp_buf[CEPH_BASE64_CHARS(NAME_MAX)];

		name_len = snprintf(tmp_buf, sizeof(tmp_buf), "_%.*s_%ld",
				    oname->len, oname->name, dir->i_ino);
		memcpy(oname->name, tmp_buf, name_len);
		oname->len = name_len;
	}

out:
	fscrypt_fname_free_buffer(&_tname);
out_inode:
	if (dir != fname->dir) {
		if ((dir->i_state & I_NEW))
			discard_new_inode(dir);
		else
			iput(dir);
	}
	return ret;
}

 
int ceph_fscrypt_prepare_readdir(struct inode *dir)
{
	bool had_key = fscrypt_has_encryption_key(dir);
	int err;

	if (!IS_ENCRYPTED(dir))
		return 0;

	err = __fscrypt_prepare_readdir(dir);
	if (err)
		return err;
	if (!had_key && fscrypt_has_encryption_key(dir)) {
		 
		ceph_dir_clear_complete(dir);
		return 1;
	}
	return 0;
}

int ceph_fscrypt_decrypt_block_inplace(const struct inode *inode,
				  struct page *page, unsigned int len,
				  unsigned int offs, u64 lblk_num)
{
	dout("%s: len %u offs %u blk %llu\n", __func__, len, offs, lblk_num);
	return fscrypt_decrypt_block_inplace(inode, page, len, offs, lblk_num);
}

int ceph_fscrypt_encrypt_block_inplace(const struct inode *inode,
				  struct page *page, unsigned int len,
				  unsigned int offs, u64 lblk_num,
				  gfp_t gfp_flags)
{
	dout("%s: len %u offs %u blk %llu\n", __func__, len, offs, lblk_num);
	return fscrypt_encrypt_block_inplace(inode, page, len, offs, lblk_num,
					     gfp_flags);
}

 
int ceph_fscrypt_decrypt_pages(struct inode *inode, struct page **page,
			       u64 off, int len)
{
	int i, num_blocks;
	u64 baseblk = off >> CEPH_FSCRYPT_BLOCK_SHIFT;
	int ret = 0;

	 
	num_blocks = ceph_fscrypt_blocks(off, len & CEPH_FSCRYPT_BLOCK_MASK);

	 
	for (i = 0; i < num_blocks; ++i) {
		int blkoff = i << CEPH_FSCRYPT_BLOCK_SHIFT;
		int pgidx = blkoff >> PAGE_SHIFT;
		unsigned int pgoffs = offset_in_page(blkoff);
		int fret;

		fret = ceph_fscrypt_decrypt_block_inplace(inode, page[pgidx],
				CEPH_FSCRYPT_BLOCK_SIZE, pgoffs,
				baseblk + i);
		if (fret < 0) {
			if (ret == 0)
				ret = fret;
			break;
		}
		ret += CEPH_FSCRYPT_BLOCK_SIZE;
	}
	return ret;
}

 
int ceph_fscrypt_decrypt_extents(struct inode *inode, struct page **page,
				 u64 off, struct ceph_sparse_extent *map,
				 u32 ext_cnt)
{
	int i, ret = 0;
	struct ceph_inode_info *ci = ceph_inode(inode);
	u64 objno, objoff;
	u32 xlen;

	 
	if (ext_cnt == 0) {
		dout("%s: empty array, ret 0\n", __func__);
		return 0;
	}

	ceph_calc_file_object_mapping(&ci->i_layout, off, map[0].len,
				      &objno, &objoff, &xlen);

	for (i = 0; i < ext_cnt; ++i) {
		struct ceph_sparse_extent *ext = &map[i];
		int pgsoff = ext->off - objoff;
		int pgidx = pgsoff >> PAGE_SHIFT;
		int fret;

		if ((ext->off | ext->len) & ~CEPH_FSCRYPT_BLOCK_MASK) {
			pr_warn("%s: bad encrypted sparse extent idx %d off %llx len %llx\n",
				__func__, i, ext->off, ext->len);
			return -EIO;
		}
		fret = ceph_fscrypt_decrypt_pages(inode, &page[pgidx],
						 off + pgsoff, ext->len);
		dout("%s: [%d] 0x%llx~0x%llx fret %d\n", __func__, i,
				ext->off, ext->len, fret);
		if (fret < 0) {
			if (ret == 0)
				ret = fret;
			break;
		}
		ret = pgsoff + fret;
	}
	dout("%s: ret %d\n", __func__, ret);
	return ret;
}

 
int ceph_fscrypt_encrypt_pages(struct inode *inode, struct page **page, u64 off,
				int len, gfp_t gfp)
{
	int i, num_blocks;
	u64 baseblk = off >> CEPH_FSCRYPT_BLOCK_SHIFT;
	int ret = 0;

	 
	num_blocks = ceph_fscrypt_blocks(off, len & CEPH_FSCRYPT_BLOCK_MASK);

	 
	for (i = 0; i < num_blocks; ++i) {
		int blkoff = i << CEPH_FSCRYPT_BLOCK_SHIFT;
		int pgidx = blkoff >> PAGE_SHIFT;
		unsigned int pgoffs = offset_in_page(blkoff);
		int fret;

		fret = ceph_fscrypt_encrypt_block_inplace(inode, page[pgidx],
				CEPH_FSCRYPT_BLOCK_SIZE, pgoffs,
				baseblk + i, gfp);
		if (fret < 0) {
			if (ret == 0)
				ret = fret;
			break;
		}
		ret += CEPH_FSCRYPT_BLOCK_SIZE;
	}
	return ret;
}
