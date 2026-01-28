#include <crypto/aes.h>
#include <crypto/des.h>
#include <crypto/engine.h>
#include <crypto/skcipher.h>
#include <linux/atomic.h>
#include <linux/debugfs.h>
#include <linux/crypto.h>
#include <linux/hw_random.h>
#include <crypto/internal/hash.h>
#include <crypto/md5.h>
#include <crypto/rng.h>
#include <crypto/sha1.h>
#include <crypto/sha2.h>
#define CE_TDQ	0x00
#define CE_CTR	0x04
#define CE_ICR	0x08
#define CE_ISR	0x0C
#define CE_TLR	0x10
#define CE_TSR	0x14
#define CE_ESR	0x18
#define CE_CSSGR	0x1C
#define CE_CDSGR	0x20
#define CE_CSAR	0x24
#define CE_CDAR	0x28
#define CE_TPR	0x2C
#define CE_ENCRYPTION		0
#define CE_DECRYPTION		BIT(8)
#define CE_COMM_INT		BIT(31)
#define CE_AES_128BITS 0
#define CE_AES_192BITS 1
#define CE_AES_256BITS 2
#define CE_OP_ECB	0
#define CE_OP_CBC	(1 << 8)
#define CE_ALG_AES		0
#define CE_ALG_DES		1
#define CE_ALG_3DES		2
#define CE_ALG_MD5              16
#define CE_ALG_SHA1             17
#define CE_ALG_SHA224           18
#define CE_ALG_SHA256           19
#define CE_ALG_SHA384           20
#define CE_ALG_SHA512           21
#define CE_ALG_TRNG		48
#define CE_ALG_PRNG		49
#define CE_ALG_TRNG_V2		0x1c
#define CE_ALG_PRNG_V2		0x1d
#define CE_ID_NOTSUPP		0xFF
#define CE_ID_CIPHER_AES	0
#define CE_ID_CIPHER_DES	1
#define CE_ID_CIPHER_DES3	2
#define CE_ID_CIPHER_MAX	3
#define CE_ID_HASH_MD5		0
#define CE_ID_HASH_SHA1		1
#define CE_ID_HASH_SHA224	2
#define CE_ID_HASH_SHA256	3
#define CE_ID_HASH_SHA384	4
#define CE_ID_HASH_SHA512	5
#define CE_ID_HASH_MAX		6
#define CE_ID_OP_ECB	0
#define CE_ID_OP_CBC	1
#define CE_ID_OP_MAX	2
#define CE_ERR_ALGO_NOTSUP	BIT(0)
#define CE_ERR_DATALEN		BIT(1)
#define CE_ERR_KEYSRAM		BIT(2)
#define CE_ERR_ADDR_INVALID	BIT(5)
#define CE_ERR_KEYLADDER	BIT(6)
#define ESR_H3	0
#define ESR_A64	1
#define ESR_R40	2
#define ESR_H5	3
#define ESR_H6	4
#define ESR_D1	5
#define PRNG_DATA_SIZE (160 / 8)
#define PRNG_SEED_SIZE DIV_ROUND_UP(175, 8)
#define PRNG_LD BIT(17)
#define CE_DIE_ID_SHIFT	16
#define CE_DIE_ID_MASK	0x07
#define MAX_SG 8
#define CE_MAX_CLOCKS 4
#define MAXFLOW 4
struct ce_clock {
	const char *name;
	unsigned long freq;
	unsigned long max_freq;
};
struct ce_variant {
	char alg_cipher[CE_ID_CIPHER_MAX];
	char alg_hash[CE_ID_HASH_MAX];
	u32 op_mode[CE_ID_OP_MAX];
	bool cipher_t_dlen_in_bytes;
	bool hash_t_dlen_in_bits;
	bool prng_t_dlen_in_bytes;
	bool trng_t_dlen_in_bytes;
	struct ce_clock ce_clks[CE_MAX_CLOCKS];
	int esr;
	unsigned char prng;
	unsigned char trng;
};
struct sginfo {
	__le32 addr;
	__le32 len;
} __packed;
struct ce_task {
	__le32 t_id;
	__le32 t_common_ctl;
	__le32 t_sym_ctl;
	__le32 t_asym_ctl;
	__le32 t_key;
	__le32 t_iv;
	__le32 t_ctr;
	__le32 t_dlen;
	struct sginfo t_src[MAX_SG];
	struct sginfo t_dst[MAX_SG];
	__le32 next;
	__le32 reserved[3];
} __packed __aligned(8);
struct sun8i_ce_flow {
	struct crypto_engine *engine;
	struct completion complete;
	int status;
	dma_addr_t t_phy;
	int timeout;
	struct ce_task *tl;
	void *backup_iv;
	void *bounce_iv;
#ifdef CONFIG_CRYPTO_DEV_SUN8I_CE_DEBUG
	unsigned long stat_req;
#endif
};
struct sun8i_ce_dev {
	void __iomem *base;
	struct clk *ceclks[CE_MAX_CLOCKS];
	struct reset_control *reset;
	struct device *dev;
	struct mutex mlock;
	struct mutex rnglock;
	struct sun8i_ce_flow *chanlist;
	atomic_t flow;
	const struct ce_variant *variant;
#ifdef CONFIG_CRYPTO_DEV_SUN8I_CE_DEBUG
	struct dentry *dbgfs_dir;
	struct dentry *dbgfs_stats;
#endif
#ifdef CONFIG_CRYPTO_DEV_SUN8I_CE_TRNG
	struct hwrng trng;
#ifdef CONFIG_CRYPTO_DEV_SUN8I_CE_DEBUG
	unsigned long hwrng_stat_req;
	unsigned long hwrng_stat_bytes;
#endif
#endif
};
struct sun8i_cipher_req_ctx {
	u32 op_dir;
	int flow;
	unsigned int ivlen;
	int nr_sgs;
	int nr_sgd;
	dma_addr_t addr_iv;
	dma_addr_t addr_key;
	struct skcipher_request fallback_req;    
};
struct sun8i_cipher_tfm_ctx {
	u32 *key;
	u32 keylen;
	struct sun8i_ce_dev *ce;
	struct crypto_skcipher *fallback_tfm;
};
struct sun8i_ce_hash_tfm_ctx {
	struct sun8i_ce_dev *ce;
	struct crypto_ahash *fallback_tfm;
};
struct sun8i_ce_hash_reqctx {
	struct ahash_request fallback_req;
	int flow;
};
struct sun8i_ce_rng_tfm_ctx {
	void *seed;
	unsigned int slen;
};
struct sun8i_ce_alg_template {
	u32 type;
	u32 ce_algo_id;
	u32 ce_blockmode;
	struct sun8i_ce_dev *ce;
	union {
		struct skcipher_engine_alg skcipher;
		struct ahash_engine_alg hash;
		struct rng_alg rng;
	} alg;
	unsigned long stat_req;
	unsigned long stat_fb;
	unsigned long stat_bytes;
	unsigned long stat_fb_maxsg;
	unsigned long stat_fb_leniv;
	unsigned long stat_fb_len0;
	unsigned long stat_fb_mod16;
	unsigned long stat_fb_srcali;
	unsigned long stat_fb_srclen;
	unsigned long stat_fb_dstali;
	unsigned long stat_fb_dstlen;
	char fbname[CRYPTO_MAX_ALG_NAME];
};
int sun8i_ce_aes_setkey(struct crypto_skcipher *tfm, const u8 *key,
			unsigned int keylen);
