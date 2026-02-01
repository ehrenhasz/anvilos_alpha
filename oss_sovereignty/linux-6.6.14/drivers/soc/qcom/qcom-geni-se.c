


 
#define __DISABLE_TRACE_MMIO__

#include <linux/acpi.h>
#include <linux/clk.h>
#include <linux/slab.h>
#include <linux/dma-mapping.h>
#include <linux/io.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/of_platform.h>
#include <linux/pinctrl/consumer.h>
#include <linux/platform_device.h>
#include <linux/soc/qcom/geni-se.h>

 

 

 

#define MAX_CLK_PERF_LEVEL 32
#define MAX_CLKS 2

 
struct geni_wrapper {
	struct device *dev;
	void __iomem *base;
	struct clk_bulk_data clks[MAX_CLKS];
	unsigned int num_clks;
};

 
struct geni_se_desc {
	unsigned int num_clks;
	const char * const *clks;
};

static const char * const icc_path_names[] = {"qup-core", "qup-config",
						"qup-memory"};

#define QUP_HW_VER_REG			0x4

 
#define GENI_INIT_CFG_REVISION		0x0
#define GENI_S_INIT_CFG_REVISION	0x4
#define GENI_OUTPUT_CTRL		0x24
#define GENI_CGC_CTRL			0x28
#define GENI_CLK_CTRL_RO		0x60
#define GENI_FW_S_REVISION_RO		0x6c
#define SE_GENI_BYTE_GRAN		0x254
#define SE_GENI_TX_PACKING_CFG0		0x260
#define SE_GENI_TX_PACKING_CFG1		0x264
#define SE_GENI_RX_PACKING_CFG0		0x284
#define SE_GENI_RX_PACKING_CFG1		0x288
#define SE_GENI_M_GP_LENGTH		0x910
#define SE_GENI_S_GP_LENGTH		0x914
#define SE_DMA_TX_PTR_L			0xc30
#define SE_DMA_TX_PTR_H			0xc34
#define SE_DMA_TX_ATTR			0xc38
#define SE_DMA_TX_LEN			0xc3c
#define SE_DMA_TX_IRQ_EN		0xc48
#define SE_DMA_TX_IRQ_EN_SET		0xc4c
#define SE_DMA_TX_IRQ_EN_CLR		0xc50
#define SE_DMA_TX_LEN_IN		0xc54
#define SE_DMA_TX_MAX_BURST		0xc5c
#define SE_DMA_RX_PTR_L			0xd30
#define SE_DMA_RX_PTR_H			0xd34
#define SE_DMA_RX_ATTR			0xd38
#define SE_DMA_RX_LEN			0xd3c
#define SE_DMA_RX_IRQ_EN		0xd48
#define SE_DMA_RX_IRQ_EN_SET		0xd4c
#define SE_DMA_RX_IRQ_EN_CLR		0xd50
#define SE_DMA_RX_LEN_IN		0xd54
#define SE_DMA_RX_MAX_BURST		0xd5c
#define SE_DMA_RX_FLUSH			0xd60
#define SE_GSI_EVENT_EN			0xe18
#define SE_IRQ_EN			0xe1c
#define SE_DMA_GENERAL_CFG		0xe30

 
#define DEFAULT_IO_OUTPUT_CTRL_MSK	GENMASK(6, 0)

 
#define CFG_AHB_CLK_CGC_ON		BIT(0)
#define CFG_AHB_WR_ACLK_CGC_ON		BIT(1)
#define DATA_AHB_CLK_CGC_ON		BIT(2)
#define SCLK_CGC_ON			BIT(3)
#define TX_CLK_CGC_ON			BIT(4)
#define RX_CLK_CGC_ON			BIT(5)
#define EXT_CLK_CGC_ON			BIT(6)
#define PROG_RAM_HCLK_OFF		BIT(8)
#define PROG_RAM_SCLK_OFF		BIT(9)
#define DEFAULT_CGC_EN			GENMASK(6, 0)

 
#define DMA_RX_EVENT_EN			BIT(0)
#define DMA_TX_EVENT_EN			BIT(1)
#define GENI_M_EVENT_EN			BIT(2)
#define GENI_S_EVENT_EN			BIT(3)

 
#define DMA_RX_IRQ_EN			BIT(0)
#define DMA_TX_IRQ_EN			BIT(1)
#define GENI_M_IRQ_EN			BIT(2)
#define GENI_S_IRQ_EN			BIT(3)

 
#define DMA_RX_CLK_CGC_ON		BIT(0)
#define DMA_TX_CLK_CGC_ON		BIT(1)
#define DMA_AHB_SLV_CFG_ON		BIT(2)
#define AHB_SEC_SLV_CLK_CGC_ON		BIT(3)
#define DUMMY_RX_NON_BUFFERABLE		BIT(4)
#define RX_DMA_ZERO_PADDING_EN		BIT(5)
#define RX_DMA_IRQ_DELAY_MSK		GENMASK(8, 6)
#define RX_DMA_IRQ_DELAY_SHFT		6

 
u32 geni_se_get_qup_hw_version(struct geni_se *se)
{
	struct geni_wrapper *wrapper = se->wrapper;

	return readl_relaxed(wrapper->base + QUP_HW_VER_REG);
}
EXPORT_SYMBOL(geni_se_get_qup_hw_version);

