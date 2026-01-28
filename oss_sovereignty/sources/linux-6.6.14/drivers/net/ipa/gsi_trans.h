


#ifndef _GSI_TRANS_H_
#define _GSI_TRANS_H_

#include <linux/types.h>
#include <linux/refcount.h>
#include <linux/completion.h>
#include <linux/dma-direction.h>

#include "ipa_cmd.h"

struct page;
struct scatterlist;
struct device;
struct sk_buff;

struct gsi;
struct gsi_trans;
struct gsi_trans_pool;


#define IPA_COMMAND_TRANS_TRE_MAX	8


struct gsi_trans {
	struct gsi *gsi;
	u8 channel_id;

	bool cancelled;			

	u8 rsvd_count;			
	u8 used_count;			
	u32 len;			

	union {
		void *data;
		u8 cmd_opcode[IPA_COMMAND_TRANS_TRE_MAX];
	};
	struct scatterlist *sgl;
	enum dma_data_direction direction;

	refcount_t refcount;
	struct completion completion;

	u64 byte_count;			
	u64 trans_count;		
};


int gsi_trans_pool_init(struct gsi_trans_pool *pool, size_t size, u32 count,
			u32 max_alloc);


void *gsi_trans_pool_alloc(struct gsi_trans_pool *pool, u32 count);


void gsi_trans_pool_exit(struct gsi_trans_pool *pool);


int gsi_trans_pool_init_dma(struct device *dev, struct gsi_trans_pool *pool,
			    size_t size, u32 count, u32 max_alloc);


void *gsi_trans_pool_alloc_dma(struct gsi_trans_pool *pool, dma_addr_t *addr);


void gsi_trans_pool_exit_dma(struct device *dev, struct gsi_trans_pool *pool);


bool gsi_channel_trans_idle(struct gsi *gsi, u32 channel_id);


struct gsi_trans *gsi_channel_trans_alloc(struct gsi *gsi, u32 channel_id,
					  u32 tre_count,
					  enum dma_data_direction direction);


void gsi_trans_free(struct gsi_trans *trans);


void gsi_trans_cmd_add(struct gsi_trans *trans, void *buf, u32 size,
		       dma_addr_t addr, enum ipa_cmd_opcode opcode);


int gsi_trans_page_add(struct gsi_trans *trans, struct page *page, u32 size,
		       u32 offset);


int gsi_trans_skb_add(struct gsi_trans *trans, struct sk_buff *skb);


void gsi_trans_commit(struct gsi_trans *trans, bool ring_db);


void gsi_trans_commit_wait(struct gsi_trans *trans);


int gsi_trans_read_byte(struct gsi *gsi, u32 channel_id, dma_addr_t addr);


void gsi_trans_read_byte_done(struct gsi *gsi, u32 channel_id);

#endif 