int sun8i_ce_des3_setkey(struct crypto_skcipher *tfm, const u8 *key,
			 unsigned int keylen);
int sun8i_ce_cipher_init(struct crypto_tfm *tfm);
void sun8i_ce_cipher_exit(struct crypto_tfm *tfm);
int sun8i_ce_cipher_do_one(struct crypto_engine *engine, void *areq);
int sun8i_ce_skdecrypt(struct skcipher_request *areq);
int sun8i_ce_skencrypt(struct skcipher_request *areq);
int sun8i_ce_get_engine_number(struct sun8i_ce_dev *ce);
int sun8i_ce_run_task(struct sun8i_ce_dev *ce, int flow, const char *name);
int sun8i_ce_hash_init_tfm(struct crypto_ahash *tfm);
void sun8i_ce_hash_exit_tfm(struct crypto_ahash *tfm);
int sun8i_ce_hash_init(struct ahash_request *areq);
int sun8i_ce_hash_export(struct ahash_request *areq, void *out);
int sun8i_ce_hash_import(struct ahash_request *areq, const void *in);
int sun8i_ce_hash_final(struct ahash_request *areq);
int sun8i_ce_hash_update(struct ahash_request *areq);
int sun8i_ce_hash_finup(struct ahash_request *areq);
int sun8i_ce_hash_digest(struct ahash_request *areq);
int sun8i_ce_hash_run(struct crypto_engine *engine, void *breq);
int sun8i_ce_prng_generate(struct crypto_rng *tfm, const u8 *src,
			   unsigned int slen, u8 *dst, unsigned int dlen);
int sun8i_ce_prng_seed(struct crypto_rng *tfm, const u8 *seed, unsigned int slen);
void sun8i_ce_prng_exit(struct crypto_tfm *tfm);
int sun8i_ce_prng_init(struct crypto_tfm *tfm);
int sun8i_ce_hwrng_register(struct sun8i_ce_dev *ce);
void sun8i_ce_hwrng_unregister(struct sun8i_ce_dev *ce);