static void geni_se_io_set_mode(void __iomem *base)
{
	u32 val;

	val = readl_relaxed(base + SE_IRQ_EN);
	val |= GENI_M_IRQ_EN | GENI_S_IRQ_EN;
	val |= DMA_TX_IRQ_EN | DMA_RX_IRQ_EN;
	writel_relaxed(val, base + SE_IRQ_EN);

	val = readl_relaxed(base + SE_GENI_DMA_MODE_EN);
	val &= ~GENI_DMA_MODE_EN;
	writel_relaxed(val, base + SE_GENI_DMA_MODE_EN);

	writel_relaxed(0, base + SE_GSI_EVENT_EN);
}

static void geni_se_io_init(void __iomem *base)
{
	u32 val;

	val = readl_relaxed(base + GENI_CGC_CTRL);
	val |= DEFAULT_CGC_EN;
	writel_relaxed(val, base + GENI_CGC_CTRL);

	val = readl_relaxed(base + SE_DMA_GENERAL_CFG);
	val |= AHB_SEC_SLV_CLK_CGC_ON | DMA_AHB_SLV_CFG_ON;
	val |= DMA_TX_CLK_CGC_ON | DMA_RX_CLK_CGC_ON;
	writel_relaxed(val, base + SE_DMA_GENERAL_CFG);

	writel_relaxed(DEFAULT_IO_OUTPUT_CTRL_MSK, base + GENI_OUTPUT_CTRL);
	writel_relaxed(FORCE_DEFAULT, base + GENI_FORCE_DEFAULT_REG);
}

static void geni_se_irq_clear(struct geni_se *se)
{
	writel_relaxed(0, se->base + SE_GSI_EVENT_EN);
	writel_relaxed(0xffffffff, se->base + SE_GENI_M_IRQ_CLEAR);
	writel_relaxed(0xffffffff, se->base + SE_GENI_S_IRQ_CLEAR);
	writel_relaxed(0xffffffff, se->base + SE_DMA_TX_IRQ_CLR);
	writel_relaxed(0xffffffff, se->base + SE_DMA_RX_IRQ_CLR);
	writel_relaxed(0xffffffff, se->base + SE_IRQ_EN);
}

 
void geni_se_init(struct geni_se *se, u32 rx_wm, u32 rx_rfr)
{
	u32 val;

	geni_se_irq_clear(se);
	geni_se_io_init(se->base);
	geni_se_io_set_mode(se->base);

	writel_relaxed(rx_wm, se->base + SE_GENI_RX_WATERMARK_REG);
	writel_relaxed(rx_rfr, se->base + SE_GENI_RX_RFR_WATERMARK_REG);

	val = readl_relaxed(se->base + SE_GENI_M_IRQ_EN);
	val |= M_COMMON_GENI_M_IRQ_EN;
	writel_relaxed(val, se->base + SE_GENI_M_IRQ_EN);

	val = readl_relaxed(se->base + SE_GENI_S_IRQ_EN);
	val |= S_COMMON_GENI_S_IRQ_EN;
	writel_relaxed(val, se->base + SE_GENI_S_IRQ_EN);
}
EXPORT_SYMBOL(geni_se_init);

