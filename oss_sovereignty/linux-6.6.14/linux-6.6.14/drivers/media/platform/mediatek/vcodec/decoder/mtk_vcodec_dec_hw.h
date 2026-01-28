#ifndef _MTK_VCODEC_DEC_HW_H_
#define _MTK_VCODEC_DEC_HW_H_
#include <linux/io.h>
#include <linux/platform_device.h>
#include "mtk_vcodec_dec_drv.h"
#define VDEC_HW_ACTIVE_ADDR 0x0
#define VDEC_HW_ACTIVE_MASK BIT(4)
#define VDEC_IRQ_CFG 0x11
#define VDEC_IRQ_CLR 0x10
#define VDEC_IRQ_CFG_REG 0xa4
#define IS_SUPPORT_VDEC_HW_IRQ(hw_idx) ((hw_idx) != MTK_VDEC_LAT_SOC)
enum mtk_vdec_hw_reg_idx {
	VDEC_HW_SYS,
	VDEC_HW_MISC,
	VDEC_HW_MAX
};
struct mtk_vdec_hw_dev {
	struct platform_device *plat_dev;
	struct mtk_vcodec_dec_dev *main_dev;
	void __iomem *reg_base[VDEC_HW_MAX];
	struct mtk_vcodec_dec_ctx *curr_ctx;
	int dec_irq;
	struct mtk_vcodec_pm pm;
	int hw_idx;
};
#endif  
