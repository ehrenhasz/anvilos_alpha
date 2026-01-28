#ifndef _SPU2_H
#define _SPU2_H
enum spu2_cipher_type {
	SPU2_CIPHER_TYPE_NONE = 0x0,
	SPU2_CIPHER_TYPE_AES128 = 0x1,
	SPU2_CIPHER_TYPE_AES192 = 0x2,
	SPU2_CIPHER_TYPE_AES256 = 0x3,
	SPU2_CIPHER_TYPE_DES = 0x4,
	SPU2_CIPHER_TYPE_3DES = 0x5,
	SPU2_CIPHER_TYPE_LAST
};
enum spu2_cipher_mode {
	SPU2_CIPHER_MODE_ECB = 0x0,
	SPU2_CIPHER_MODE_CBC = 0x1,
	SPU2_CIPHER_MODE_CTR = 0x2,
	SPU2_CIPHER_MODE_CFB = 0x3,
	SPU2_CIPHER_MODE_OFB = 0x4,
	SPU2_CIPHER_MODE_XTS = 0x5,
	SPU2_CIPHER_MODE_CCM = 0x6,
	SPU2_CIPHER_MODE_GCM = 0x7,
	SPU2_CIPHER_MODE_LAST
};
enum spu2_hash_type {
	SPU2_HASH_TYPE_NONE = 0x0,
	SPU2_HASH_TYPE_AES128 = 0x1,
	SPU2_HASH_TYPE_AES192 = 0x2,
	SPU2_HASH_TYPE_AES256 = 0x3,
	SPU2_HASH_TYPE_MD5 = 0x6,
	SPU2_HASH_TYPE_SHA1 = 0x7,
	SPU2_HASH_TYPE_SHA224 = 0x8,
	SPU2_HASH_TYPE_SHA256 = 0x9,
	SPU2_HASH_TYPE_SHA384 = 0xa,
	SPU2_HASH_TYPE_SHA512 = 0xb,
	SPU2_HASH_TYPE_SHA512_224 = 0xc,
	SPU2_HASH_TYPE_SHA512_256 = 0xd,
	SPU2_HASH_TYPE_SHA3_224 = 0xe,
	SPU2_HASH_TYPE_SHA3_256 = 0xf,
	SPU2_HASH_TYPE_SHA3_384 = 0x10,
	SPU2_HASH_TYPE_SHA3_512 = 0x11,
	SPU2_HASH_TYPE_LAST
};
enum spu2_hash_mode {
	SPU2_HASH_MODE_CMAC = 0x0,
	SPU2_HASH_MODE_CBC_MAC = 0x1,
	SPU2_HASH_MODE_XCBC_MAC = 0x2,
	SPU2_HASH_MODE_HMAC = 0x3,
	SPU2_HASH_MODE_RABIN = 0x4,
	SPU2_HASH_MODE_CCM = 0x5,
	SPU2_HASH_MODE_GCM = 0x6,
	SPU2_HASH_MODE_RESERVED = 0x7,
	SPU2_HASH_MODE_LAST
};
enum spu2_ret_md_opts {
	SPU2_RET_NO_MD = 0,	 
	SPU2_RET_FMD_OMD = 1,	 
	SPU2_RET_FMD_ONLY = 2,	 
	SPU2_RET_FMD_OMD_IV = 3,	 
};
struct SPU2_FMD {
	__le64 ctrl0;
	__le64 ctrl1;
	__le64 ctrl2;
	__le64 ctrl3;
};
#define FMD_SIZE  sizeof(struct SPU2_FMD)
#define SPU2_REQ_FIXED_LEN FMD_SIZE
#define SPU2_HEADER_ALLOC_LEN (SPU_REQ_FIXED_LEN + \
				2 * MAX_KEY_SIZE + 2 * MAX_IV_SIZE)
