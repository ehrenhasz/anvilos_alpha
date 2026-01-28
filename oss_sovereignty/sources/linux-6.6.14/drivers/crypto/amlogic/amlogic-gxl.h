

#include <crypto/aes.h>
#include <crypto/engine.h>
#include <crypto/skcipher.h>
#include <linux/debugfs.h>
#include <linux/crypto.h>
#include <linux/scatterlist.h>

#define MODE_KEY 1
#define MODE_AES_128 0x8
#define MODE_AES_192 0x9
#define MODE_AES_256 0xa

#define MESON_DECRYPT 0
#define MESON_ENCRYPT 1

#define MESON_OPMODE_ECB 0
#define MESON_OPMODE_CBC 1

#define MAXFLOW 2

#define MAXDESC 64

#define DESC_LAST BIT(18)
#define DESC_ENCRYPTION BIT(28)
#define DESC_OWN BIT(31)


struct meson_desc {
	__le32 t_status;
	__le32 t_src;
	__le32 t_dst;
};


struct meson_flow {
	struct crypto_engine *engine;
	struct completion complete;
	int status;
	unsigned int keylen;
	dma_addr_t t_phy;
	struct meson_desc *tl;
#ifdef CONFIG_CRYPTO_DEV_AMLOGIC_GXL_DEBUG
	unsigned long stat_req;
#endif
};


struct meson_dev {
	void __iomem *base;
	struct clk *busclk;
	struct device *dev;
	struct meson_flow *chanlist;
	atomic_t flow;
	int irqs[MAXFLOW];
#ifdef CONFIG_CRYPTO_DEV_AMLOGIC_GXL_DEBUG
	struct dentry *dbgfs_dir;
#endif
};


struct meson_cipher_req_ctx {
	u32 op_dir;
	int flow;
	struct skcipher_request fallback_req;	
};


struct meson_cipher_tfm_ctx {
	u32 *key;
	u32 keylen;
	u32 keymode;
	struct meson_dev *mc;
	struct crypto_skcipher *fallback_tfm;
};


struct meson_alg_template {
	u32 type;
	u32 blockmode;
	union {
		struct skcipher_engine_alg skcipher;
	} alg;
	struct meson_dev *mc;
#ifdef CONFIG_CRYPTO_DEV_AMLOGIC_GXL_DEBUG
	unsigned long stat_req;
	unsigned long stat_fb;
#endif
};

int meson_enqueue(struct crypto_async_request *areq, u32 type);

int meson_aes_setkey(struct crypto_skcipher *tfm, const u8 *key,
		     unsigned int keylen);
int meson_cipher_init(struct crypto_tfm *tfm);
void meson_cipher_exit(struct crypto_tfm *tfm);
int meson_skdecrypt(struct skcipher_request *areq);
int meson_skencrypt(struct skcipher_request *areq);
int meson_handle_cipher_request(struct crypto_engine *engine, void *areq);
