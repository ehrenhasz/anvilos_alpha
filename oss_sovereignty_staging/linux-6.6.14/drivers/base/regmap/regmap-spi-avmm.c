





#include <linux/module.h>
#include <linux/regmap.h>
#include <linux/spi/spi.h>
#include <linux/swab.h>

 

#define PKT_SOP			0x7a
#define PKT_EOP			0x7b
#define PKT_CHANNEL		0x7c
#define PKT_ESC			0x7d

#define PHY_IDLE		0x4a
#define PHY_ESC			0x4d

#define TRANS_CODE_WRITE	0x0
#define TRANS_CODE_SEQ_WRITE	0x4
#define TRANS_CODE_READ		0x10
#define TRANS_CODE_SEQ_READ	0x14
#define TRANS_CODE_NO_TRANS	0x7f

#define SPI_AVMM_XFER_TIMEOUT	(msecs_to_jiffies(200))

 
#define SPI_AVMM_REG_SIZE		4UL
 
#define SPI_AVMM_VAL_SIZE		4UL

 
#define MAX_READ_CNT		256UL
#define MAX_WRITE_CNT		1UL

struct trans_req_header {
	u8 code;
	u8 rsvd;
	__be16 size;
	__be32 addr;
} __packed;

struct trans_resp_header {
	u8 r_code;
	u8 rsvd;
	__be16 size;
} __packed;

#define TRANS_REQ_HD_SIZE	(sizeof(struct trans_req_header))
#define TRANS_RESP_HD_SIZE	(sizeof(struct trans_resp_header))

 
#define TRANS_WR_TX_SIZE(n)	(TRANS_REQ_HD_SIZE + SPI_AVMM_VAL_SIZE * (n))
#define TRANS_RD_TX_SIZE	TRANS_REQ_HD_SIZE
#define TRANS_TX_MAX		TRANS_WR_TX_SIZE(MAX_WRITE_CNT)

#define TRANS_RD_RX_SIZE(n)	(SPI_AVMM_VAL_SIZE * (n))
#define TRANS_WR_RX_SIZE	TRANS_RESP_HD_SIZE
#define TRANS_RX_MAX		TRANS_RD_RX_SIZE(MAX_READ_CNT)

 
#define TRANS_BUF_SIZE		((TRANS_TX_MAX > TRANS_RX_MAX) ?	\
				 TRANS_TX_MAX : TRANS_RX_MAX)

 
#define PHY_TX_MAX		ALIGN(2 * TRANS_TX_MAX + 4, 4)

 
#define PHY_BUF_SIZE		PHY_TX_MAX

 
struct spi_avmm_bridge {
	struct spi_device *spi;
	unsigned char word_len;
	unsigned int trans_len;
	unsigned int phy_len;
	 
	char trans_buf[TRANS_BUF_SIZE];
	char phy_buf[PHY_BUF_SIZE];
	void (*swap_words)(void *buf, unsigned int len);
};

