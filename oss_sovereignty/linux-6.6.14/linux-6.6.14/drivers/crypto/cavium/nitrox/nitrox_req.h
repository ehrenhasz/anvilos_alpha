#ifndef __NITROX_REQ_H
#define __NITROX_REQ_H
#include <linux/dma-mapping.h>
#include <crypto/aes.h>
#include "nitrox_dev.h"
#define PENDING_SIG	0xFFFFFFFFFFFFFFFFUL
#define PRIO 4001
typedef void (*sereq_completion_t)(void *req, int err);
struct gphdr {
	__be16 param0;
	__be16 param1;
	__be16 param2;
	__be16 param3;
};
union se_req_ctrl {
	u64 value;
	struct {
		u64 raz	: 22;
		u64 arg	: 8;
		u64 ctxc : 2;
		u64 unca : 1;
		u64 info : 3;
		u64 unc : 8;
		u64 ctxl : 12;
		u64 uddl : 8;
	} s;
};
#define MAX_IV_LEN 16
struct se_crypto_request {
	u8 opcode;
	gfp_t gfp;
	u32 flags;
	u64 ctx_handle;
	struct gphdr gph;
	union se_req_ctrl ctrl;
	u64 *orh;
	u64 *comp;
	struct scatterlist *src;
	struct scatterlist *dst;
};
#define FLEXI_CRYPTO_ENCRYPT_HMAC	0x33
#define ENCRYPT	0
#define DECRYPT 1
#define IV_FROM_CTX	0
#define IV_FROM_DPTR	1
enum flexi_cipher {
	CIPHER_NULL = 0,
	CIPHER_3DES_CBC,
	CIPHER_3DES_ECB,
	CIPHER_AES_CBC,
	CIPHER_AES_ECB,
	CIPHER_AES_CFB,
	CIPHER_AES_CTR,
	CIPHER_AES_GCM,
	CIPHER_AES_XTS,
	CIPHER_AES_CCM,
	CIPHER_AES_CBC_CTS,
	CIPHER_AES_ECB_CTS,
	CIPHER_INVALID
};
enum flexi_auth {
	AUTH_NULL = 0,
	AUTH_MD5,
	AUTH_SHA1,
	AUTH_SHA2_SHA224,
	AUTH_SHA2_SHA256,
	AUTH_SHA2_SHA384,
	AUTH_SHA2_SHA512,
	AUTH_GMAC,
	AUTH_INVALID
};
struct crypto_keys {
	union {
		u8 key[AES_MAX_KEY_SIZE];
		u8 key1[AES_MAX_KEY_SIZE];
	} u;
	u8 iv[AES_BLOCK_SIZE];
};
struct auth_keys {
	union {
		u8 ipad[64];
		u8 key2[64];
	} u;
	u8 opad[64];
};
union fc_ctx_flags {
	__be64 f;
	u64 fu;
	struct {
#if defined(__BIG_ENDIAN_BITFIELD)
		u64 cipher_type	: 4;
		u64 reserved_59	: 1;
		u64 aes_keylen : 2;
		u64 iv_source : 1;
		u64 hash_type : 4;
		u64 reserved_49_51 : 3;
		u64 auth_input_type: 1;
		u64 mac_len : 8;
		u64 reserved_0_39 : 40;
#else
		u64 reserved_0_39 : 40;
		u64 mac_len : 8;
		u64 auth_input_type: 1;
		u64 reserved_49_51 : 3;
		u64 hash_type : 4;
		u64 iv_source : 1;
		u64 aes_keylen : 2;
		u64 reserved_59	: 1;
		u64 cipher_type	: 4;
#endif
	} w0;
};
struct flexi_crypto_context {
	union fc_ctx_flags flags;
	struct crypto_keys crypto;
	struct auth_keys auth;
};
struct crypto_ctx_hdr {
	struct dma_pool *pool;
	dma_addr_t dma;
	void *vaddr;
};
struct nitrox_crypto_ctx {
	struct nitrox_device *ndev;
	union {
		u64 ctx_handle;
		struct flexi_crypto_context *fctx;
	} u;
	struct crypto_ctx_hdr *chdr;
	sereq_completion_t callback;
};
struct nitrox_kcrypt_request {
	struct se_crypto_request creq;
	u8 *src;
	u8 *dst;
	u8 *iv_out;
};
struct nitrox_aead_rctx {
	struct nitrox_kcrypt_request nkreq;
	unsigned int cryptlen;
	unsigned int assoclen;
	unsigned int srclen;
	unsigned int dstlen;
	u8 *iv;
	int ivsize;
	u32 flags;
	u64 ctx_handle;
	struct scatterlist *src;
	struct scatterlist *dst;
	u8 ctrl_arg;
};
struct nitrox_rfc4106_rctx {
	struct nitrox_aead_rctx base;
	struct scatterlist src[3];
	struct scatterlist dst[3];
	u8 assoc[20];
};
union pkt_instr_hdr {
	__be64 bev;
	u64 value;
	struct {
#if defined(__BIG_ENDIAN_BITFIELD)
		u64 raz_48_63 : 16;
		u64 g : 1;
		u64 gsz	: 7;
		u64 ihi	: 1;
		u64 ssz	: 7;
		u64 raz_30_31 : 2;
		u64 fsz	: 6;
		u64 raz_16_23 : 8;
		u64 tlen : 16;
#else
		u64 tlen : 16;
		u64 raz_16_23 : 8;
		u64 fsz	: 6;
		u64 raz_30_31 : 2;
		u64 ssz	: 7;
		u64 ihi	: 1;
		u64 gsz	: 7;
		u64 g : 1;
		u64 raz_48_63 : 16;
#endif
	} s;
};
union pkt_hdr {
	__be64 bev[2];
	u64 value[2];
	struct {
#if defined(__BIG_ENDIAN_BITFIELD)
		u64 opcode : 8;
		u64 arg	: 8;
		u64 ctxc : 2;
		u64 unca : 1;
		u64 raz_44 : 1;
		u64 info : 3;
		u64 destport : 9;
		u64 unc	: 8;
		u64 raz_19_23 : 5;
		u64 grp	: 3;
		u64 raz_15 : 1;
		u64 ctxl : 7;
		u64 uddl : 8;
#else
		u64 uddl : 8;
		u64 ctxl : 7;
		u64 raz_15 : 1;
		u64 grp	: 3;
		u64 raz_19_23 : 5;
		u64 unc	: 8;
		u64 destport : 9;
		u64 info : 3;
		u64 raz_44 : 1;
		u64 unca : 1;
		u64 ctxc : 2;
		u64 arg	: 8;
		u64 opcode : 8;
#endif
		__be64 ctxp;
	} s;
};
union slc_store_info {
	__be64 bev[2];
	u64 value[2];
	struct {
#if defined(__BIG_ENDIAN_BITFIELD)
		u64 raz_39_63 : 25;
		u64 ssz	: 7;
		u64 raz_0_31 : 32;
#else
		u64 raz_0_31 : 32;
		u64 ssz	: 7;
		u64 raz_39_63 : 25;
#endif
		__be64 rptr;
	} s;
};
struct nps_pkt_instr {
	__be64 dptr0;
	union pkt_instr_hdr ih;
	union pkt_hdr irh;
	union slc_store_info slc;
	u64 fdata[2];
};
struct aqmq_command_s {
	__be16 opcode;
	__be16 param1;
	__be16 param2;
	__be16 dlen;
	__be64 dptr;
	__be64 rptr;
	union {
		__be64 word3;
#if defined(__BIG_ENDIAN_BITFIELD)
		u64 grp : 3;
		u64 cptr : 61;
#else
		u64 cptr : 61;
		u64 grp : 3;
#endif
	};
};
struct ctx_hdr {
	struct dma_pool *pool;
	dma_addr_t dma;
	dma_addr_t ctx_dma;
};
struct nitrox_sgcomp {
	__be16 len[4];
	__be64 dma[4];
};
struct nitrox_sgtable {
	u8 sgmap_cnt;
	u16 total_bytes;
	u32 sgcomp_len;
	dma_addr_t sgcomp_dma;
	struct scatterlist *sg;
	struct nitrox_sgcomp *sgcomp;
};
#define ORH_HLEN	8
#define COMP_HLEN	8
struct resp_hdr {
	u64 *orh;
	u64 *completion;
};
typedef void (*completion_t)(void *arg, int err);
struct nitrox_softreq {
	struct list_head response;
	struct list_head backlog;
	u32 flags;
	gfp_t gfp;
	atomic_t status;
	struct nitrox_device *ndev;
	struct nitrox_cmdq *cmdq;
	struct nps_pkt_instr instr;
	struct resp_hdr resp;
	struct nitrox_sgtable in;
	struct nitrox_sgtable out;
	unsigned long tstamp;
	completion_t callback;
	void *cb_arg;
};
static inline int flexi_aes_keylen(int keylen)
{
	int aes_keylen;
	switch (keylen) {
	case AES_KEYSIZE_128:
		aes_keylen = 1;
		break;
	case AES_KEYSIZE_192:
		aes_keylen = 2;
		break;
	case AES_KEYSIZE_256:
		aes_keylen = 3;
		break;
	default:
		aes_keylen = -EINVAL;
		break;
	}
	return aes_keylen;
}
static inline void *alloc_req_buf(int nents, int extralen, gfp_t gfp)
{
	size_t size;
	size = sizeof(struct scatterlist) * nents;
	size += extralen;
	return kzalloc(size, gfp);
}
static inline struct scatterlist *create_single_sg(struct scatterlist *sg,
						   void *buf, int buflen)
{
	sg_set_buf(sg, buf, buflen);
	sg++;
	return sg;
}
static inline struct scatterlist *create_multi_sg(struct scatterlist *to_sg,
						  struct scatterlist *from_sg,
						  int buflen)
{
	struct scatterlist *sg = to_sg;
	unsigned int sglen;
	for (; buflen && from_sg; buflen -= sglen) {
		sglen = from_sg->length;
		if (sglen > buflen)
			sglen = buflen;
		sg_set_buf(sg, sg_virt(from_sg), sglen);
		from_sg = sg_next(from_sg);
		sg++;
	}
	return sg;
}
static inline void set_orh_value(u64 *orh)
{
	WRITE_ONCE(*orh, PENDING_SIG);
}
static inline void set_comp_value(u64 *comp)
{
	WRITE_ONCE(*comp, PENDING_SIG);
}
static inline int alloc_src_req_buf(struct nitrox_kcrypt_request *nkreq,
				    int nents, int ivsize)
{
	struct se_crypto_request *creq = &nkreq->creq;
	nkreq->src = alloc_req_buf(nents, ivsize, creq->gfp);
	if (!nkreq->src)
		return -ENOMEM;
	return 0;
}
static inline void nitrox_creq_copy_iv(char *dst, char *src, int size)
{
	memcpy(dst, src, size);
}
static inline struct scatterlist *nitrox_creq_src_sg(char *iv, int ivsize)
{
	return (struct scatterlist *)(iv + ivsize);
}
static inline void nitrox_creq_set_src_sg(struct nitrox_kcrypt_request *nkreq,
					  int nents, int ivsize,
					  struct scatterlist *src, int buflen)
{
	char *iv = nkreq->src;
	struct scatterlist *sg;
	struct se_crypto_request *creq = &nkreq->creq;
	creq->src = nitrox_creq_src_sg(iv, ivsize);
	sg = creq->src;
	sg_init_table(sg, nents);
	sg = create_single_sg(sg, iv, ivsize);
	create_multi_sg(sg, src, buflen);
}
static inline int alloc_dst_req_buf(struct nitrox_kcrypt_request *nkreq,
				    int nents)
{
	int extralen = ORH_HLEN + COMP_HLEN;
	struct se_crypto_request *creq = &nkreq->creq;
	nkreq->dst = alloc_req_buf(nents, extralen, creq->gfp);
	if (!nkreq->dst)
		return -ENOMEM;
	return 0;
}
static inline void nitrox_creq_set_orh(struct nitrox_kcrypt_request *nkreq)
{
	struct se_crypto_request *creq = &nkreq->creq;
	creq->orh = (u64 *)(nkreq->dst);
	set_orh_value(creq->orh);
}
static inline void nitrox_creq_set_comp(struct nitrox_kcrypt_request *nkreq)
{
	struct se_crypto_request *creq = &nkreq->creq;
	creq->comp = (u64 *)(nkreq->dst + ORH_HLEN);
	set_comp_value(creq->comp);
}
static inline struct scatterlist *nitrox_creq_dst_sg(char *dst)
{
	return (struct scatterlist *)(dst + ORH_HLEN + COMP_HLEN);
}
static inline void nitrox_creq_set_dst_sg(struct nitrox_kcrypt_request *nkreq,
					  int nents, int ivsize,
					  struct scatterlist *dst, int buflen)
{
	struct se_crypto_request *creq = &nkreq->creq;
	struct scatterlist *sg;
	char *iv = nkreq->src;
	creq->dst = nitrox_creq_dst_sg(nkreq->dst);
	sg = creq->dst;
	sg_init_table(sg, nents);
	sg = create_single_sg(sg, creq->orh, ORH_HLEN);
	sg = create_single_sg(sg, iv, ivsize);
	sg = create_multi_sg(sg, dst, buflen);
	create_single_sg(sg, creq->comp, COMP_HLEN);
}
#endif  