static void geni_se_select_fifo_mode(struct geni_se *se)
{
	u32 proto = geni_se_read_proto(se);
	u32 val, val_old;

	geni_se_irq_clear(se);

	 
	if (proto != GENI_SE_UART) {
		 
		val_old = val = readl_relaxed(se->base + SE_GENI_M_IRQ_EN);
		val |= M_CMD_DONE_EN | M_TX_FIFO_WATERMARK_EN;
		val |= M_RX_FIFO_WATERMARK_EN | M_RX_FIFO_LAST_EN;
		if (val != val_old)
			writel_relaxed(val, se->base + SE_GENI_M_IRQ_EN);
	}

	val_old = val = readl_relaxed(se->base + SE_GENI_DMA_MODE_EN);
	val &= ~GENI_DMA_MODE_EN;
	if (val != val_old)
		writel_relaxed(val, se->base + SE_GENI_DMA_MODE_EN);
}

static void geni_se_select_dma_mode(struct geni_se *se)
{
	u32 proto = geni_se_read_proto(se);
	u32 val, val_old;

	geni_se_irq_clear(se);

	 
	if (proto != GENI_SE_UART) {
		 
		val_old = val = readl_relaxed(se->base + SE_GENI_M_IRQ_EN);
		val &= ~(M_CMD_DONE_EN | M_TX_FIFO_WATERMARK_EN);
		val &= ~(M_RX_FIFO_WATERMARK_EN | M_RX_FIFO_LAST_EN);
		if (val != val_old)
			writel_relaxed(val, se->base + SE_GENI_M_IRQ_EN);
	}

	val_old = val = readl_relaxed(se->base + SE_GENI_DMA_MODE_EN);
	val |= GENI_DMA_MODE_EN;
	if (val != val_old)
		writel_relaxed(val, se->base + SE_GENI_DMA_MODE_EN);
}

static void geni_se_select_gpi_mode(struct geni_se *se)
{
	u32 val;

	geni_se_irq_clear(se);

	writel(0, se->base + SE_IRQ_EN);

	val = readl(se->base + SE_GENI_M_IRQ_EN);
	val &= ~(M_CMD_DONE_EN | M_TX_FIFO_WATERMARK_EN |
		 M_RX_FIFO_WATERMARK_EN | M_RX_FIFO_LAST_EN);
	writel(val, se->base + SE_GENI_M_IRQ_EN);

	writel(GENI_DMA_MODE_EN, se->base + SE_GENI_DMA_MODE_EN);

	val = readl(se->base + SE_GSI_EVENT_EN);
	val |= (DMA_RX_EVENT_EN | DMA_TX_EVENT_EN | GENI_M_EVENT_EN | GENI_S_EVENT_EN);
	writel(val, se->base + SE_GSI_EVENT_EN);
}

 
void geni_se_select_mode(struct geni_se *se, enum geni_se_xfer_mode mode)
{
	WARN_ON(mode != GENI_SE_FIFO && mode != GENI_SE_DMA && mode != GENI_GPI_DMA);

	switch (mode) {
	case GENI_SE_FIFO:
		geni_se_select_fifo_mode(se);
		break;
	case GENI_SE_DMA:
		geni_se_select_dma_mode(se);
		break;
	case GENI_GPI_DMA:
		geni_se_select_gpi_mode(se);
		break;
	case GENI_SE_INVALID:
	default:
		break;
	}
}
EXPORT_SYMBOL(geni_se_select_mode);

 