#define SPU2_CIPH_ENCRYPT_EN            0x1  
#define SPU2_CIPH_TYPE                 0xF0  
#define SPU2_CIPH_TYPE_SHIFT              4
#define SPU2_CIPH_MODE                0xF00  
#define SPU2_CIPH_MODE_SHIFT              8
#define SPU2_CFB_MASK                0x7000  
#define SPU2_CFB_MASK_SHIFT              12
#define SPU2_PROTO_SEL             0xF00000  
#define SPU2_PROTO_SEL_SHIFT             20
#define SPU2_HASH_FIRST           0x1000000  
#define SPU2_CHK_TAG              0x2000000  
#define SPU2_HASH_TYPE          0x1F0000000  
#define SPU2_HASH_TYPE_SHIFT             28
#define SPU2_HASH_MODE         0xF000000000  
#define SPU2_HASH_MODE_SHIFT             36
#define SPU2_CIPH_PAD_EN     0x100000000000  
#define SPU2_CIPH_PAD      0xFF000000000000  
#define SPU2_CIPH_PAD_SHIFT              48
#define SPU2_TAG_LOC                    0x1  
#define SPU2_HAS_FR_DATA                0x2  
#define SPU2_HAS_AAD1                   0x4  
#define SPU2_HAS_NAAD                   0x8  
#define SPU2_HAS_AAD2                  0x10  
#define SPU2_HAS_ESN                   0x20  
#define SPU2_HASH_KEY_LEN            0xFF00  
#define SPU2_HASH_KEY_LEN_SHIFT           8
#define SPU2_CIPH_KEY_LEN         0xFF00000  
#define SPU2_CIPH_KEY_LEN_SHIFT          20
#define SPU2_GENIV               0x10000000  
#define SPU2_HASH_IV             0x20000000  
#define SPU2_RET_IV              0x40000000  
#define SPU2_RET_IV_LEN         0xF00000000  
#define SPU2_RET_IV_LEN_SHIFT            32
#define SPU2_IV_OFFSET         0xF000000000  
#define SPU2_IV_OFFSET_SHIFT             36
#define SPU2_IV_LEN          0x1F0000000000  
#define SPU2_IV_LEN_SHIFT                40
#define SPU2_HASH_TAG_LEN  0x7F000000000000  
#define SPU2_HASH_TAG_LEN_SHIFT          48
#define SPU2_RETURN_MD    0x300000000000000  
#define SPU2_RETURN_MD_SHIFT             56
#define SPU2_RETURN_FD    0x400000000000000
#define SPU2_RETURN_AAD1  0x800000000000000
#define SPU2_RETURN_NAAD 0x1000000000000000
#define SPU2_RETURN_AAD2 0x2000000000000000
#define SPU2_RETURN_PAY  0x4000000000000000  
#define SPU2_AAD1_OFFSET              0xFFF  
#define SPU2_AAD1_LEN               0xFF000  
#define SPU2_AAD1_LEN_SHIFT              12
#define SPU2_AAD2_OFFSET         0xFFF00000  
#define SPU2_AAD2_OFFSET_SHIFT           20
#define SPU2_PL_OFFSET   0xFFFFFFFF00000000  
#define SPU2_PL_OFFSET_SHIFT             32
#define SPU2_PL_LEN              0xFFFFFFFF  
#define SPU2_TLS_LEN         0xFFFF00000000  
#define SPU2_TLS_LEN_SHIFT               32
#define SPU2_MAX_PAYLOAD  SPU2_PL_LEN
#define SPU2_INVALID_ICV  1
void spu2_dump_msg_hdr(u8 *buf, unsigned int buf_len);
u32 spu2_ctx_max_payload(enum spu_cipher_alg cipher_alg,
			 enum spu_cipher_mode cipher_mode,
			 unsigned int blocksize);
u32 spu2_payload_length(u8 *spu_hdr);
u16 spu2_response_hdr_len(u16 auth_key_len, u16 enc_key_len, bool is_hash);
u16 spu2_hash_pad_len(enum hash_alg hash_alg, enum hash_mode hash_mode,
		      u32 chunksize, u16 hash_block_size);
u32 spu2_gcm_ccm_pad_len(enum spu_cipher_mode cipher_mode,
			 unsigned int data_size);
u32 spu2_assoc_resp_len(enum spu_cipher_mode cipher_mode,
			unsigned int assoc_len, unsigned int iv_len,
			bool is_encrypt);
u8 spu2_aead_ivlen(enum spu_cipher_mode cipher_mode,
		   u16 iv_len);
enum hash_type spu2_hash_type(u32 src_sent);
u32 spu2_digest_size(u32 alg_digest_size, enum hash_alg alg,
		     enum hash_type htype);
u32 spu2_create_request(u8 *spu_hdr,
			struct spu_request_opts *req_opts,
			struct spu_cipher_parms *cipher_parms,
			struct spu_hash_parms *hash_parms,
			struct spu_aead_parms *aead_parms,
			unsigned int data_size);
u16 spu2_cipher_req_init(u8 *spu_hdr, struct spu_cipher_parms *cipher_parms);
void spu2_cipher_req_finish(u8 *spu_hdr,
			    u16 spu_req_hdr_len,
			    unsigned int is_inbound,
			    struct spu_cipher_parms *cipher_parms,
			    unsigned int data_size);
void spu2_request_pad(u8 *pad_start, u32 gcm_padding, u32 hash_pad_len,
		      enum hash_alg auth_alg, enum hash_mode auth_mode,
		      unsigned int total_sent, u32 status_padding);
u8 spu2_xts_tweak_in_payload(void);
u8 spu2_tx_status_len(void);
u8 spu2_rx_status_len(void);
int spu2_status_process(u8 *statp);
void spu2_ccm_update_iv(unsigned int digestsize,
			struct spu_cipher_parms *cipher_parms,
			unsigned int assoclen, unsigned int chunksize,
			bool is_encrypt, bool is_esp);
u32 spu2_wordalign_padlen(u32 data_size);
#endif
