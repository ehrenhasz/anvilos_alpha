 
 

 

#ifndef _SPU_H
#define _SPU_H

#include <linux/types.h>
#include <linux/scatterlist.h>
#include <crypto/sha1.h>
#include <crypto/sha2.h>

enum spu_cipher_alg {
	CIPHER_ALG_NONE = 0x0,
	CIPHER_ALG_RC4 = 0x1,
	CIPHER_ALG_DES = 0x2,
	CIPHER_ALG_3DES = 0x3,
	CIPHER_ALG_AES = 0x4,
	CIPHER_ALG_LAST = 0x5
};

enum spu_cipher_mode {
	CIPHER_MODE_NONE = 0x0,
	CIPHER_MODE_ECB = 0x0,
	CIPHER_MODE_CBC = 0x1,
	CIPHER_MODE_OFB = 0x2,
	CIPHER_MODE_CFB = 0x3,
	CIPHER_MODE_CTR = 0x4,
	CIPHER_MODE_CCM = 0x5,
	CIPHER_MODE_GCM = 0x6,
	CIPHER_MODE_XTS = 0x7,
	CIPHER_MODE_LAST = 0x8
};

enum spu_cipher_type {
	CIPHER_TYPE_NONE = 0x0,
	CIPHER_TYPE_DES = 0x0,
	CIPHER_TYPE_3DES = 0x0,
	CIPHER_TYPE_INIT = 0x0,	 
	CIPHER_TYPE_AES128 = 0x0,
	CIPHER_TYPE_AES192 = 0x1,
	CIPHER_TYPE_UPDT = 0x1,	 
	CIPHER_TYPE_AES256 = 0x2,
};

enum hash_alg {
	HASH_ALG_NONE = 0x0,
	HASH_ALG_MD5 = 0x1,
	HASH_ALG_SHA1 = 0x2,
	HASH_ALG_SHA224 = 0x3,
	HASH_ALG_SHA256 = 0x4,
	HASH_ALG_AES = 0x5,
	HASH_ALG_SHA384 = 0x6,
	HASH_ALG_SHA512 = 0x7,
	 
	HASH_ALG_SHA3_224 = 0x8,
	HASH_ALG_SHA3_256 = 0x9,
	HASH_ALG_SHA3_384 = 0xa,
	HASH_ALG_SHA3_512 = 0xb,
	HASH_ALG_LAST
};

enum hash_mode {
	HASH_MODE_NONE = 0x0,
	HASH_MODE_HASH = 0x0,
	HASH_MODE_XCBC = 0x0,
	HASH_MODE_CMAC = 0x1,
	HASH_MODE_CTXT = 0x1,
	HASH_MODE_HMAC = 0x2,
	HASH_MODE_RABIN = 0x4,
	HASH_MODE_FHMAC = 0x6,
	HASH_MODE_CCM = 0x5,
	HASH_MODE_GCM = 0x6,
};

enum hash_type {
	HASH_TYPE_NONE = 0x0,
	HASH_TYPE_FULL = 0x0,
	HASH_TYPE_INIT = 0x1,
	HASH_TYPE_UPDT = 0x2,
	HASH_TYPE_FIN = 0x3,
	HASH_TYPE_AES128 = 0x0,
	HASH_TYPE_AES192 = 0x1,
	HASH_TYPE_AES256 = 0x2
};

enum aead_type {
	AES_CCM,
	AES_GCM,
	AUTHENC,
	AEAD_TYPE_LAST
};

extern char *hash_alg_name[HASH_ALG_LAST];
extern char *aead_alg_name[AEAD_TYPE_LAST];

struct spu_request_opts {
	bool is_inbound;
	bool auth_first;
	bool is_aead;
	bool is_esp;
	bool bd_suppress;
	bool is_rfc4543;
};

struct spu_cipher_parms {
	enum spu_cipher_alg  alg;
	enum spu_cipher_mode mode;
	enum spu_cipher_type type;
	u8                  *key_buf;
	u16                  key_len;
	 
	u8                  *iv_buf;
	u16                  iv_len;
};

struct spu_hash_parms {
	enum hash_alg  alg;
	enum hash_mode mode;
	enum hash_type type;
	u8             digestsize;
	u8            *key_buf;
	u16            key_len;
	u16            prebuf_len;
	 
	int            pad_len;
};

struct spu_aead_parms {
	u32 assoc_size;
	u16 iv_len;       
	u8  aad_pad_len;  
	u8  data_pad_len; 
	bool return_iv;   
	u32 ret_iv_len;   
	u32 ret_iv_off;   
};

 

