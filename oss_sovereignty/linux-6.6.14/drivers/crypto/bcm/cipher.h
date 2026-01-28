#ifndef _CIPHER_H
#define _CIPHER_H
#include <linux/atomic.h>
#include <linux/mailbox/brcm-message.h>
#include <linux/mailbox_client.h>
#include <crypto/aes.h>
#include <crypto/internal/hash.h>
#include <crypto/internal/skcipher.h>
#include <crypto/aead.h>
#include <crypto/arc4.h>
#include <crypto/gcm.h>
#include <crypto/sha1.h>
#include <crypto/sha2.h>
#include <crypto/sha3.h>
#include "spu.h"
#include "spum.h"
#include "spu2.h"
#define MAX_SPUS 16
#define ARC4_STATE_SIZE     4
#define CCM_AES_IV_SIZE    16
#define CCM_ESP_IV_SIZE     8
#define RFC4543_ICV_SIZE   16
#define MAX_KEY_SIZE	ARC4_MAX_KEY_SIZE
#define MAX_IV_SIZE	AES_BLOCK_SIZE
#define MAX_DIGEST_SIZE	SHA3_512_DIGEST_SIZE
#define MAX_ASSOC_SIZE	512
#define GCM_ESP_SALT_SIZE   4
#define CCM_ESP_SALT_SIZE   3
#define MAX_SALT_SIZE       GCM_ESP_SALT_SIZE
#define GCM_ESP_SALT_OFFSET 0
#define CCM_ESP_SALT_OFFSET 1
#define GCM_ESP_DIGESTSIZE 16
#define MAX_HASH_BLOCK_SIZE SHA512_BLOCK_SIZE
#define HASH_CARRY_MAX  MAX_HASH_BLOCK_SIZE
#define SPU_MSG_ALIGN  4
#define SPU_MB_RETRY_MAX  1000
enum op_type {
	SPU_OP_CIPHER,
	SPU_OP_HASH,
	SPU_OP_HMAC,
	SPU_OP_AEAD,
	SPU_OP_NUM
};
enum spu_spu_type {
	SPU_TYPE_SPUM,
	SPU_TYPE_SPU2,
};
enum spu_spu_subtype {
	SPU_SUBTYPE_SPUM_NS2,
	SPU_SUBTYPE_SPUM_NSP,
	SPU_SUBTYPE_SPU2_V1,
	SPU_SUBTYPE_SPU2_V2
};
struct spu_type_subtype {
	enum spu_spu_type type;
	enum spu_spu_subtype subtype;
};
struct cipher_op {
	enum spu_cipher_alg alg;
	enum spu_cipher_mode mode;
};
struct auth_op {
	enum hash_alg alg;
	enum hash_mode mode;
};
struct iproc_alg_s {
	u32 type;
	union {
		struct skcipher_alg skcipher;
		struct ahash_alg hash;
		struct aead_alg aead;
	} alg;
	struct cipher_op cipher_info;
	struct auth_op auth_info;
	bool auth_first;
	bool registered;
};
struct spu_msg_buf {
	u8 bcm_spu_req_hdr[ALIGN(SPU2_HEADER_ALLOC_LEN, SPU_MSG_ALIGN)];
	u8 iv_ctr[ALIGN(2 * AES_BLOCK_SIZE, SPU_MSG_ALIGN)];
	u8 digest[ALIGN(MAX_DIGEST_SIZE, SPU_MSG_ALIGN)];
	u8 spu_req_pad[ALIGN(SPU_PAD_LEN_MAX, SPU_MSG_ALIGN)];
	u8 tx_stat[ALIGN(SPU_TX_STATUS_LEN, SPU_MSG_ALIGN)];
	u8 spu_resp_hdr[ALIGN(SPU2_HEADER_ALLOC_LEN, SPU_MSG_ALIGN)];
	u8 rx_stat_pad[ALIGN(SPU_STAT_PAD_MAX, SPU_MSG_ALIGN)];
	u8 rx_stat[ALIGN(SPU_RX_STATUS_LEN, SPU_MSG_ALIGN)];
	union {
		struct {
			u8 supdt_tweak[ALIGN(SPU_SUPDT_LEN, SPU_MSG_ALIGN)];
		} c;
		struct {
			u8 gcmpad[ALIGN(AES_BLOCK_SIZE, SPU_MSG_ALIGN)];
			u8 req_aad_pad[ALIGN(SPU_PAD_LEN_MAX, SPU_MSG_ALIGN)];
			u8 resp_aad[ALIGN(MAX_ASSOC_SIZE + MAX_IV_SIZE,
					  SPU_MSG_ALIGN)];
		} a;
	};
};
struct iproc_ctx_s {
	u8 enckey[MAX_KEY_SIZE + ARC4_STATE_SIZE];
	unsigned int enckeylen;
	u8 authkey[MAX_KEY_SIZE + ARC4_STATE_SIZE];
	unsigned int authkeylen;
	u8 salt[MAX_SALT_SIZE];
	unsigned int salt_len;
	unsigned int salt_offset;
	u8 iv[MAX_IV_SIZE];
	unsigned int digestsize;
	struct iproc_alg_s *alg;
	bool is_esp;
	struct cipher_op cipher;
	enum spu_cipher_type cipher_type;
	struct auth_op auth;
	bool auth_first;
	unsigned int max_payload;
	struct crypto_aead *fallback_cipher;
	u8 ipad[MAX_HASH_BLOCK_SIZE];
	u8 opad[MAX_HASH_BLOCK_SIZE];
	u8 bcm_spu_req_hdr[ALIGN(SPU2_HEADER_ALLOC_LEN, SPU_MSG_ALIGN)];
	u16 spu_req_hdr_len;
	u16 spu_resp_hdr_len;
	struct shash_desc *shash;
	bool is_rfc4543;	 
};
struct spu_hash_export_s {
	unsigned int total_todo;
	unsigned int total_sent;
	u8 hash_carry[HASH_CARRY_MAX];
	unsigned int hash_carry_len;
	u8 incr_hash[MAX_DIGEST_SIZE];
	bool is_sw_hmac;
};
struct iproc_reqctx_s {
	struct crypto_async_request *parent;
	struct iproc_ctx_s *ctx;
	u8 chan_idx;    
	unsigned int total_todo;
	unsigned int total_received;	 
	unsigned int total_sent;
	unsigned int src_sent;
	struct scatterlist *assoc;
	struct scatterlist *src_sg;
	int src_nents;		 
	u32 src_skip;		 
	struct scatterlist *dst_sg;
	int dst_nents;		 
	u32 dst_skip;		 
	struct brcm_message mb_mssg;
	bool bd_suppress;	 
	bool is_encrypt;
	u8 *iv_ctr;
	unsigned int iv_ctr_len;
	u8 hash_carry[HASH_CARRY_MAX];
	unsigned int hash_carry_len;
	unsigned int is_final;	 
	u8 incr_hash[MAX_DIGEST_SIZE];
	bool is_sw_hmac;
	gfp_t gfp;
	struct spu_msg_buf msg_buf;
	struct aead_request req;
};
struct spu_hw {
	void (*spu_dump_msg_hdr)(u8 *buf, unsigned int buf_len);
	u32 (*spu_ctx_max_payload)(enum spu_cipher_alg cipher_alg,
				   enum spu_cipher_mode cipher_mode,
				   unsigned int blocksize);
	u32 (*spu_payload_length)(u8 *spu_hdr);
	u16 (*spu_response_hdr_len)(u16 auth_key_len, u16 enc_key_len,
				    bool is_hash);
	u16 (*spu_hash_pad_len)(enum hash_alg hash_alg,
				enum hash_mode hash_mode, u32 chunksize,
				u16 hash_block_size);
	u32 (*spu_gcm_ccm_pad_len)(enum spu_cipher_mode cipher_mode,
				   unsigned int data_size);
	u32 (*spu_assoc_resp_len)(enum spu_cipher_mode cipher_mode,
				  unsigned int assoc_len,
				  unsigned int iv_len, bool is_encrypt);
	u8 (*spu_aead_ivlen)(enum spu_cipher_mode cipher_mode,
			     u16 iv_len);
	enum hash_type (*spu_hash_type)(u32 src_sent);
	u32 (*spu_digest_size)(u32 digest_size, enum hash_alg alg,
			       enum hash_type);
	u32 (*spu_create_request)(u8 *spu_hdr,
				  struct spu_request_opts *req_opts,
				  struct spu_cipher_parms *cipher_parms,
				  struct spu_hash_parms *hash_parms,
				  struct spu_aead_parms *aead_parms,
				  unsigned int data_size);
	u16 (*spu_cipher_req_init)(u8 *spu_hdr,
				   struct spu_cipher_parms *cipher_parms);
	void (*spu_cipher_req_finish)(u8 *spu_hdr,
				      u16 spu_req_hdr_len,
				      unsigned int is_inbound,
				      struct spu_cipher_parms *cipher_parms,
				      unsigned int data_size);
	void (*spu_request_pad)(u8 *pad_start, u32 gcm_padding,
				u32 hash_pad_len, enum hash_alg auth_alg,
				enum hash_mode auth_mode,
				unsigned int total_sent, u32 status_padding);
	u8 (*spu_xts_tweak_in_payload)(void);
	u8 (*spu_tx_status_len)(void);
	u8 (*spu_rx_status_len)(void);
	int (*spu_status_process)(u8 *statp);
	void (*spu_ccm_update_iv)(unsigned int digestsize,
				  struct spu_cipher_parms *cipher_parms,
				  unsigned int assoclen, unsigned int chunksize,
				  bool is_encrypt, bool is_esp);
	u32 (*spu_wordalign_padlen)(u32 data_size);
	void __iomem *reg_vbase[MAX_SPUS];
	enum spu_spu_type spu_type;
	enum spu_spu_subtype spu_subtype;
	u32 num_spu;
	u32 num_chan;
};
struct bcm_device_private {
	struct platform_device *pdev;
	struct spu_hw spu;
	atomic_t session_count;	 
	atomic_t stream_count;	 
	u8 bcm_hdr_len;
	atomic_t next_chan;
	struct dentry *debugfs_dir;
	struct dentry *debugfs_stats;
	atomic64_t bytes_in;
	atomic64_t bytes_out;
	atomic_t op_counts[SPU_OP_NUM];
	atomic_t cipher_cnt[CIPHER_ALG_LAST][CIPHER_MODE_LAST];
	atomic_t hash_cnt[HASH_ALG_LAST];
	atomic_t hmac_cnt[HASH_ALG_LAST];
	atomic_t aead_cnt[AEAD_TYPE_LAST];
	atomic_t setkey_cnt[SPU_OP_NUM];
	atomic_t mb_no_spc;
	atomic_t mb_send_fail;
	atomic_t bad_icv;
	struct mbox_client mcl;
	struct mbox_chan **mbox;
};
extern struct bcm_device_private iproc_priv;
#endif
