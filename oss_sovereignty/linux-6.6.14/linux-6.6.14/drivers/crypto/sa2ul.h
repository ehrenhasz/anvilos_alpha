#ifndef _K3_SA2UL_
#define _K3_SA2UL_
#include <crypto/aes.h>
#include <crypto/sha1.h>
#include <crypto/sha2.h>
#define SA_ENGINE_STATUS		0x0008
#define SA_ENGINE_ENABLE_CONTROL	0x1000
struct sa_tfm_ctx;
#define SA_EEC_ENCSS_EN			0x00000001
#define SA_EEC_AUTHSS_EN		0x00000002
#define SA_EEC_TRNG_EN			0x00000008
#define SA_EEC_PKA_EN			0x00000010
#define SA_EEC_CTXCACH_EN		0x00000080
#define SA_EEC_CPPI_PORT_IN_EN		0x00000200
#define SA_EEC_CPPI_PORT_OUT_EN		0x00000800
#define SA_REQ_SUBTYPE_ENC	0x0001
#define SA_REQ_SUBTYPE_DEC	0x0002
#define SA_REQ_SUBTYPE_SHIFT	16
#define SA_REQ_SUBTYPE_MASK	0xffff
#define SA_DMA_NUM_EPIB_WORDS   4
#define SA_DMA_NUM_PS_WORDS     16
#define NKEY_SZ			3
#define MCI_SZ			27
#define SA_MAX_NUM_CTX	512
#define SA_CTX_SIZE_TO_DMA_SIZE(ctx_sz) \
		((ctx_sz) ? ((ctx_sz) / 32 - 1) : 0)
#define SA_CTX_ENC_KEY_OFFSET   32
#define SA_CTX_ENC_AUX1_OFFSET  64
#define SA_CTX_ENC_AUX2_OFFSET  96
#define SA_CTX_ENC_AUX3_OFFSET  112
#define SA_CTX_ENC_AUX4_OFFSET  128
#define SA_ENG_ID_EM1   2        
#define SA_ENG_ID_EM2   3        
#define SA_ENG_ID_AM1   4        
#define SA_ENG_ID_AM2   5        
#define SA_ENG_ID_OUTPORT2 20    
#define SA_CMDL_OFFSET_NESC           0       
#define SA_CMDL_OFFSET_LABEL_LEN      1       
#define SA_CMDL_OFFSET_DATA_LEN       2
#define SA_CMDL_OFFSET_DATA_OFFSET    4       
#define SA_CMDL_OFFSET_OPTION_CTRL1   5       
#define SA_CMDL_OFFSET_OPTION_CTRL2   6       
#define SA_CMDL_OFFSET_OPTION_CTRL3   7       
#define SA_CMDL_OFFSET_OPTION_BYTE    8
#define SA_CMDL_HEADER_SIZE_BYTES	8
#define SA_CMDL_OPTION_BYTES_MAX_SIZE     72
#define SA_CMDL_MAX_SIZE_BYTES (SA_CMDL_HEADER_SIZE_BYTES + \
				SA_CMDL_OPTION_BYTES_MAX_SIZE)
