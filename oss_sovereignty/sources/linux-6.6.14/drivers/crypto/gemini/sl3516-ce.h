

#include <crypto/aes.h>
#include <crypto/engine.h>
#include <crypto/scatterwalk.h>
#include <crypto/skcipher.h>
#include <linux/debugfs.h>
#include <linux/hw_random.h>

#define TQ0_TYPE_DATA 0
#define TQ0_TYPE_CTRL BIT(0)
#define TQ1_CIPHER BIT(1)
#define TQ2_AUTH BIT(2)
#define TQ3_IV BIT(3)
#define TQ4_KEY0 BIT(4)
#define TQ5_KEY4 BIT(5)
#define TQ6_KEY6 BIT(6)
#define TQ7_AKEY0 BIT(7)
#define TQ8_AKEY2 BIT(8)
#define TQ9_AKEY2 BIT(9)

#define ECB_AES       0x2

#define DESC_LAST 0x01
#define DESC_FIRST 0x02

#define IPSEC_ID		0x0000
#define IPSEC_STATUS_REG	0x00a8
#define IPSEC_RAND_NUM_REG	0x00ac
#define IPSEC_DMA_DEVICE_ID	0xff00
#define IPSEC_DMA_STATUS	0xff04
#define IPSEC_TXDMA_CTRL	0xff08
#define IPSEC_TXDMA_FIRST_DESC	0xff0c
#define IPSEC_TXDMA_CURR_DESC	0xff10
#define IPSEC_RXDMA_CTRL	0xff14
#define IPSEC_RXDMA_FIRST_DESC	0xff18
#define IPSEC_RXDMA_CURR_DESC	0xff1c
#define IPSEC_TXDMA_BUF_ADDR	0xff28
#define IPSEC_RXDMA_BUF_ADDR	0xff38
#define IPSEC_RXDMA_BUF_SIZE	0xff30

#define CE_ENCRYPTION		0x01
#define CE_DECRYPTION		0x03

#define MAXDESC 6

#define DMA_STATUS_RS_EOFI	BIT(22)
#define DMA_STATUS_RS_PERR	BIT(24)
#define DMA_STATUS_RS_DERR	BIT(25)
#define DMA_STATUS_TS_EOFI	BIT(27)
#define DMA_STATUS_TS_PERR	BIT(29)
#define DMA_STATUS_TS_DERR	BIT(30)

#define TXDMA_CTRL_START BIT(31)
#define TXDMA_CTRL_CONTINUE BIT(30)
#define TXDMA_CTRL_CHAIN_MODE BIT(29)

#define TXDMA_CTRL_BURST_UNK BIT(22)
#define TXDMA_CTRL_INT_FAIL BIT(17)
#define TXDMA_CTRL_INT_PERR BIT(16)

#define RXDMA_CTRL_START BIT(31)
#define RXDMA_CTRL_CONTINUE BIT(30)
#define RXDMA_CTRL_CHAIN_MODE BIT(29)

#define RXDMA_CTRL_BURST_UNK BIT(22)
#define RXDMA_CTRL_INT_FINISH BIT(18)
#define RXDMA_CTRL_INT_FAIL BIT(17)
#define RXDMA_CTRL_INT_PERR BIT(16)
#define RXDMA_CTRL_INT_EOD BIT(15)
#define RXDMA_CTRL_INT_EOF BIT(14)

#define CE_CPU 0
#define CE_DMA 1


struct descriptor {
	union {
		u32 raw;
		
		struct desc_frame_ctrl {
			u32 buffer_size	:16;
			u32 desc_count	:6;
			u32 checksum	:6;
			u32 authcomp	:1;
			u32 perr	:1;
			u32 derr	:1;
			u32 own		:1;
		} bits;
	} frame_ctrl;

	union {
		u32 raw;
		
		struct desc_tx_flag_status {
			u32 tqflag	:10;
			u32 unused	:22;
		} tx_flag;
	} flag_status;