static void br_swap_words_32(void *buf, unsigned int len)
{
	swab32_array(buf, len / 4);
}

 
static int br_trans_tx_prepare(struct spi_avmm_bridge *br, bool is_read, u32 reg,
			       u32 *wr_val, u32 count)
{
	struct trans_req_header *header;
	unsigned int trans_len;
	u8 code;
	__le32 *data;
	int i;

	if (is_read) {
		if (count == 1)
			code = TRANS_CODE_READ;
		else
			code = TRANS_CODE_SEQ_READ;
	} else {
		if (count == 1)
			code = TRANS_CODE_WRITE;
		else
			code = TRANS_CODE_SEQ_WRITE;
	}

	header = (struct trans_req_header *)br->trans_buf;
	header->code = code;
	header->rsvd = 0;
	header->size = cpu_to_be16((u16)count * SPI_AVMM_VAL_SIZE);
	header->addr = cpu_to_be32(reg);

	trans_len = TRANS_REQ_HD_SIZE;

	if (!is_read) {
		trans_len += SPI_AVMM_VAL_SIZE * count;
		if (trans_len > sizeof(br->trans_buf))
			return -ENOMEM;

		data = (__le32 *)(br->trans_buf + TRANS_REQ_HD_SIZE);

		for (i = 0; i < count; i++)
			*data++ = cpu_to_le32(*wr_val++);
	}

	 
	br->trans_len = trans_len;

	return 0;
}

 
static int br_pkt_phy_tx_prepare(struct spi_avmm_bridge *br)
{
	char *tb, *tb_end, *pb, *pb_limit, *pb_eop = NULL;
	unsigned int aligned_phy_len, move_size;
	bool need_esc = false;

	tb = br->trans_buf;
	tb_end = tb + br->trans_len;
	pb = br->phy_buf;
	pb_limit = pb + ARRAY_SIZE(br->phy_buf);

	*pb++ = PKT_SOP;

	 
	*pb++ = PKT_CHANNEL;
	*pb++ = 0x0;

	for (; pb < pb_limit && tb < tb_end; pb++) {
		if (need_esc) {
			*pb = *tb++ ^ 0x20;
			need_esc = false;
			continue;
		}

		 
		if (tb == tb_end - 1 && !pb_eop) {
			*pb = PKT_EOP;
			pb_eop = pb;
			continue;
		}

		 
		switch (*tb) {
		case PKT_SOP:
		case PKT_EOP:
		case PKT_CHANNEL:
		case PKT_ESC:
			*pb = PKT_ESC;
			need_esc = true;
			break;
		case PHY_IDLE:
		case PHY_ESC:
			*pb = PHY_ESC;
			need_esc = true;
			break;
		default:
			*pb = *tb++;
			break;
		}
	}

	 
	if (tb < tb_end)
		return -ENOMEM;

	 
	br->phy_len = pb - br->phy_buf;

	if (br->word_len == 1)
		return 0;

	 
	aligned_phy_len = ALIGN(br->phy_len, br->word_len);
	if (aligned_phy_len > sizeof(br->phy_buf))
		return -ENOMEM;

	if (aligned_phy_len == br->phy_len)
		return 0;

	 
	move_size = pb - pb_eop;
	memmove(&br->phy_buf[aligned_phy_len - move_size], pb_eop, move_size);

	 
	memset(pb_eop, PHY_IDLE, aligned_phy_len - br->phy_len);

	 
	br->phy_len = aligned_phy_len;

	return 0;
}

 
static int br_do_tx(struct spi_avmm_bridge *br)
{
	 
	if (br->swap_words)
		br->swap_words(br->phy_buf, br->phy_len);

	 
	return spi_write(br->spi, br->phy_buf, br->phy_len);
}

 
static int br_do_rx_and_pkt_phy_parse(struct spi_avmm_bridge *br)
{
	bool eop_found = false, channel_found = false, esc_found = false;
	bool valid_word = false, last_try = false;
	struct device *dev = &br->spi->dev;
	char *pb, *tb_limit, *tb = NULL;
	unsigned long poll_timeout;
	int ret, i;

	tb_limit = br->trans_buf + ARRAY_SIZE(br->trans_buf);
	pb = br->phy_buf;
	poll_timeout = jiffies + SPI_AVMM_XFER_TIMEOUT;
	while (tb < tb_limit) {
		ret = spi_read(br->spi, pb, br->word_len);
		if (ret)
			return ret;

		 
		if (br->swap_words)
			br->swap_words(pb, br->word_len);

		valid_word = false;
		for (i = 0; i < br->word_len; i++) {
			 
			if (!tb && pb[i] != PKT_SOP)
				continue;

			 
			if (pb[i] == PHY_IDLE)
				continue;

			valid_word = true;

			 
			if (channel_found) {
				if (pb[i] != 0) {
					dev_err(dev, "%s channel num != 0\n",
						__func__);
					return -EFAULT;
				}

				channel_found = false;
				continue;
			}

			switch (pb[i]) {
			case PKT_SOP:
				 
				tb = br->trans_buf;
				eop_found = false;
				channel_found = false;
				esc_found = false;
				break;
			case PKT_EOP:
				 
				if (esc_found || eop_found)
					return -EFAULT;

				eop_found = true;
				break;
			case PKT_CHANNEL:
				if (esc_found || eop_found)
					return -EFAULT;

				channel_found = true;
				break;
			case PKT_ESC:
			case PHY_ESC:
				if (esc_found)
					return -EFAULT;

				esc_found = true;
				break;
			default:
				 
				if (esc_found) {
					*tb++ = pb[i] ^ 0x20;
					esc_found = false;
				} else {
					*tb++ = pb[i];
				}

				 
				if (eop_found) {
					br->trans_len = tb - br->trans_buf;
					return 0;
				}
			}
		}

		if (valid_word) {
			 
			poll_timeout = jiffies + SPI_AVMM_XFER_TIMEOUT;
			last_try = false;
		} else {
			 
			if (last_try)
				return -ETIMEDOUT;

			if (time_after(jiffies, poll_timeout))
				last_try = true;
		}
	}

	 
	dev_err(dev, "%s transfer buffer is full but rx doesn't end\n",
		__func__);

	return -EFAULT;
}

 
static int br_rd_trans_rx_parse(struct spi_avmm_bridge *br,
				u32 *val, unsigned int expected_count)
{
	unsigned int i, trans_len = br->trans_len;
	__le32 *data;

	if (expected_count * SPI_AVMM_VAL_SIZE != trans_len)
		return -EFAULT;

	data = (__le32 *)br->trans_buf;
	for (i = 0; i < expected_count; i++)
		*val++ = le32_to_cpu(*data++);

	return 0;
}

 
static int br_wr_trans_rx_parse(struct spi_avmm_bridge *br,
				unsigned int expected_count)
{
	unsigned int trans_len = br->trans_len;
	struct trans_resp_header *resp;
	u8 code;
	u16 val_len;

	if (trans_len != TRANS_RESP_HD_SIZE)
		return -EFAULT;

	resp = (struct trans_resp_header *)br->trans_buf;

	code = resp->r_code ^ 0x80;
	val_len = be16_to_cpu(resp->size);
	if (!val_len || val_len != expected_count * SPI_AVMM_VAL_SIZE)
		return -EFAULT;

	 
	if ((val_len == SPI_AVMM_VAL_SIZE && code != TRANS_CODE_WRITE) ||
	    (val_len > SPI_AVMM_VAL_SIZE && code != TRANS_CODE_SEQ_WRITE))
		return -EFAULT;

	return 0;
}

