
 

 

#include <linux/blk-crypto.h>
#include <linux/blkdev.h>
#include <linux/buffer_head.h>
#include <linux/sched/mm.h>
#include <linux/slab.h>
#include <linux/uio.h>

#include "fscrypt_private.h"

static struct block_device **fscrypt_get_devices(struct super_block *sb,
						 unsigned int *num_devs)
{
	struct block_device **devs;

	if (sb->s_cop->get_devices) {
		devs = sb->s_cop->get_devices(sb, num_devs);
		if (devs)
			return devs;
	}
	devs = kmalloc(sizeof(*devs), GFP_KERNEL);
	if (!devs)
		return ERR_PTR(-ENOMEM);
	devs[0] = sb->s_bdev;
	*num_devs = 1;
	return devs;
}

static unsigned int fscrypt_get_dun_bytes(const struct fscrypt_info *ci)
{
	struct super_block *sb = ci->ci_inode->i_sb;
	unsigned int flags = fscrypt_policy_flags(&ci->ci_policy);
	int ino_bits = 64, lblk_bits = 64;

	if (flags & FSCRYPT_POLICY_FLAG_DIRECT_KEY)
		return offsetofend(union fscrypt_iv, nonce);

	if (flags & FSCRYPT_POLICY_FLAG_IV_INO_LBLK_64)
		return sizeof(__le64);

	if (flags & FSCRYPT_POLICY_FLAG_IV_INO_LBLK_32)
		return sizeof(__le32);

	 
	if (sb->s_cop->get_ino_and_lblk_bits)
		sb->s_cop->get_ino_and_lblk_bits(sb, &ino_bits, &lblk_bits);
	return DIV_ROUND_UP(lblk_bits, 8);
}

 
static void fscrypt_log_blk_crypto_impl(struct fscrypt_mode *mode,
					struct block_device **devs,
					unsigned int num_devs,
					const struct blk_crypto_config *cfg)
{
	unsigned int i;

	for (i = 0; i < num_devs; i++) {
		if (!IS_ENABLED(CONFIG_BLK_INLINE_ENCRYPTION_FALLBACK) ||
		    blk_crypto_config_supported_natively(devs[i], cfg)) {
			if (!xchg(&mode->logged_blk_crypto_native, 1))
				pr_info("fscrypt: %s using blk-crypto (native)\n",
					mode->friendly_name);
		} else if (!xchg(&mode->logged_blk_crypto_fallback, 1)) {
			pr_info("fscrypt: %s using blk-crypto-fallback\n",
				mode->friendly_name);
		}
	}
}

 
int fscrypt_select_encryption_impl(struct fscrypt_info *ci)
{
	const struct inode *inode = ci->ci_inode;
	struct super_block *sb = inode->i_sb;
	struct blk_crypto_config crypto_cfg;
	struct block_device **devs;
	unsigned int num_devs;
	unsigned int i;

	 
	if (!S_ISREG(inode->i_mode))
		return 0;

	 
	if (ci->ci_mode->blk_crypto_mode == BLK_ENCRYPTION_MODE_INVALID)
		return 0;

	 
	if (!(sb->s_flags & SB_INLINECRYPT))
		return 0;

	 
	if ((fscrypt_policy_flags(&ci->ci_policy) &
	     FSCRYPT_POLICY_FLAG_IV_INO_LBLK_32) &&
	    sb->s_blocksize != PAGE_SIZE)
		return 0;

	 
	crypto_cfg.crypto_mode = ci->ci_mode->blk_crypto_mode;
	crypto_cfg.data_unit_size = sb->s_blocksize;
	crypto_cfg.dun_bytes = fscrypt_get_dun_bytes(ci);

	devs = fscrypt_get_devices(sb, &num_devs);
	if (IS_ERR(devs))
		return PTR_ERR(devs);

	for (i = 0; i < num_devs; i++) {
		if (!blk_crypto_config_supported(devs[i], &crypto_cfg))
			goto out_free_devs;
	}

	fscrypt_log_blk_crypto_impl(ci->ci_mode, devs, num_devs, &crypto_cfg);

	ci->ci_inlinecrypt = true;
out_free_devs:
	kfree(devs);

	return 0;
}