	u32 buf_adr;

	union {
		u32 next_descriptor;
		
		struct desc_next {
			u32 sof_eof	:2;
			u32 dec		:1;
			u32 eofie	:1;
			u32 ndar	:28;
		} bits;
	} next_desc;
};


struct pkt_control_header {
	u32 process_id		:8;
	u32 auth_check_len	:3;
	u32 un1			:1;
	u32 auth_algorithm	:3;
	u32 auth_mode		:1;
	u32 fcs_stream_copy	:1;
	u32 un2			:2;
	u32 mix_key_sel		:1;
	u32 aesnk		:4;
	u32 cipher_algorithm	:3;
	u32 un3			:1;
	u32 op_mode		:4;
};

struct pkt_control_cipher {
	u32 algorithm_len	:16;
	u32 header_len		:16;
};


struct pkt_control_ecb {
	struct pkt_control_header control;
	struct pkt_control_cipher cipher;
	unsigned char key[AES_MAX_KEY_SIZE];
};


struct sl3516_ce_dev {
	void __iomem *base;
	struct clk *clks;
	struct reset_control *reset;
	struct device *dev;
	struct crypto_engine *engine;
	struct completion complete;
	int status;
	dma_addr_t dtx;
	struct descriptor *tx;
	dma_addr_t drx;
	struct descriptor *rx;
	int ctx;
	int crx;
	struct hwrng trng;
	unsigned long hwrng_stat_req;
	unsigned long hwrng_stat_bytes;
	unsigned long stat_irq;
	unsigned long stat_irq_tx;
	unsigned long stat_irq_rx;
	unsigned long stat_req;
	unsigned long fallback_sg_count_tx;
	unsigned long fallback_sg_count_rx;
	unsigned long fallback_not_same_len;
	unsigned long fallback_mod16;
	unsigned long fallback_align16;
#ifdef CONFIG_CRYPTO_DEV_SL3516_DEBUG
	struct dentry *dbgfs_dir;
	struct dentry *dbgfs_stats;
#endif
	void *pctrl;
	dma_addr_t dctrl;
};

struct sginfo {
	u32 addr;
	u32 len;
};


struct sl3516_ce_cipher_req_ctx {
	struct sginfo t_src[MAXDESC];
	struct sginfo t_dst[MAXDESC];
	u32 op_dir;
	unsigned int pctrllen;
	u32 tqflag;
	struct pkt_control_cipher *h;
	int nr_sgs;
	int nr_sgd;
	struct skcipher_request fallback_req;   
};


struct sl3516_ce_cipher_tfm_ctx {
	u32 *key;
	u32 keylen;
	struct sl3516_ce_dev *ce;
	struct crypto_skcipher *fallback_tfm;
};


struct sl3516_ce_alg_template {
	u32 type;
	u32 mode;
	struct sl3516_ce_dev *ce;
	union {
		struct skcipher_engine_alg skcipher;
	} alg;
	unsigned long stat_req;
	unsigned long stat_fb;
	unsigned long stat_bytes;
};

int sl3516_ce_enqueue(struct crypto_async_request *areq, u32 type);

int sl3516_ce_aes_setkey(struct crypto_skcipher *tfm, const u8 *key,
			 unsigned int keylen);
int sl3516_ce_cipher_init(struct crypto_tfm *tfm);
void sl3516_ce_cipher_exit(struct crypto_tfm *tfm);
int sl3516_ce_skdecrypt(struct skcipher_request *areq);
int sl3516_ce_skencrypt(struct skcipher_request *areq);

int sl3516_ce_run_task(struct sl3516_ce_dev *ce,
		       struct sl3516_ce_cipher_req_ctx *rctx, const char *name);

int sl3516_ce_rng_register(struct sl3516_ce_dev *ce);
void sl3516_ce_rng_unregister(struct sl3516_ce_dev *ce);
int sl3516_ce_handle_cipher_request(struct crypto_engine *engine, void *areq);
