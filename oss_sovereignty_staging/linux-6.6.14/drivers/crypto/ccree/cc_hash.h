 
 

 

#ifndef __CC_HASH_H__
#define __CC_HASH_H__

#include "cc_buffer_mgr.h"

#define HMAC_IPAD_CONST	0x36363636
#define HMAC_OPAD_CONST	0x5C5C5C5C
#define HASH_LEN_SIZE_712 16
#define HASH_LEN_SIZE_630 8
#define HASH_MAX_LEN_SIZE HASH_LEN_SIZE_712
#define CC_MAX_HASH_DIGEST_SIZE	SHA512_DIGEST_SIZE
#define CC_MAX_HASH_BLCK_SIZE SHA512_BLOCK_SIZE

#define XCBC_MAC_K1_OFFSET 0
#define XCBC_MAC_K2_OFFSET 16
#define XCBC_MAC_K3_OFFSET 32

#define CC_EXPORT_MAGIC 0xC2EE1070U

 
struct aeshash_state {
	u8 state[AES_BLOCK_SIZE];
	unsigned int count;
	u8 buffer[AES_BLOCK_SIZE];
};

 
struct ahash_req_ctx {
	u8 buffers[2][CC_MAX_HASH_BLCK_SIZE] ____cacheline_aligned;
	u8 digest_result_buff[CC_MAX_HASH_DIGEST_SIZE] ____cacheline_aligned;
	u8 digest_buff[CC_MAX_HASH_DIGEST_SIZE] ____cacheline_aligned;
	u8 opad_digest_buff[CC_MAX_HASH_DIGEST_SIZE] ____cacheline_aligned;
	u8 digest_bytes_len[HASH_MAX_LEN_SIZE] ____cacheline_aligned;
	struct async_gen_req_ctx gen_ctx ____cacheline_aligned;
	enum cc_req_dma_buf_type data_dma_buf_type;
	dma_addr_t opad_digest_dma_addr;
	dma_addr_t digest_buff_dma_addr;
	dma_addr_t digest_bytes_len_dma_addr;
	dma_addr_t digest_result_dma_addr;
	u32 buf_cnt[2];
	u32 buff_index;
	u32 xcbc_count;  
	struct scatterlist buff_sg[2];
	struct scatterlist *curr_sg;
	u32 in_nents;
	u32 mlli_nents;
	struct mlli_params mlli_params;
};

static inline u32 *cc_hash_buf_cnt(struct ahash_req_ctx *state)
{
	return &state->buf_cnt[state->buff_index];
}

static inline u8 *cc_hash_buf(struct ahash_req_ctx *state)
{
	return state->buffers[state->buff_index];
}

static inline u32 *cc_next_buf_cnt(struct ahash_req_ctx *state)
{
	return &state->buf_cnt[state->buff_index ^ 1];
}

static inline u8 *cc_next_buf(struct ahash_req_ctx *state)
{
	return state->buffers[state->buff_index ^ 1];
}

int cc_hash_alloc(struct cc_drvdata *drvdata);
int cc_init_hash_sram(struct cc_drvdata *drvdata);
int cc_hash_free(struct cc_drvdata *drvdata);

 
u32 cc_digest_len_addr(void *drvdata, u32 mode);

 
u32 cc_larval_digest_addr(void *drvdata, u32 mode);

#endif  
