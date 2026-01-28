#ifndef __CC_AEAD_H__
#define __CC_AEAD_H__
#include <linux/kernel.h>
#include <crypto/algapi.h>
#include <crypto/ctr.h>
#define ICV_CMP_SIZE 8
#define CCM_CONFIG_BUF_SIZE (AES_BLOCK_SIZE * 3)
#define MAX_MAC_SIZE SHA256_DIGEST_SIZE
#define GCM_BLOCK_LEN_SIZE 8
#define GCM_BLOCK_RFC4_IV_OFFSET	4
#define GCM_BLOCK_RFC4_IV_SIZE		8   
#define GCM_BLOCK_RFC4_NONCE_OFFSET	0
#define GCM_BLOCK_RFC4_NONCE_SIZE	4
#define CCM_B0_OFFSET 0
#define CCM_A0_OFFSET 16
#define CCM_CTR_COUNT_0_OFFSET 32
#define CCM_BLOCK_NONCE_OFFSET 1   
#define CCM_BLOCK_NONCE_SIZE   3   
#define CCM_BLOCK_IV_OFFSET    4   
#define CCM_BLOCK_IV_SIZE      8   
enum aead_ccm_header_size {
	ccm_header_size_null = -1,
	ccm_header_size_zero = 0,
	ccm_header_size_2 = 2,
	ccm_header_size_6 = 6,
	ccm_header_size_max = S32_MAX
};
struct aead_req_ctx {
	u8 mac_buf[MAX_MAC_SIZE] ____cacheline_aligned;
	u8 ctr_iv[AES_BLOCK_SIZE] ____cacheline_aligned;
	u8 gcm_iv_inc1[AES_BLOCK_SIZE] ____cacheline_aligned;
	u8 gcm_iv_inc2[AES_BLOCK_SIZE] ____cacheline_aligned;
	u8 hkey[AES_BLOCK_SIZE] ____cacheline_aligned;
	struct {
		u8 len_a[GCM_BLOCK_LEN_SIZE] ____cacheline_aligned;
		u8 len_c[GCM_BLOCK_LEN_SIZE];
	} gcm_len_block;
	u8 ccm_config[CCM_CONFIG_BUF_SIZE] ____cacheline_aligned;
	unsigned int hw_iv_size ____cacheline_aligned;
	u8 backup_mac[MAX_MAC_SIZE];
	u8 *backup_iv;  
	u32 assoclen;  
	dma_addr_t mac_buf_dma_addr;  
	dma_addr_t ccm_iv0_dma_addr;
	dma_addr_t icv_dma_addr;  
	dma_addr_t gcm_iv_inc1_dma_addr;
	dma_addr_t gcm_iv_inc2_dma_addr;
	dma_addr_t hkey_dma_addr;  
	dma_addr_t gcm_block_len_dma_addr;  
	u8 *icv_virt_addr;  
	struct async_gen_req_ctx gen_ctx;
	struct cc_mlli assoc;
	struct cc_mlli src;
	struct cc_mlli dst;
	struct scatterlist *src_sgl;
	struct scatterlist *dst_sgl;
	unsigned int src_offset;
	unsigned int dst_offset;
	enum cc_req_dma_buf_type assoc_buff_type;
	enum cc_req_dma_buf_type data_buff_type;
	struct mlli_params mlli_params;
	unsigned int cryptlen;
	struct scatterlist ccm_adata_sg;
	enum aead_ccm_header_size ccm_hdr_size;
	unsigned int req_authsize;
	enum drv_cipher_mode cipher_mode;
	bool is_icv_fragmented;
	bool is_single_pass;
	bool plaintext_authenticate_only;  
};
int cc_aead_alloc(struct cc_drvdata *drvdata);
int cc_aead_free(struct cc_drvdata *drvdata);
#endif  
