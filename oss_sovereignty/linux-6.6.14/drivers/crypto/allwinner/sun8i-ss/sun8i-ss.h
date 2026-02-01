 
 
#include <crypto/aes.h>
#include <crypto/des.h>
#include <crypto/engine.h>
#include <crypto/rng.h>
#include <crypto/skcipher.h>
#include <linux/atomic.h>
#include <linux/debugfs.h>
#include <linux/crypto.h>
#include <crypto/internal/hash.h>
#include <crypto/md5.h>
#include <crypto/sha1.h>
#include <crypto/sha2.h>

#define SS_START	1

#define SS_ENCRYPTION		0
#define SS_DECRYPTION		BIT(6)

#define SS_ALG_AES		0
#define SS_ALG_DES		(1 << 2)
#define SS_ALG_3DES		(2 << 2)
#define SS_ALG_MD5		(3 << 2)
#define SS_ALG_PRNG		(4 << 2)
#define SS_ALG_SHA1		(6 << 2)
#define SS_ALG_SHA224		(7 << 2)
#define SS_ALG_SHA256		(8 << 2)

#define SS_CTL_REG		0x00
#define SS_INT_CTL_REG		0x04
#define SS_INT_STA_REG		0x08
#define SS_KEY_ADR_REG		0x10
#define SS_IV_ADR_REG		0x18
#define SS_SRC_ADR_REG		0x20
#define SS_DST_ADR_REG		0x28
#define SS_LEN_ADR_REG		0x30

#define SS_ID_NOTSUPP		0xFF

#define SS_ID_CIPHER_AES	0
#define SS_ID_CIPHER_DES	1
#define SS_ID_CIPHER_DES3	2
#define SS_ID_CIPHER_MAX	3

#define SS_ID_OP_ECB	0
#define SS_ID_OP_CBC	1
#define SS_ID_OP_MAX	2

#define SS_AES_128BITS 0
#define SS_AES_192BITS 1
#define SS_AES_256BITS 2

#define SS_OP_ECB	0
#define SS_OP_CBC	(1 << 13)

#define SS_ID_HASH_MD5	0
#define SS_ID_HASH_SHA1	1
#define SS_ID_HASH_SHA224	2
#define SS_ID_HASH_SHA256	3
#define SS_ID_HASH_MAX	4

#define SS_FLOW0	BIT(30)
#define SS_FLOW1	BIT(31)

#define SS_PRNG_CONTINUE	BIT(18)

#define MAX_SG 8

#define MAXFLOW 2

#define SS_MAX_CLOCKS 2

#define SS_DIE_ID_SHIFT	20
#define SS_DIE_ID_MASK	0x07

#define PRNG_DATA_SIZE (160 / 8)
#define PRNG_SEED_SIZE DIV_ROUND_UP(175, 8)

#define MAX_PAD_SIZE 4096

 
struct ss_clock {
	const char *name;
	unsigned long freq;
	unsigned long max_freq;
};

 
struct ss_variant {
	char alg_cipher[SS_ID_CIPHER_MAX];
	char alg_hash[SS_ID_HASH_MAX];
	u32 op_mode[SS_ID_OP_MAX];
	struct ss_clock ss_clks[SS_MAX_CLOCKS];
};