#define NUM_PACKING_VECTORS 4
#define PACKING_START_SHIFT 5
#define PACKING_DIR_SHIFT 4
#define PACKING_LEN_SHIFT 1
#define PACKING_STOP_BIT BIT(0)
#define PACKING_VECTOR_SHIFT 10
 
void geni_se_config_packing(struct geni_se *se, int bpw, int pack_words,
			    bool msb_to_lsb, bool tx_cfg, bool rx_cfg)
{
	u32 cfg0, cfg1, cfg[NUM_PACKING_VECTORS] = {0};
	int len;
	int temp_bpw = bpw;
	int idx_start = msb_to_lsb ? bpw - 1 : 0;
	int idx = idx_start;
	int idx_delta = msb_to_lsb ? -BITS_PER_BYTE : BITS_PER_BYTE;
	int ceil_bpw = ALIGN(bpw, BITS_PER_BYTE);
	int iter = (ceil_bpw * pack_words) / BITS_PER_BYTE;
	int i;

	if (iter <= 0 || iter > NUM_PACKING_VECTORS)
		return;

	for (i = 0; i < iter; i++) {
		len = min_t(int, temp_bpw, BITS_PER_BYTE) - 1;
		cfg[i] = idx << PACKING_START_SHIFT;
		cfg[i] |= msb_to_lsb << PACKING_DIR_SHIFT;
		cfg[i] |= len << PACKING_LEN_SHIFT;

		if (temp_bpw <= BITS_PER_BYTE) {
			idx = ((i + 1) * BITS_PER_BYTE) + idx_start;
			temp_bpw = bpw;
		} else {
			idx = idx + idx_delta;
			temp_bpw = temp_bpw - BITS_PER_BYTE;
		}
	}
	cfg[iter - 1] |= PACKING_STOP_BIT;
	cfg0 = cfg[0] | (cfg[1] << PACKING_VECTOR_SHIFT);
	cfg1 = cfg[2] | (cfg[3] << PACKING_VECTOR_SHIFT);

	if (tx_cfg) {
		writel_relaxed(cfg0, se->base + SE_GENI_TX_PACKING_CFG0);
		writel_relaxed(cfg1, se->base + SE_GENI_TX_PACKING_CFG1);
	}
	if (rx_cfg) {
		writel_relaxed(cfg0, se->base + SE_GENI_RX_PACKING_CFG0);
		writel_relaxed(cfg1, se->base + SE_GENI_RX_PACKING_CFG1);
	}

	 
	if (pack_words || bpw == 32)
		writel_relaxed(bpw / 16, se->base + SE_GENI_BYTE_GRAN);
}
EXPORT_SYMBOL(geni_se_config_packing);

static void geni_se_clks_off(struct geni_se *se)
{
	struct geni_wrapper *wrapper = se->wrapper;

	clk_disable_unprepare(se->clk);
	clk_bulk_disable_unprepare(wrapper->num_clks, wrapper->clks);
}

 
int geni_se_resources_off(struct geni_se *se)
{
	int ret;

	if (has_acpi_companion(se->dev))
		return 0;

	ret = pinctrl_pm_select_sleep_state(se->dev);
	if (ret)
		return ret;

	geni_se_clks_off(se);
	return 0;
}
EXPORT_SYMBOL(geni_se_resources_off);

