
 

#include <linux/bitfield.h>
#include <linux/dma-mapping.h>

#include "meson_drv.h"
#include "meson_registers.h"
#include "meson_rdma.h"

 

#define RDMA_DESC_SIZE	(sizeof(uint32_t) * 2)

int meson_rdma_init(struct meson_drm *priv)
{
	if (!priv->rdma.addr) {
		 
		priv->rdma.addr =
			dma_alloc_coherent(priv->dev, SZ_4K,
					   &priv->rdma.addr_dma,
					   GFP_KERNEL);
		if (!priv->rdma.addr)
			return -ENOMEM;
	}

	priv->rdma.offset = 0;

	writel_relaxed(RDMA_CTRL_SW_RESET,
		       priv->io_base + _REG(RDMA_CTRL));
	writel_relaxed(RDMA_DEFAULT_CONFIG |
		       FIELD_PREP(RDMA_CTRL_AHB_WR_BURST, 3) |
		       FIELD_PREP(RDMA_CTRL_AHB_RD_BURST, 0),
		       priv->io_base + _REG(RDMA_CTRL));

	return 0;
}

void meson_rdma_free(struct meson_drm *priv)
{
	if (!priv->rdma.addr && !priv->rdma.addr_dma)
		return;

	meson_rdma_stop(priv);

	dma_free_coherent(priv->dev, SZ_4K,
			  priv->rdma.addr, priv->rdma.addr_dma);

	priv->rdma.addr = NULL;
	priv->rdma.addr_dma = (dma_addr_t)0;
}

void meson_rdma_setup(struct meson_drm *priv)
{
	 
	writel_bits_relaxed(RDMA_ACCESS_RW_FLAG_CHAN1 |
			    RDMA_ACCESS_ADDR_INC_CHAN1,
			    RDMA_ACCESS_RW_FLAG_CHAN1,
			    priv->io_base + _REG(RDMA_ACCESS_AUTO));
}

void meson_rdma_stop(struct meson_drm *priv)
{
	writel_bits_relaxed(RDMA_IRQ_CLEAR_CHAN1,
			    RDMA_IRQ_CLEAR_CHAN1,
			    priv->io_base + _REG(RDMA_CTRL));

	 
	writel_bits_relaxed(RDMA_ACCESS_TRIGGER_CHAN1,
			    FIELD_PREP(RDMA_ACCESS_ADDR_INC_CHAN1,
				       RDMA_ACCESS_TRIGGER_STOP),
			    priv->io_base + _REG(RDMA_ACCESS_AUTO));
}

void meson_rdma_reset(struct meson_drm *priv)
{
	meson_rdma_stop(priv);

	priv->rdma.offset = 0;
}

static void meson_rdma_writel(struct meson_drm *priv, uint32_t val,
			      uint32_t reg)
{
	if (priv->rdma.offset >= (SZ_4K / RDMA_DESC_SIZE)) {
		dev_warn_once(priv->dev, "%s: overflow\n", __func__);
		return;
	}

	priv->rdma.addr[priv->rdma.offset++] = reg;
	priv->rdma.addr[priv->rdma.offset++] = val;
}

 
void meson_rdma_writel_sync(struct meson_drm *priv, uint32_t val, uint32_t reg)
{
	meson_rdma_writel(priv, val, reg);

	writel_relaxed(val, priv->io_base + _REG(reg));
}

void meson_rdma_flush(struct meson_drm *priv)
{
	meson_rdma_stop(priv);

	 
	writel(priv->rdma.addr_dma,
	       priv->io_base + _REG(RDMA_AHB_START_ADDR_1));

	 
	writel(priv->rdma.addr_dma + (priv->rdma.offset * RDMA_DESC_SIZE) - 1,
	       priv->io_base + _REG(RDMA_AHB_END_ADDR_1));

	 
	writel_bits_relaxed(RDMA_ACCESS_TRIGGER_CHAN1,
			    FIELD_PREP(RDMA_ACCESS_TRIGGER_CHAN1,
				       RDMA_ACCESS_TRIGGER_VSYNC),
			    priv->io_base + _REG(RDMA_ACCESS_AUTO));

	priv->rdma.offset = 0;
}