int fscrypt_prepare_inline_crypt_key(struct fscrypt_prepared_key *prep_key,
				     const u8 *raw_key,
				     const struct fscrypt_info *ci)
{
	const struct inode *inode = ci->ci_inode;
	struct super_block *sb = inode->i_sb;
	enum blk_crypto_mode_num crypto_mode = ci->ci_mode->blk_crypto_mode;
	struct blk_crypto_key *blk_key;
	struct block_device **devs;
	unsigned int num_devs;
	unsigned int i;
	int err;

	blk_key = kmalloc(sizeof(*blk_key), GFP_KERNEL);
	if (!blk_key)
		return -ENOMEM;

	err = blk_crypto_init_key(blk_key, raw_key, crypto_mode,
				  fscrypt_get_dun_bytes(ci), sb->s_blocksize);
	if (err) {
		fscrypt_err(inode, "error %d initializing blk-crypto key", err);
		goto fail;
	}

	 
	devs = fscrypt_get_devices(sb, &num_devs);
	if (IS_ERR(devs)) {
		err = PTR_ERR(devs);
		goto fail;
	}
	for (i = 0; i < num_devs; i++) {
		err = blk_crypto_start_using_key(devs[i], blk_key);
		if (err)
			break;
	}
	kfree(devs);
	if (err) {
		fscrypt_err(inode, "error %d starting to use blk-crypto", err);
		goto fail;
	}

	 
	smp_store_release(&prep_key->blk_key, blk_key);
	return 0;

fail:
	kfree_sensitive(blk_key);
	return err;
}

void fscrypt_destroy_inline_crypt_key(struct super_block *sb,
				      struct fscrypt_prepared_key *prep_key)
{
	struct blk_crypto_key *blk_key = prep_key->blk_key;
	struct block_device **devs;
	unsigned int num_devs;
	unsigned int i;

	if (!blk_key)
		return;

	 
	devs = fscrypt_get_devices(sb, &num_devs);
	if (!IS_ERR(devs)) {
		for (i = 0; i < num_devs; i++)
			blk_crypto_evict_key(devs[i], blk_key);
		kfree(devs);
	}
	kfree_sensitive(blk_key);
}

bool __fscrypt_inode_uses_inline_crypto(const struct inode *inode)
{
	return inode->i_crypt_info->ci_inlinecrypt;
}
EXPORT_SYMBOL_GPL(__fscrypt_inode_uses_inline_crypto);