#define SPU_RX_STATUS_LEN  4

 
#define SPU_STAT_PAD_MAX  4

 
#define SPU_PAD_LEN_MAX (SPU_GCM_CCM_ALIGN + MAX_HASH_BLOCK_SIZE + \
			 SPU_STAT_PAD_MAX)

 
#define SPU_GCM_CCM_ALIGN 16

 
#define SPU_SUPDT_LEN 260

 
#define SPU_INVALID_ICV  1

 
#define SPU_MAX_PAYLOAD_INF  0xFFFFFFFF

 
#define SPU_XTS_TWEAK_SIZE 16

 
#define CCM_B0_ADATA		0x40
#define CCM_B0_ADATA_SHIFT	   6
#define CCM_B0_M_PRIME		0x38
#define CCM_B0_M_PRIME_SHIFT	   3
#define CCM_B0_L_PRIME		0x07
#define CCM_B0_L_PRIME_SHIFT	   0
#define CCM_ESP_L_VALUE		   4

 
static __always_inline  bool spu_req_incl_icv(enum spu_cipher_mode cipher_mode,
					      bool is_encrypt)
{
	if ((cipher_mode == CIPHER_MODE_GCM) && !is_encrypt)
		return true;
	if ((cipher_mode == CIPHER_MODE_CCM) && !is_encrypt)
		return true;

	return false;
}

static __always_inline u32 spu_real_db_size(u32 assoc_size,
					    u32 aead_iv_buf_len,
					    u32 prebuf_len,
					    u32 data_size,
					    u32 aad_pad_len,
					    u32 gcm_pad_len,
					    u32 hash_pad_len)
{
	return assoc_size + aead_iv_buf_len + prebuf_len + data_size +
	    aad_pad_len + gcm_pad_len + hash_pad_len;
}

 

void spum_dump_msg_hdr(u8 *buf, unsigned int buf_len);

u32 spum_ns2_ctx_max_payload(enum spu_cipher_alg cipher_alg,
			     enum spu_cipher_mode cipher_mode,
			     unsigned int blocksize);
u32 spum_nsp_ctx_max_payload(enum spu_cipher_alg cipher_alg,
			     enum spu_cipher_mode cipher_mode,
			     unsigned int blocksize);
u32 spum_payload_length(u8 *spu_hdr);
u16 spum_response_hdr_len(u16 auth_key_len, u16 enc_key_len, bool is_hash);
u16 spum_hash_pad_len(enum hash_alg hash_alg, enum hash_mode hash_mode,
		      u32 chunksize, u16 hash_block_size);
u32 spum_gcm_ccm_pad_len(enum spu_cipher_mode cipher_mode,
			 unsigned int data_size);
u32 spum_assoc_resp_len(enum spu_cipher_mode cipher_mode,
			unsigned int assoc_len, unsigned int iv_len,
			bool is_encrypt);
u8 spum_aead_ivlen(enum spu_cipher_mode cipher_mode, u16 iv_len);
bool spu_req_incl_icv(enum spu_cipher_mode cipher_mode, bool is_encrypt);
enum hash_type spum_hash_type(u32 src_sent);
u32 spum_digest_size(u32 alg_digest_size, enum hash_alg alg,
		     enum hash_type htype);

u32 spum_create_request(u8 *spu_hdr,
			struct spu_request_opts *req_opts,
			struct spu_cipher_parms *cipher_parms,
			struct spu_hash_parms *hash_parms,
			struct spu_aead_parms *aead_parms,
			unsigned int data_size);

u16 spum_cipher_req_init(u8 *spu_hdr, struct spu_cipher_parms *cipher_parms);

void spum_cipher_req_finish(u8 *spu_hdr,
			    u16 spu_req_hdr_len,
			    unsigned int is_inbound,
			    struct spu_cipher_parms *cipher_parms,
			    unsigned int data_size);

void spum_request_pad(u8 *pad_start,
		      u32 gcm_padding,
		      u32 hash_pad_len,
		      enum hash_alg auth_alg,
		      enum hash_mode auth_mode,
		      unsigned int total_sent, u32 status_padding);

u8 spum_xts_tweak_in_payload(void);
u8 spum_tx_status_len(void);
u8 spum_rx_status_len(void);
int spum_status_process(u8 *statp);

void spum_ccm_update_iv(unsigned int digestsize,
			struct spu_cipher_parms *cipher_parms,
			unsigned int assoclen,
			unsigned int chunksize,
			bool is_encrypt,
			bool is_esp);
u32 spum_wordalign_padlen(u32 data_size);
#endif