static int do_reg_access(void *context, bool is_read, unsigned int reg,
			 unsigned int *value, unsigned int count)
{
	struct spi_avmm_bridge *br = context;
	int ret;

	 
	br->trans_len = 0;
	br->phy_len = 0;

	ret = br_trans_tx_prepare(br, is_read, reg, value, count);
	if (ret)
		return ret;

	ret = br_pkt_phy_tx_prepare(br);
	if (ret)
		return ret;

	ret = br_do_tx(br);
	if (ret)
		return ret;

	ret = br_do_rx_and_pkt_phy_parse(br);
	if (ret)
		return ret;

	if (is_read)
		return br_rd_trans_rx_parse(br, value, count);
	else
		return br_wr_trans_rx_parse(br, count);
}

static int regmap_spi_avmm_gather_write(void *context,
					const void *reg_buf, size_t reg_len,
					const void *val_buf, size_t val_len)
{
	if (reg_len != SPI_AVMM_REG_SIZE)
		return -EINVAL;

	if (!IS_ALIGNED(val_len, SPI_AVMM_VAL_SIZE))
		return -EINVAL;

	return do_reg_access(context, false, *(u32 *)reg_buf, (u32 *)val_buf,
			     val_len / SPI_AVMM_VAL_SIZE);
}

static int regmap_spi_avmm_write(void *context, const void *data, size_t bytes)
{
	if (bytes < SPI_AVMM_REG_SIZE + SPI_AVMM_VAL_SIZE)
		return -EINVAL;

	return regmap_spi_avmm_gather_write(context, data, SPI_AVMM_REG_SIZE,
					    data + SPI_AVMM_REG_SIZE,
					    bytes - SPI_AVMM_REG_SIZE);
}

