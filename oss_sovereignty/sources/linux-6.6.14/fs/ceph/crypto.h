


#ifndef _CEPH_CRYPTO_H
#define _CEPH_CRYPTO_H

#include <crypto/sha2.h>
#include <linux/fscrypt.h>

#define CEPH_FSCRYPT_BLOCK_SHIFT   12
#define CEPH_FSCRYPT_BLOCK_SIZE    (_AC(1, UL) << CEPH_FSCRYPT_BLOCK_SHIFT)
#define CEPH_FSCRYPT_BLOCK_MASK	   (~(CEPH_FSCRYPT_BLOCK_SIZE-1))

struct ceph_fs_client;
struct ceph_acl_sec_ctx;
struct ceph_mds_request;

struct ceph_fname {
	struct inode	*dir;
	char		*name;		
	unsigned char	*ctext;		
	u32		name_len;	
	u32		ctext_len;	
	bool		no_copy;
};


struct ceph_fscrypt_truncate_size_header {
	__u8  ver;
	__u8  compat;

	
	__le32 data_len;

	__le64 change_attr;
	__le64 file_offset;
	__le32 block_size;
} __packed;

struct ceph_fscrypt_auth {
	__le32	cfa_version;
	__le32	cfa_blob_len;
	u8	cfa_blob[FSCRYPT_SET_CONTEXT_MAX_SIZE];
} __packed;

#define CEPH_FSCRYPT_AUTH_VERSION	1
static inline u32 ceph_fscrypt_auth_len(struct ceph_fscrypt_auth *fa)
{
	u32 ctxsize = le32_to_cpu(fa->cfa_blob_len);

	return offsetof(struct ceph_fscrypt_auth, cfa_blob) + ctxsize;
}

#ifdef CONFIG_FS_ENCRYPTION

#define CEPH_NOHASH_NAME_MAX (180 - SHA256_DIGEST_SIZE)

#define CEPH_BASE64_CHARS(nbytes) DIV_ROUND_UP((nbytes) * 4, 3)

int ceph_base64_encode(const u8 *src, int srclen, char *dst);
int ceph_base64_decode(const char *src, int srclen, u8 *dst);

void ceph_fscrypt_set_ops(struct super_block *sb);

void ceph_fscrypt_free_dummy_policy(struct ceph_fs_client *fsc);

int ceph_fscrypt_prepare_context(struct inode *dir, struct inode *inode,
				 struct ceph_acl_sec_ctx *as);
void ceph_fscrypt_as_ctx_to_req(struct ceph_mds_request *req,
				struct ceph_acl_sec_ctx *as);
int ceph_encode_encrypted_dname(struct inode *parent, struct qstr *d_name,
				char *buf);
int ceph_encode_encrypted_fname(struct inode *parent, struct dentry *dentry,
				char *buf);

static inline int ceph_fname_alloc_buffer(struct inode *parent,
					  struct fscrypt_str *fname)
{
	if (!IS_ENCRYPTED(parent))
		return 0;
	return fscrypt_fname_alloc_buffer(NAME_MAX, fname);
}

static inline void ceph_fname_free_buffer(struct inode *parent,
					  struct fscrypt_str *fname)
{
	if (IS_ENCRYPTED(parent))
		fscrypt_fname_free_buffer(fname);
}

int ceph_fname_to_usr(const struct ceph_fname *fname, struct fscrypt_str *tname,
		      struct fscrypt_str *oname, bool *is_nokey);
int ceph_fscrypt_prepare_readdir(struct inode *dir);

static inline unsigned int ceph_fscrypt_blocks(u64 off, u64 len)
{
	
	BUILD_BUG_ON(CEPH_FSCRYPT_BLOCK_SHIFT > PAGE_SHIFT);

	return ((off+len+CEPH_FSCRYPT_BLOCK_SIZE-1) >> CEPH_FSCRYPT_BLOCK_SHIFT) -
		(off >> CEPH_FSCRYPT_BLOCK_SHIFT);
}


static inline void ceph_fscrypt_adjust_off_and_len(struct inode *inode,
						   u64 *off, u64 *len)
{
	if (IS_ENCRYPTED(inode)) {
		*len = ceph_fscrypt_blocks(*off, *len) * CEPH_FSCRYPT_BLOCK_SIZE;
		*off &= CEPH_FSCRYPT_BLOCK_MASK;
	}
}

