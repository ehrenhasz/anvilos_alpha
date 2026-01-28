#ifndef DM_VERITY_H
#define DM_VERITY_H
#include <linux/dm-bufio.h>
#include <linux/device-mapper.h>
#include <linux/interrupt.h>
#include <crypto/hash.h>
#define DM_VERITY_MAX_LEVELS		63
enum verity_mode {
	DM_VERITY_MODE_EIO,
	DM_VERITY_MODE_LOGGING,
	DM_VERITY_MODE_RESTART,
	DM_VERITY_MODE_PANIC
};
enum verity_block_type {
	DM_VERITY_BLOCK_TYPE_DATA,
	DM_VERITY_BLOCK_TYPE_METADATA
};
struct dm_verity_fec;
struct dm_verity {
	struct dm_dev *data_dev;
	struct dm_dev *hash_dev;
	struct dm_target *ti;
	struct dm_bufio_client *bufio;
	char *alg_name;
	struct crypto_ahash *tfm;
	u8 *root_digest;	 
	u8 *salt;		 
	u8 *zero_digest;	 
	unsigned int salt_size;
	sector_t data_start;	 
	sector_t hash_start;	 
	sector_t data_blocks;	 
	sector_t hash_blocks;	 
	unsigned char data_dev_block_bits;	 
	unsigned char hash_dev_block_bits;	 
	unsigned char hash_per_block_bits;	 
	unsigned char levels;	 
	unsigned char version;
	bool hash_failed:1;	 
	bool use_tasklet:1;	 
	unsigned int digest_size;	 
	unsigned int ahash_reqsize; 
	enum verity_mode mode;	 
	unsigned int corrupted_errs; 
	struct workqueue_struct *verify_wq;
	sector_t hash_level_block[DM_VERITY_MAX_LEVELS];
	struct dm_verity_fec *fec;	 
	unsigned long *validated_blocks;  
	char *signature_key_desc;  
};
struct dm_verity_io {
	struct dm_verity *v;
	bio_end_io_t *orig_bi_end_io;
	sector_t block;
	unsigned int n_blocks;
	bool in_tasklet;
	struct bvec_iter iter;
	struct work_struct work;
	struct tasklet_struct tasklet;
};
static inline struct ahash_request *verity_io_hash_req(struct dm_verity *v,
						     struct dm_verity_io *io)
{
	return (struct ahash_request *)(io + 1);
}
static inline u8 *verity_io_real_digest(struct dm_verity *v,
					struct dm_verity_io *io)
{
	return (u8 *)(io + 1) + v->ahash_reqsize;
}
static inline u8 *verity_io_want_digest(struct dm_verity *v,
					struct dm_verity_io *io)
{
	return (u8 *)(io + 1) + v->ahash_reqsize + v->digest_size;
}
extern int verity_for_bv_block(struct dm_verity *v, struct dm_verity_io *io,
			       struct bvec_iter *iter,
			       int (*process)(struct dm_verity *v,
					      struct dm_verity_io *io,
					      u8 *data, size_t len));
extern int verity_hash(struct dm_verity *v, struct ahash_request *req,
		       const u8 *data, size_t len, u8 *digest, bool may_sleep);
extern int verity_hash_for_block(struct dm_verity *v, struct dm_verity_io *io,
				 sector_t block, u8 *digest, bool *is_zero);
extern bool dm_is_verity_target(struct dm_target *ti);
extern int dm_verity_get_mode(struct dm_target *ti);
extern int dm_verity_get_root_digest(struct dm_target *ti, u8 **root_digest,
				     unsigned int *digest_size);
#endif  