#define SA_SW_INFO_FLAG_EVICT   0x0001
#define SA_SW_INFO_FLAG_TEAR    0x0002
#define SA_SW_INFO_FLAG_NOPD    0x0004
#define SA_CTX_PE_PKT_TYPE_3GPP_AIR    0     
#define SA_CTX_PE_PKT_TYPE_SRTP        1     
#define SA_CTX_PE_PKT_TYPE_IPSEC_AH    2     
#define SA_CTX_PE_PKT_TYPE_IPSEC_ESP   3
#define SA_CTX_PE_PKT_TYPE_NONE        4
#define SA_CTX_ENC_TYPE1_SZ     64       
#define SA_CTX_ENC_TYPE2_SZ     96       
#define SA_CTX_AUTH_TYPE1_SZ    64       
#define SA_CTX_AUTH_TYPE2_SZ    96       
#define SA_CTX_PHP_PE_CTX_SZ    64
#define SA_CTX_MAX_SZ (64 + SA_CTX_ENC_TYPE2_SZ + SA_CTX_AUTH_TYPE2_SZ)
#define SA_CTX_DMA_SIZE_0       0
#define SA_CTX_DMA_SIZE_64      1
#define SA_CTX_DMA_SIZE_96      2
#define SA_CTX_DMA_SIZE_128     3
#define SA_CTX_SCCTL_OWNER_OFFSET 0
#define SA_CTX_ENC_KEY_OFFSET   32
#define SA_CTX_ENC_AUX1_OFFSET  64
#define SA_CTX_ENC_AUX2_OFFSET  96
#define SA_CTX_ENC_AUX3_OFFSET  112
#define SA_CTX_ENC_AUX4_OFFSET  128
#define SA_SCCTL_FE_AUTH_ENC	0x65
#define SA_SCCTL_FE_ENC		0x8D
#define SA_ALIGN_MASK		(sizeof(u32) - 1)
#define SA_ALIGNED		__aligned(32)
#define SA_AUTH_SW_CTRL_MD5	1
#define SA_AUTH_SW_CTRL_SHA1	2
#define SA_AUTH_SW_CTRL_SHA224	3
#define SA_AUTH_SW_CTRL_SHA256	4
#define SA_AUTH_SW_CTRL_SHA384	5
#define SA_AUTH_SW_CTRL_SHA512	6
#define SA_MAX_DATA_SZ		U16_MAX
#define SA_UNSAFE_DATA_SZ_MIN	240
#define SA_UNSAFE_DATA_SZ_MAX	255
struct sa_match_data;
struct sa_crypto_data {
	void __iomem *base;
	const struct sa_match_data *match_data;
	struct platform_device	*pdev;
	struct dma_pool		*sc_pool;
	struct device *dev;
	spinlock_t	scid_lock;  
	u16		sc_id_start;
	u16		sc_id_end;
	u16		sc_id;
	unsigned long	ctx_bm[DIV_ROUND_UP(SA_MAX_NUM_CTX,
				BITS_PER_LONG)];
	struct sa_tfm_ctx	*ctx;
	struct dma_chan		*dma_rx1;
	struct dma_chan		*dma_rx2;
	struct dma_chan		*dma_tx;
};
struct sa_cmdl_param_info {
	u16	index;
	u16	offset;
	u16	size;
};
#define SA_MAX_AUX_DATA_WORDS	8
struct sa_cmdl_upd_info {
	u16	flags;
	u16	submode;
	struct sa_cmdl_param_info	enc_size;
	struct sa_cmdl_param_info	enc_size2;
	struct sa_cmdl_param_info	enc_offset;
	struct sa_cmdl_param_info	enc_iv;
	struct sa_cmdl_param_info	enc_iv2;
	struct sa_cmdl_param_info	aad;
	struct sa_cmdl_param_info	payload;
	struct sa_cmdl_param_info	auth_size;
	struct sa_cmdl_param_info	auth_size2;
	struct sa_cmdl_param_info	auth_offset;
	struct sa_cmdl_param_info	auth_iv;
	struct sa_cmdl_param_info	aux_key_info;
	u32				aux_key[SA_MAX_AUX_DATA_WORDS];
};
#define SA_PSDATA_CTX_WORDS 4
#define SA_MAX_CMDL_WORDS (SA_DMA_NUM_PS_WORDS - SA_PSDATA_CTX_WORDS)
struct sa_ctx_info {
	u8		*sc;
	dma_addr_t	sc_phys;
	u16		sc_id;
	u16		cmdl_size;
	u32		cmdl[SA_MAX_CMDL_WORDS];
	struct sa_cmdl_upd_info cmdl_upd_info;
	u32		epib[SA_DMA_NUM_EPIB_WORDS];
};
struct sa_tfm_ctx {
	struct sa_crypto_data *dev_data;
	struct sa_ctx_info enc;
	struct sa_ctx_info dec;
	struct sa_ctx_info auth;
	int keylen;
	int iv_idx;
	u32 key[AES_KEYSIZE_256 / sizeof(u32)];
	u8 authkey[SHA512_BLOCK_SIZE];
	struct crypto_shash	*shash;
	union {
		struct crypto_skcipher		*skcipher;
		struct crypto_ahash		*ahash;
		struct crypto_aead		*aead;
	} fallback;
};
struct sa_sha_req_ctx {
	struct sa_crypto_data	*dev_data;
	u32			cmdl[SA_MAX_CMDL_WORDS + SA_PSDATA_CTX_WORDS];
	struct ahash_request	fallback_req;
};
enum sa_submode {
	SA_MODE_GEN = 0,
	SA_MODE_CCM,
	SA_MODE_GCM,
	SA_MODE_GMAC
};
enum sa_ealg_id {
	SA_EALG_ID_NONE = 0,         
	SA_EALG_ID_NULL,             
	SA_EALG_ID_AES_CTR,          
	SA_EALG_ID_AES_F8,           
	SA_EALG_ID_AES_CBC,          
	SA_EALG_ID_DES_CBC,          
	SA_EALG_ID_3DES_CBC,         
	SA_EALG_ID_CCM,              
	SA_EALG_ID_GCM,              
	SA_EALG_ID_AES_ECB,
	SA_EALG_ID_LAST
};
enum sa_aalg_id {
	SA_AALG_ID_NONE = 0,       
	SA_AALG_ID_NULL = SA_EALG_ID_LAST,  
	SA_AALG_ID_MD5,            
	SA_AALG_ID_SHA1,           
	SA_AALG_ID_SHA2_224,       
	SA_AALG_ID_SHA2_256,       
	SA_AALG_ID_SHA2_512,       
	SA_AALG_ID_HMAC_MD5,       
	SA_AALG_ID_HMAC_SHA1,      
	SA_AALG_ID_HMAC_SHA2_224,  
	SA_AALG_ID_HMAC_SHA2_256,  
	SA_AALG_ID_GMAC,           
	SA_AALG_ID_CMAC,           
	SA_AALG_ID_CBC_MAC,        
	SA_AALG_ID_AES_XCBC        
};
enum sa_eng_algo_id {
	SA_ENG_ALGO_ECB = 0,
	SA_ENG_ALGO_CBC,
	SA_ENG_ALGO_CFB,
	SA_ENG_ALGO_OFB,
	SA_ENG_ALGO_CTR,
	SA_ENG_ALGO_F8,
	SA_ENG_ALGO_F8F9,
	SA_ENG_ALGO_GCM,
	SA_ENG_ALGO_GMAC,
	SA_ENG_ALGO_CCM,
	SA_ENG_ALGO_CMAC,
	SA_ENG_ALGO_CBCMAC,
	SA_NUM_ENG_ALGOS
};
struct sa_eng_info {
	u8	eng_id;
	u16	sc_size;
};
#endif  