int ceph_fscrypt_decrypt_block_inplace(const struct inode *inode,
				  struct page *page, unsigned int len,
				  unsigned int offs, u64 lblk_num);
int ceph_fscrypt_encrypt_block_inplace(const struct inode *inode,
				  struct page *page, unsigned int len,
				  unsigned int offs, u64 lblk_num,
				  gfp_t gfp_flags);
int ceph_fscrypt_decrypt_pages(struct inode *inode, struct page **page,
			       u64 off, int len);
int ceph_fscrypt_decrypt_extents(struct inode *inode, struct page **page,
				 u64 off, struct ceph_sparse_extent *map,
				 u32 ext_cnt);
int ceph_fscrypt_encrypt_pages(struct inode *inode, struct page **page, u64 off,
			       int len, gfp_t gfp);

static inline struct page *ceph_fscrypt_pagecache_page(struct page *page)
{
	return fscrypt_is_bounce_page(page) ? fscrypt_pagecache_page(page) : page;
}

#else 

static inline void ceph_fscrypt_set_ops(struct super_block *sb)
{
}

static inline void ceph_fscrypt_free_dummy_policy(struct ceph_fs_client *fsc)
{
}

static inline int ceph_fscrypt_prepare_context(struct inode *dir,
					       struct inode *inode,
					       struct ceph_acl_sec_ctx *as)
{
	if (IS_ENCRYPTED(dir))
		return -EOPNOTSUPP;
	return 0;
}

static inline void ceph_fscrypt_as_ctx_to_req(struct ceph_mds_request *req,
						struct ceph_acl_sec_ctx *as_ctx)
{
}

static inline int ceph_encode_encrypted_dname(struct inode *parent,
					      struct qstr *d_name, char *buf)
{
	memcpy(buf, d_name->name, d_name->len);
	return d_name->len;
}

static inline int ceph_encode_encrypted_fname(struct inode *parent,
					      struct dentry *dentry, char *buf)
{
	return -EOPNOTSUPP;
}

static inline int ceph_fname_alloc_buffer(struct inode *parent,
					  struct fscrypt_str *fname)
{
	return 0;
}

static inline void ceph_fname_free_buffer(struct inode *parent,
					  struct fscrypt_str *fname)
{
}

static inline int ceph_fname_to_usr(const struct ceph_fname *fname,
				    struct fscrypt_str *tname,
				    struct fscrypt_str *oname, bool *is_nokey)
{
	oname->name = fname->name;
	oname->len = fname->name_len;
	return 0;
}

static inline int ceph_fscrypt_prepare_readdir(struct inode *dir)
{
	return 0;
}

static inline void ceph_fscrypt_adjust_off_and_len(struct inode *inode,
						   u64 *off, u64 *len)
{
}

static inline int ceph_fscrypt_decrypt_block_inplace(const struct inode *inode,
					  struct page *page, unsigned int len,
					  unsigned int offs, u64 lblk_num)
{
	return 0;
}

static inline int ceph_fscrypt_encrypt_block_inplace(const struct inode *inode,
					  struct page *page, unsigned int len,
					  unsigned int offs, u64 lblk_num,
					  gfp_t gfp_flags)
{
	return 0;
}

static inline int ceph_fscrypt_decrypt_pages(struct inode *inode,
					     struct page **page, u64 off,
					     int len)
{
	return 0;
}

static inline int ceph_fscrypt_decrypt_extents(struct inode *inode,
					       struct page **page, u64 off,
					       struct ceph_sparse_extent *map,
					       u32 ext_cnt)
{
	return 0;
}

static inline int ceph_fscrypt_encrypt_pages(struct inode *inode,
					     struct page **page, u64 off,
					     int len, gfp_t gfp)
{
	return 0;
}

static inline struct page *ceph_fscrypt_pagecache_page(struct page *page)
{
	return page;
}
#endif 

static inline loff_t ceph_fscrypt_page_offset(struct page *page)
{
	return page_offset(ceph_fscrypt_pagecache_page(page));
}

#endif 
