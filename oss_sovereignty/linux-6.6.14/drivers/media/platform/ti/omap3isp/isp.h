 
 

#ifndef OMAP3_ISP_CORE_H
#define OMAP3_ISP_CORE_H

#include <media/media-entity.h>
#include <media/v4l2-async.h>
#include <media/v4l2-device.h>
#include <linux/clk-provider.h>
#include <linux/device.h>
#include <linux/io.h>
#include <linux/platform_device.h>
#include <linux/wait.h>

#include "omap3isp.h"
#include "ispstat.h"
#include "ispccdc.h"
#include "ispreg.h"
#include "ispresizer.h"
#include "isppreview.h"
#include "ispcsiphy.h"
#include "ispcsi2.h"
#include "ispccp2.h"

#define ISP_TOK_TERM		0xFFFFFFFF	 
#define to_isp_device(ptr_module)				\
	container_of(ptr_module, struct isp_device, isp_##ptr_module)
#define to_device(ptr_module)						\
	(to_isp_device(ptr_module)->dev)

enum isp_mem_resources {
	OMAP3_ISP_IOMEM_MAIN,
	OMAP3_ISP_IOMEM_CCP2,
	OMAP3_ISP_IOMEM_CCDC,
	OMAP3_ISP_IOMEM_HIST,
	OMAP3_ISP_IOMEM_H3A,
	OMAP3_ISP_IOMEM_PREV,
	OMAP3_ISP_IOMEM_RESZ,
	OMAP3_ISP_IOMEM_SBL,
	OMAP3_ISP_IOMEM_CSI2A_REGS1,
	OMAP3_ISP_IOMEM_CSIPHY2,
	OMAP3_ISP_IOMEM_CSI2A_REGS2,
	OMAP3_ISP_IOMEM_CSI2C_REGS1,
	OMAP3_ISP_IOMEM_CSIPHY1,
	OMAP3_ISP_IOMEM_CSI2C_REGS2,
	OMAP3_ISP_IOMEM_LAST
};

enum isp_sbl_resource {
	OMAP3_ISP_SBL_CSI1_READ		= 0x1,
	OMAP3_ISP_SBL_CSI1_WRITE	= 0x2,
	OMAP3_ISP_SBL_CSI2A_WRITE	= 0x4,
	OMAP3_ISP_SBL_CSI2C_WRITE	= 0x8,
	OMAP3_ISP_SBL_CCDC_LSC_READ	= 0x10,
	OMAP3_ISP_SBL_CCDC_WRITE	= 0x20,
	OMAP3_ISP_SBL_PREVIEW_READ	= 0x40,
	OMAP3_ISP_SBL_PREVIEW_WRITE	= 0x80,
	OMAP3_ISP_SBL_RESIZER_READ	= 0x100,
	OMAP3_ISP_SBL_RESIZER_WRITE	= 0x200,
};

enum isp_subclk_resource {
	OMAP3_ISP_SUBCLK_CCDC		= (1 << 0),
	OMAP3_ISP_SUBCLK_AEWB		= (1 << 1),
	OMAP3_ISP_SUBCLK_AF		= (1 << 2),
	OMAP3_ISP_SUBCLK_HIST		= (1 << 3),
	OMAP3_ISP_SUBCLK_PREVIEW	= (1 << 4),
	OMAP3_ISP_SUBCLK_RESIZER	= (1 << 5),
};

 
#define ISP_REVISION_1_0		0x10
 
#define ISP_REVISION_2_0		0x20
 
#define ISP_REVISION_15_0		0xF0

#define ISP_PHY_TYPE_3430		0
#define ISP_PHY_TYPE_3630		1

struct regmap;

 
struct isp_res_mapping {
	u32 isp_rev;
	u32 offset[OMAP3_ISP_IOMEM_LAST];
	u32 phy_type;
};

 
struct isp_reg {
	enum isp_mem_resources mmio_range;
	u32 reg;
	u32 val;
};

enum isp_xclk_id {
	ISP_XCLK_A,
	ISP_XCLK_B,
};

struct isp_xclk {
	struct isp_device *isp;
	struct clk_hw hw;
	struct clk *clk;
	enum isp_xclk_id id;

	spinlock_t lock;	 
	bool enabled;
	unsigned int divider;
};

 
struct isp_device {
	struct v4l2_device v4l2_dev;
	struct v4l2_async_notifier notifier;
	struct media_device media_dev;
	struct device *dev;
	u32 revision;

	 
	unsigned int irq_num;

	void __iomem *mmio_base[OMAP3_ISP_IOMEM_LAST];
	unsigned long mmio_hist_base_phys;
	struct regmap *syscon;
	u32 syscon_offset;
	u32 phy_type;

	struct dma_iommu_mapping *mapping;

	 
	spinlock_t stat_lock;	 
	struct mutex isp_mutex;	 
	bool stop_failure;
	struct media_entity_enum crashed;
	int has_context;
	int ref_count;
	unsigned int autoidle;
#define ISP_CLK_CAM_ICK		0
#define ISP_CLK_CAM_MCLK	1
#define ISP_CLK_CSI2_FCK	2
#define ISP_CLK_L3_ICK		3
	struct clk *clock[4];
	struct isp_xclk xclks[2];

	 
	struct ispstat isp_af;
	struct ispstat isp_aewb;
	struct ispstat isp_hist;
	struct isp_res_device isp_res;
	struct isp_prev_device isp_prev;
	struct isp_ccdc_device isp_ccdc;
	struct isp_csi2_device isp_csi2a;
	struct isp_csi2_device isp_csi2c;
	struct isp_ccp2_device isp_ccp2;
	struct isp_csiphy isp_csiphy1;
	struct isp_csiphy isp_csiphy2;

	unsigned int sbl_resources;
	unsigned int subclk_resources;
};

struct isp_async_subdev {
	struct v4l2_async_connection asd;
	struct isp_bus_cfg bus;
};

static inline struct isp_bus_cfg *
v4l2_subdev_to_bus_cfg(struct v4l2_subdev *sd)
{
	struct v4l2_async_connection *asc;

	asc = v4l2_async_connection_unique(sd);
	if (!asc)
		return NULL;

	return &container_of(asc, struct isp_async_subdev, asd)->bus;
}

#define v4l2_dev_to_isp_device(dev) \
	container_of(dev, struct isp_device, v4l2_dev)

void omap3isp_hist_dma_done(struct isp_device *isp);

void omap3isp_flush(struct isp_device *isp);

int omap3isp_module_sync_idle(struct media_entity *me, wait_queue_head_t *wait,
			      atomic_t *stopping);

int omap3isp_module_sync_is_stopping(wait_queue_head_t *wait,
				     atomic_t *stopping);

int omap3isp_pipeline_set_stream(struct isp_pipeline *pipe,
				 enum isp_pipeline_stream_state state);
void omap3isp_pipeline_cancel_stream(struct isp_pipeline *pipe);
void omap3isp_configure_bridge(struct isp_device *isp,
			       enum ccdc_input_entity input,
			       const struct isp_parallel_cfg *buscfg,
			       unsigned int shift, unsigned int bridge);

struct isp_device *omap3isp_get(struct isp_device *isp);
void omap3isp_put(struct isp_device *isp);

void omap3isp_print_status(struct isp_device *isp);

void omap3isp_sbl_enable(struct isp_device *isp, enum isp_sbl_resource res);
void omap3isp_sbl_disable(struct isp_device *isp, enum isp_sbl_resource res);

void omap3isp_subclk_enable(struct isp_device *isp,
			    enum isp_subclk_resource res);
void omap3isp_subclk_disable(struct isp_device *isp,
			     enum isp_subclk_resource res);

int omap3isp_register_entities(struct platform_device *pdev,
			       struct v4l2_device *v4l2_dev);
void omap3isp_unregister_entities(struct platform_device *pdev);

 
static inline
u32 isp_reg_readl(struct isp_device *isp, enum isp_mem_resources isp_mmio_range,
		  u32 reg_offset)
{
	return __raw_readl(isp->mmio_base[isp_mmio_range] + reg_offset);
}

 
static inline
void isp_reg_writel(struct isp_device *isp, u32 reg_value,
		    enum isp_mem_resources isp_mmio_range, u32 reg_offset)
{
	__raw_writel(reg_value, isp->mmio_base[isp_mmio_range] + reg_offset);
}

 
static inline
void isp_reg_clr(struct isp_device *isp, enum isp_mem_resources mmio_range,
		 u32 reg, u32 clr_bits)
{
	u32 v = isp_reg_readl(isp, mmio_range, reg);

	isp_reg_writel(isp, v & ~clr_bits, mmio_range, reg);
}

 
static inline
void isp_reg_set(struct isp_device *isp, enum isp_mem_resources mmio_range,
		 u32 reg, u32 set_bits)
{
	u32 v = isp_reg_readl(isp, mmio_range, reg);

	isp_reg_writel(isp, v | set_bits, mmio_range, reg);
}

 
static inline
void isp_reg_clr_set(struct isp_device *isp, enum isp_mem_resources mmio_range,
		     u32 reg, u32 clr_bits, u32 set_bits)
{
	u32 v = isp_reg_readl(isp, mmio_range, reg);

	isp_reg_writel(isp, (v & ~clr_bits) | set_bits, mmio_range, reg);
}

static inline enum v4l2_buf_type
isp_pad_buffer_type(const struct v4l2_subdev *subdev, int pad)
{
	if (pad >= subdev->entity.num_pads)
		return 0;

	if (subdev->entity.pads[pad].flags & MEDIA_PAD_FL_SINK)
		return V4L2_BUF_TYPE_VIDEO_OUTPUT;
	else
		return V4L2_BUF_TYPE_VIDEO_CAPTURE;
}

#endif	 