static void fscrypt_generate_dun(const struct fscrypt_info *ci, u64 lblk_num,
				 u64 dun[BLK_CRYPTO_DUN_ARRAY_SIZE])
{
	union fscrypt_iv iv;
	int i;

	fscrypt_generate_iv(&iv, lblk_num, ci);

	BUILD_BUG_ON(FSCRYPT_MAX_IV_SIZE > BLK_CRYPTO_MAX_IV_SIZE);
	memset(dun, 0, BLK_CRYPTO_MAX_IV_SIZE);
	for (i = 0; i < ci->ci_mode->ivsize/sizeof(dun[0]); i++)
		dun[i] = le64_to_cpu(iv.dun[i]);
}

 
void fscrypt_set_bio_crypt_ctx(struct bio *bio, const struct inode *inode,
			       u64 first_lblk, gfp_t gfp_mask)
{
	const struct fscrypt_info *ci;
	u64 dun[BLK_CRYPTO_DUN_ARRAY_SIZE];

	if (!fscrypt_inode_uses_inline_crypto(inode))
		return;
	ci = inode->i_crypt_info;

	fscrypt_generate_dun(ci, first_lblk, dun);
	bio_crypt_set_ctx(bio, ci->ci_enc_key.blk_key, dun, gfp_mask);
}
EXPORT_SYMBOL_GPL(fscrypt_set_bio_crypt_ctx);

 
static bool bh_get_inode_and_lblk_num(const struct buffer_head *bh,
				      const struct inode **inode_ret,
				      u64 *lblk_num_ret)
{
	struct page *page = bh->b_page;
	const struct address_space *mapping;
	const struct inode *inode;

	 
	mapping = page_mapping(page);
	if (!mapping)
		return false;
	inode = mapping->host;

	*inode_ret = inode;
	*lblk_num_ret = ((u64)page->index << (PAGE_SHIFT - inode->i_blkbits)) +
			(bh_offset(bh) >> inode->i_blkbits);
	return true;
}

 
void fscrypt_set_bio_crypt_ctx_bh(struct bio *bio,
				  const struct buffer_head *first_bh,
				  gfp_t gfp_mask)
{
	const struct inode *inode;
	u64 first_lblk;

	if (bh_get_inode_and_lblk_num(first_bh, &inode, &first_lblk))
		fscrypt_set_bio_crypt_ctx(bio, inode, first_lblk, gfp_mask);
}
EXPORT_SYMBOL_GPL(fscrypt_set_bio_crypt_ctx_bh);

 
bool fscrypt_mergeable_bio(struct bio *bio, const struct inode *inode,
			   u64 next_lblk)
{
	const struct bio_crypt_ctx *bc = bio->bi_crypt_context;
	u64 next_dun[BLK_CRYPTO_DUN_ARRAY_SIZE];

	if (!!bc != fscrypt_inode_uses_inline_crypto(inode))
		return false;
	if (!bc)
		return true;

	 
	if (bc->bc_key != inode->i_crypt_info->ci_enc_key.blk_key)
		return false;

	fscrypt_generate_dun(inode->i_crypt_info, next_lblk, next_dun);
	return bio_crypt_dun_is_contiguous(bc, bio->bi_iter.bi_size, next_dun);
}
EXPORT_SYMBOL_GPL(fscrypt_mergeable_bio);

 
bool fscrypt_mergeable_bio_bh(struct bio *bio,
			      const struct buffer_head *next_bh)
{
	const struct inode *inode;
	u64 next_lblk;

	if (!bh_get_inode_and_lblk_num(next_bh, &inode, &next_lblk))
		return !bio->bi_crypt_context;

	return fscrypt_mergeable_bio(bio, inode, next_lblk);
}
EXPORT_SYMBOL_GPL(fscrypt_mergeable_bio_bh);

 
bool fscrypt_dio_supported(struct inode *inode)
{
	int err;

	 
	if (!fscrypt_needs_contents_encryption(inode))
		return true;

	 
	err = fscrypt_require_key(inode);
	if (err) {
		 
		return false;
	}
	return fscrypt_inode_uses_inline_crypto(inode);
}
EXPORT_SYMBOL_GPL(fscrypt_dio_supported);

 
u64 fscrypt_limit_io_blocks(const struct inode *inode, u64 lblk, u64 nr_blocks)
{
	const struct fscrypt_info *ci;
	u32 dun;

	if (!fscrypt_inode_uses_inline_crypto(inode))
		return nr_blocks;

	if (nr_blocks <= 1)
		return nr_blocks;

	ci = inode->i_crypt_info;
	if (!(fscrypt_policy_flags(&ci->ci_policy) &
	      FSCRYPT_POLICY_FLAG_IV_INO_LBLK_32))
		return nr_blocks;

	 

	dun = ci->ci_hashed_ino + lblk;

	return min_t(u64, nr_blocks, (u64)U32_MAX + 1 - dun);
}
EXPORT_SYMBOL_GPL(fscrypt_limit_io_blocks);