static int geni_se_clks_on(struct geni_se *se)
{
	int ret;
	struct geni_wrapper *wrapper = se->wrapper;

	ret = clk_bulk_prepare_enable(wrapper->num_clks, wrapper->clks);
	if (ret)
		return ret;

	ret = clk_prepare_enable(se->clk);
	if (ret)
		clk_bulk_disable_unprepare(wrapper->num_clks, wrapper->clks);
	return ret;
}

 
int geni_se_resources_on(struct geni_se *se)
{
	int ret;

	if (has_acpi_companion(se->dev))
		return 0;

	ret = geni_se_clks_on(se);
	if (ret)
		return ret;

	ret = pinctrl_pm_select_default_state(se->dev);
	if (ret)
		geni_se_clks_off(se);

	return ret;
}
EXPORT_SYMBOL(geni_se_resources_on);

 
int geni_se_clk_tbl_get(struct geni_se *se, unsigned long **tbl)
{
	long freq = 0;
	int i;

	if (se->clk_perf_tbl) {
		*tbl = se->clk_perf_tbl;
		return se->num_clk_levels;
	}

	se->clk_perf_tbl = devm_kcalloc(se->dev, MAX_CLK_PERF_LEVEL,
					sizeof(*se->clk_perf_tbl),
					GFP_KERNEL);
	if (!se->clk_perf_tbl)
		return -ENOMEM;

	for (i = 0; i < MAX_CLK_PERF_LEVEL; i++) {
		freq = clk_round_rate(se->clk, freq + 1);
		if (freq <= 0 || freq == se->clk_perf_tbl[i - 1])
			break;
		se->clk_perf_tbl[i] = freq;
	}
	se->num_clk_levels = i;
	*tbl = se->clk_perf_tbl;
	return se->num_clk_levels;
}
EXPORT_SYMBOL(geni_se_clk_tbl_get);

 
int geni_se_clk_freq_match(struct geni_se *se, unsigned long req_freq,
			   unsigned int *index, unsigned long *res_freq,
			   bool exact)
{
	unsigned long *tbl;
	int num_clk_levels;
	int i;
	unsigned long best_delta;
	unsigned long new_delta;
	unsigned int divider;

	num_clk_levels = geni_se_clk_tbl_get(se, &tbl);
	if (num_clk_levels < 0)
		return num_clk_levels;

	if (num_clk_levels == 0)
		return -EINVAL;

	best_delta = ULONG_MAX;
	for (i = 0; i < num_clk_levels; i++) {
		divider = DIV_ROUND_UP(tbl[i], req_freq);
		new_delta = req_freq - tbl[i] / divider;
		if (new_delta < best_delta) {
			 
			*index = i;
			*res_freq = tbl[i];

			 
			if (new_delta == 0)
				return 0;

			 
			best_delta = new_delta;
		}
	}

	if (exact)
		return -EINVAL;

	return 0;
}
EXPORT_SYMBOL(geni_se_clk_freq_match);

