 
 

#ifndef DM_VERITY_FEC_H
#define DM_VERITY_FEC_H

#include "dm-verity.h"
#include <linux/rslib.h>

 
#define DM_VERITY_FEC_RSM		255
#define DM_VERITY_FEC_MAX_RSN		253
#define DM_VERITY_FEC_MIN_RSN		231	 

 
#define DM_VERITY_FEC_BUF_PREALLOC	1	 
#define DM_VERITY_FEC_BUF_RS_BITS	4	 
 
#define DM_VERITY_FEC_BUF_MAX \
	(1 << (PAGE_SHIFT - DM_VERITY_FEC_BUF_RS_BITS))

 
#define DM_VERITY_FEC_MAX_RECURSION	4

#define DM_VERITY_OPT_FEC_DEV		"use_fec_from_device"
#define DM_VERITY_OPT_FEC_BLOCKS	"fec_blocks"
#define DM_VERITY_OPT_FEC_START		"fec_start"
#define DM_VERITY_OPT_FEC_ROOTS		"fec_roots"

 
struct dm_verity_fec {
	struct dm_dev *dev;	 
	struct dm_bufio_client *data_bufio;	 
	struct dm_bufio_client *bufio;		 
	size_t io_size;		 
	sector_t start;		 
	sector_t blocks;	 
	sector_t rounds;	 
	sector_t hash_blocks;	 
	unsigned char roots;	 
	unsigned char rsn;	 
	mempool_t rs_pool;	 
	mempool_t prealloc_pool;	 
	mempool_t extra_pool;	 
	mempool_t output_pool;	 
	struct kmem_cache *cache;	 
};

 
struct dm_verity_fec_io {
	struct rs_control *rs;	 
	int erasures[DM_VERITY_FEC_MAX_RSN];	 
	u8 *bufs[DM_VERITY_FEC_BUF_MAX];	 
	unsigned int nbufs;		 
	u8 *output;		 
	size_t output_pos;
	unsigned int level;		 
};

#ifdef CONFIG_DM_VERITY_FEC

 
#define DM_VERITY_OPTS_FEC	8

extern bool verity_fec_is_enabled(struct dm_verity *v);

extern int verity_fec_decode(struct dm_verity *v, struct dm_verity_io *io,
			     enum verity_block_type type, sector_t block,
			     u8 *dest, struct bvec_iter *iter);

extern unsigned int verity_fec_status_table(struct dm_verity *v, unsigned int sz,
					char *result, unsigned int maxlen);

extern void verity_fec_finish_io(struct dm_verity_io *io);
extern void verity_fec_init_io(struct dm_verity_io *io);

extern bool verity_is_fec_opt_arg(const char *arg_name);
extern int verity_fec_parse_opt_args(struct dm_arg_set *as,
				     struct dm_verity *v, unsigned int *argc,
				     const char *arg_name);

extern void verity_fec_dtr(struct dm_verity *v);

extern int verity_fec_ctr_alloc(struct dm_verity *v);
extern int verity_fec_ctr(struct dm_verity *v);

#else  

#define DM_VERITY_OPTS_FEC	0

static inline bool verity_fec_is_enabled(struct dm_verity *v)
{
	return false;
}

static inline int verity_fec_decode(struct dm_verity *v,
				    struct dm_verity_io *io,
				    enum verity_block_type type,
				    sector_t block, u8 *dest,
				    struct bvec_iter *iter)
{
	return -EOPNOTSUPP;
}

static inline unsigned int verity_fec_status_table(struct dm_verity *v,
					       unsigned int sz, char *result,
					       unsigned int maxlen)
{
	return sz;
}

static inline void verity_fec_finish_io(struct dm_verity_io *io)
{
}

static inline void verity_fec_init_io(struct dm_verity_io *io)
{
}

static inline bool verity_is_fec_opt_arg(const char *arg_name)
{
	return false;
}

static inline int verity_fec_parse_opt_args(struct dm_arg_set *as,
					    struct dm_verity *v,
					    unsigned int *argc,
					    const char *arg_name)
{
	return -EINVAL;
}

static inline void verity_fec_dtr(struct dm_verity *v)
{
}

static inline int verity_fec_ctr_alloc(struct dm_verity *v)
{
	return 0;
}

static inline int verity_fec_ctr(struct dm_verity *v)
{
	return 0;
}

#endif  

#endif  