struct sginfo {
	u32 addr;
	u32 len;
};

 
struct sun8i_ss_flow {
	struct crypto_engine *engine;
	struct completion complete;
	int status;
	u8 *iv[MAX_SG];
	u8 *biv;
	void *pad;
	void *result;
#ifdef CONFIG_CRYPTO_DEV_SUN8I_SS_DEBUG
	unsigned long stat_req;
#endif
};

 
struct sun8i_ss_dev {
	void __iomem *base;
	struct clk *ssclks[SS_MAX_CLOCKS];
	struct reset_control *reset;
	struct device *dev;
	struct mutex mlock;
	struct sun8i_ss_flow *flows;
	atomic_t flow;
	const struct ss_variant *variant;
#ifdef CONFIG_CRYPTO_DEV_SUN8I_SS_DEBUG
	struct dentry *dbgfs_dir;
	struct dentry *dbgfs_stats;
#endif
};

 
struct sun8i_cipher_req_ctx {
	struct sginfo t_src[MAX_SG];
	struct sginfo t_dst[MAX_SG];
	u32 p_key;
	u32 p_iv[MAX_SG];
	int niv;
	u32 method;
	u32 op_mode;
	u32 op_dir;
	int flow;
	unsigned int ivlen;
	unsigned int keylen;
	struct skcipher_request fallback_req;   
};

 
struct sun8i_cipher_tfm_ctx {
	u32 *key;
	u32 keylen;
	struct sun8i_ss_dev *ss;
	struct crypto_skcipher *fallback_tfm;
};

 
struct sun8i_ss_rng_tfm_ctx {
	void *seed;
	unsigned int slen;
};

 
struct sun8i_ss_hash_tfm_ctx {
	struct crypto_ahash *fallback_tfm;
	struct sun8i_ss_dev *ss;
	u8 *ipad;
	u8 *opad;
	u8 key[SHA256_BLOCK_SIZE];
	int keylen;
};

 
struct sun8i_ss_hash_reqctx {
	struct sginfo t_src[MAX_SG];
	struct sginfo t_dst[MAX_SG];
	struct ahash_request fallback_req;
	u32 method;
	int flow;
};

 
struct sun8i_ss_alg_template {
	u32 type;
	u32 ss_algo_id;
	u32 ss_blockmode;
	struct sun8i_ss_dev *ss;
	union {
		struct skcipher_engine_alg skcipher;
		struct rng_alg rng;
		struct ahash_engine_alg hash;
	} alg;
	unsigned long stat_req;
	unsigned long stat_fb;
	unsigned long stat_bytes;
	unsigned long stat_fb_len;
	unsigned long stat_fb_sglen;
	unsigned long stat_fb_align;
	unsigned long stat_fb_sgnum;
	char fbname[CRYPTO_MAX_ALG_NAME];
};

int sun8i_ss_aes_setkey(struct crypto_skcipher *tfm, const u8 *key,
			unsigned int keylen);
int sun8i_ss_des3_setkey(struct crypto_skcipher *tfm, const u8 *key,
			 unsigned int keylen);
int sun8i_ss_cipher_init(struct crypto_tfm *tfm);
void sun8i_ss_cipher_exit(struct crypto_tfm *tfm);
int sun8i_ss_handle_cipher_request(struct crypto_engine *engine, void *areq);
int sun8i_ss_skdecrypt(struct skcipher_request *areq);
int sun8i_ss_skencrypt(struct skcipher_request *areq);

int sun8i_ss_get_engine_number(struct sun8i_ss_dev *ss);

int sun8i_ss_run_task(struct sun8i_ss_dev *ss, struct sun8i_cipher_req_ctx *rctx, const char *name);
int sun8i_ss_prng_generate(struct crypto_rng *tfm, const u8 *src,
			   unsigned int slen, u8 *dst, unsigned int dlen);
int sun8i_ss_prng_seed(struct crypto_rng *tfm, const u8 *seed, unsigned int slen);
int sun8i_ss_prng_init(struct crypto_tfm *tfm);
void sun8i_ss_prng_exit(struct crypto_tfm *tfm);

int sun8i_ss_hash_init_tfm(struct crypto_ahash *tfm);
void sun8i_ss_hash_exit_tfm(struct crypto_ahash *tfm);
int sun8i_ss_hash_init(struct ahash_request *areq);
int sun8i_ss_hash_export(struct ahash_request *areq, void *out);
int sun8i_ss_hash_import(struct ahash_request *areq, const void *in);
int sun8i_ss_hash_final(struct ahash_request *areq);
int sun8i_ss_hash_update(struct ahash_request *areq);
int sun8i_ss_hash_finup(struct ahash_request *areq);
int sun8i_ss_hash_digest(struct ahash_request *areq);
int sun8i_ss_hash_run(struct crypto_engine *engine, void *breq);
int sun8i_ss_hmac_setkey(struct crypto_ahash *ahash, const u8 *key,
			 unsigned int keylen);