#define GENI_SE_DMA_DONE_EN BIT(0)
#define GENI_SE_DMA_EOT_EN BIT(1)
#define GENI_SE_DMA_AHB_ERR_EN BIT(2)
#define GENI_SE_DMA_EOT_BUF BIT(0)

 
void geni_se_tx_init_dma(struct geni_se *se, dma_addr_t iova, size_t len)
{
	u32 val;

	val = GENI_SE_DMA_DONE_EN;
	val |= GENI_SE_DMA_EOT_EN;
	val |= GENI_SE_DMA_AHB_ERR_EN;
	writel_relaxed(val, se->base + SE_DMA_TX_IRQ_EN_SET);
	writel_relaxed(lower_32_bits(iova), se->base + SE_DMA_TX_PTR_L);
	writel_relaxed(upper_32_bits(iova), se->base + SE_DMA_TX_PTR_H);
	writel_relaxed(GENI_SE_DMA_EOT_BUF, se->base + SE_DMA_TX_ATTR);
	writel(len, se->base + SE_DMA_TX_LEN);
}
EXPORT_SYMBOL(geni_se_tx_init_dma);

 
int geni_se_tx_dma_prep(struct geni_se *se, void *buf, size_t len,
			dma_addr_t *iova)
{
	struct geni_wrapper *wrapper = se->wrapper;

	if (!wrapper)
		return -EINVAL;

	*iova = dma_map_single(wrapper->dev, buf, len, DMA_TO_DEVICE);
	if (dma_mapping_error(wrapper->dev, *iova))
		return -EIO;

	geni_se_tx_init_dma(se, *iova, len);
	return 0;
}
EXPORT_SYMBOL(geni_se_tx_dma_prep);

 
void geni_se_rx_init_dma(struct geni_se *se, dma_addr_t iova, size_t len)
{
	u32 val;

	val = GENI_SE_DMA_DONE_EN;
	val |= GENI_SE_DMA_EOT_EN;
	val |= GENI_SE_DMA_AHB_ERR_EN;
	writel_relaxed(val, se->base + SE_DMA_RX_IRQ_EN_SET);
	writel_relaxed(lower_32_bits(iova), se->base + SE_DMA_RX_PTR_L);
	writel_relaxed(upper_32_bits(iova), se->base + SE_DMA_RX_PTR_H);
	 
	writel_relaxed(0, se->base + SE_DMA_RX_ATTR);
	writel(len, se->base + SE_DMA_RX_LEN);
}
EXPORT_SYMBOL(geni_se_rx_init_dma);

 
int geni_se_rx_dma_prep(struct geni_se *se, void *buf, size_t len,
			dma_addr_t *iova)
{
	struct geni_wrapper *wrapper = se->wrapper;

	if (!wrapper)
		return -EINVAL;

	*iova = dma_map_single(wrapper->dev, buf, len, DMA_FROM_DEVICE);
	if (dma_mapping_error(wrapper->dev, *iova))
		return -EIO;

	geni_se_rx_init_dma(se, *iova, len);
	return 0;
}
EXPORT_SYMBOL(geni_se_rx_dma_prep);

 
void geni_se_tx_dma_unprep(struct geni_se *se, dma_addr_t iova, size_t len)
{
	struct geni_wrapper *wrapper = se->wrapper;

	if (!dma_mapping_error(wrapper->dev, iova))
		dma_unmap_single(wrapper->dev, iova, len, DMA_TO_DEVICE);
}
EXPORT_SYMBOL(geni_se_tx_dma_unprep);

 
void geni_se_rx_dma_unprep(struct geni_se *se, dma_addr_t iova, size_t len)
{
	struct geni_wrapper *wrapper = se->wrapper;

	if (!dma_mapping_error(wrapper->dev, iova))
		dma_unmap_single(wrapper->dev, iova, len, DMA_FROM_DEVICE);
}
EXPORT_SYMBOL(geni_se_rx_dma_unprep);

int geni_icc_get(struct geni_se *se, const char *icc_ddr)
{
	int i, err;
	const char *icc_names[] = {"qup-core", "qup-config", icc_ddr};

	if (has_acpi_companion(se->dev))
		return 0;

	for (i = 0; i < ARRAY_SIZE(se->icc_paths); i++) {
		if (!icc_names[i])
			continue;

		se->icc_paths[i].path = devm_of_icc_get(se->dev, icc_names[i]);
		if (IS_ERR(se->icc_paths[i].path))
			goto err;
	}

	return 0;

err:
	err = PTR_ERR(se->icc_paths[i].path);
	if (err != -EPROBE_DEFER)
		dev_err_ratelimited(se->dev, "Failed to get ICC path '%s': %d\n",
					icc_names[i], err);
	return err;

}
EXPORT_SYMBOL(geni_icc_get);

int geni_icc_set_bw(struct geni_se *se)
{
	int i, ret;

	for (i = 0; i < ARRAY_SIZE(se->icc_paths); i++) {
		ret = icc_set_bw(se->icc_paths[i].path,
			se->icc_paths[i].avg_bw, se->icc_paths[i].avg_bw);
		if (ret) {
			dev_err_ratelimited(se->dev, "ICC BW voting failed on path '%s': %d\n",
					icc_path_names[i], ret);
			return ret;
		}
	}

	return 0;
}
EXPORT_SYMBOL(geni_icc_set_bw);