static int regmap_spi_avmm_read(void *context,
				const void *reg_buf, size_t reg_len,
				void *val_buf, size_t val_len)
{
	if (reg_len != SPI_AVMM_REG_SIZE)
		return -EINVAL;

	if (!IS_ALIGNED(val_len, SPI_AVMM_VAL_SIZE))
		return -EINVAL;

	return do_reg_access(context, true, *(u32 *)reg_buf, val_buf,
			     (val_len / SPI_AVMM_VAL_SIZE));
}

static struct spi_avmm_bridge *
spi_avmm_bridge_ctx_gen(struct spi_device *spi)
{
	struct spi_avmm_bridge *br;

	if (!spi)
		return ERR_PTR(-ENODEV);

	 
	spi->mode = SPI_MODE_1;
	spi->bits_per_word = 32;
	if (spi_setup(spi)) {
		spi->bits_per_word = 8;
		if (spi_setup(spi))
			return ERR_PTR(-EINVAL);
	}

	br = kzalloc(sizeof(*br), GFP_KERNEL);
	if (!br)
		return ERR_PTR(-ENOMEM);

	br->spi = spi;
	br->word_len = spi->bits_per_word / 8;
	if (br->word_len == 4) {
		 
		br->swap_words = br_swap_words_32;
	}

	return br;
}

static void spi_avmm_bridge_ctx_free(void *context)
{
	kfree(context);
}

static const struct regmap_bus regmap_spi_avmm_bus = {
	.write = regmap_spi_avmm_write,
	.gather_write = regmap_spi_avmm_gather_write,
	.read = regmap_spi_avmm_read,
	.reg_format_endian_default = REGMAP_ENDIAN_NATIVE,
	.val_format_endian_default = REGMAP_ENDIAN_NATIVE,
	.max_raw_read = SPI_AVMM_VAL_SIZE * MAX_READ_CNT,
	.max_raw_write = SPI_AVMM_VAL_SIZE * MAX_WRITE_CNT,
	.free_context = spi_avmm_bridge_ctx_free,
};

struct regmap *__regmap_init_spi_avmm(struct spi_device *spi,
				      const struct regmap_config *config,
				      struct lock_class_key *lock_key,
				      const char *lock_name)
{
	struct spi_avmm_bridge *bridge;
	struct regmap *map;

	bridge = spi_avmm_bridge_ctx_gen(spi);
	if (IS_ERR(bridge))
		return ERR_CAST(bridge);

	map = __regmap_init(&spi->dev, &regmap_spi_avmm_bus,
			    bridge, config, lock_key, lock_name);
	if (IS_ERR(map)) {
		spi_avmm_bridge_ctx_free(bridge);
		return ERR_CAST(map);
	}

	return map;
}
EXPORT_SYMBOL_GPL(__regmap_init_spi_avmm);

struct regmap *__devm_regmap_init_spi_avmm(struct spi_device *spi,
					   const struct regmap_config *config,
					   struct lock_class_key *lock_key,
					   const char *lock_name)
{
	struct spi_avmm_bridge *bridge;
	struct regmap *map;

	bridge = spi_avmm_bridge_ctx_gen(spi);
	if (IS_ERR(bridge))
		return ERR_CAST(bridge);

	map = __devm_regmap_init(&spi->dev, &regmap_spi_avmm_bus,
				 bridge, config, lock_key, lock_name);
	if (IS_ERR(map)) {
		spi_avmm_bridge_ctx_free(bridge);
		return ERR_CAST(map);
	}

	return map;
}
EXPORT_SYMBOL_GPL(__devm_regmap_init_spi_avmm);

MODULE_LICENSE("GPL v2");