void geni_icc_set_tag(struct geni_se *se, u32 tag)
{
	int i;

	for (i = 0; i < ARRAY_SIZE(se->icc_paths); i++)
		icc_set_tag(se->icc_paths[i].path, tag);
}
EXPORT_SYMBOL(geni_icc_set_tag);

 
int geni_icc_enable(struct geni_se *se)
{
	int i, ret;

	for (i = 0; i < ARRAY_SIZE(se->icc_paths); i++) {
		ret = icc_enable(se->icc_paths[i].path);
		if (ret) {
			dev_err_ratelimited(se->dev, "ICC enable failed on path '%s': %d\n",
					icc_path_names[i], ret);
			return ret;
		}
	}

	return 0;
}
EXPORT_SYMBOL(geni_icc_enable);

int geni_icc_disable(struct geni_se *se)
{
	int i, ret;

	for (i = 0; i < ARRAY_SIZE(se->icc_paths); i++) {
		ret = icc_disable(se->icc_paths[i].path);
		if (ret) {
			dev_err_ratelimited(se->dev, "ICC disable failed on path '%s': %d\n",
					icc_path_names[i], ret);
			return ret;
		}
	}

	return 0;
}
EXPORT_SYMBOL(geni_icc_disable);

static int geni_se_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct geni_wrapper *wrapper;
	int ret;

	wrapper = devm_kzalloc(dev, sizeof(*wrapper), GFP_KERNEL);
	if (!wrapper)
		return -ENOMEM;

	wrapper->dev = dev;
	wrapper->base = devm_platform_ioremap_resource(pdev, 0);
	if (IS_ERR(wrapper->base))
		return PTR_ERR(wrapper->base);

	if (!has_acpi_companion(&pdev->dev)) {
		const struct geni_se_desc *desc;
		int i;

		desc = device_get_match_data(&pdev->dev);
		if (!desc)
			return -EINVAL;

		wrapper->num_clks = min_t(unsigned int, desc->num_clks, MAX_CLKS);

		for (i = 0; i < wrapper->num_clks; ++i)
			wrapper->clks[i].id = desc->clks[i];

		ret = of_count_phandle_with_args(dev->of_node, "clocks", "#clock-cells");
		if (ret < 0) {
			dev_err(dev, "invalid clocks property at %pOF\n", dev->of_node);
			return ret;
		}

		if (ret < wrapper->num_clks) {
			dev_err(dev, "invalid clocks count at %pOF, expected %d entries\n",
				dev->of_node, wrapper->num_clks);
			return -EINVAL;
		}

		ret = devm_clk_bulk_get(dev, wrapper->num_clks, wrapper->clks);
		if (ret) {
			dev_err(dev, "Err getting clks %d\n", ret);
			return ret;
		}
	}

	dev_set_drvdata(dev, wrapper);
	dev_dbg(dev, "GENI SE Driver probed\n");
	return devm_of_platform_populate(dev);
}

static const char * const qup_clks[] = {
	"m-ahb",
	"s-ahb",
};

static const struct geni_se_desc qup_desc = {
	.clks = qup_clks,
	.num_clks = ARRAY_SIZE(qup_clks),
};

static const char * const i2c_master_hub_clks[] = {
	"s-ahb",
};

static const struct geni_se_desc i2c_master_hub_desc = {
	.clks = i2c_master_hub_clks,
	.num_clks = ARRAY_SIZE(i2c_master_hub_clks),
};

static const struct of_device_id geni_se_dt_match[] = {
	{ .compatible = "qcom,geni-se-qup", .data = &qup_desc },
	{ .compatible = "qcom,geni-se-i2c-master-hub", .data = &i2c_master_hub_desc },
	{}
};
MODULE_DEVICE_TABLE(of, geni_se_dt_match);

static struct platform_driver geni_se_driver = {
	.driver = {
		.name = "geni_se_qup",
		.of_match_table = geni_se_dt_match,
	},
	.probe = geni_se_probe,
};
module_platform_driver(geni_se_driver);

MODULE_DESCRIPTION("GENI Serial Engine Driver");
MODULE_LICENSE("GPL v2");
