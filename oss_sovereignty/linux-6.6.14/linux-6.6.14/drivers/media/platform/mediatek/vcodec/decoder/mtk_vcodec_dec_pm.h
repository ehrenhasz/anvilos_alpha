#ifndef _MTK_VCODEC_DEC_PM_H_
#define _MTK_VCODEC_DEC_PM_H_
#include "mtk_vcodec_dec_drv.h"
int mtk_vcodec_init_dec_clk(struct platform_device *pdev, struct mtk_vcodec_pm *pm);
void mtk_vcodec_dec_enable_hardware(struct mtk_vcodec_dec_ctx *ctx, int hw_idx);
void mtk_vcodec_dec_disable_hardware(struct mtk_vcodec_dec_ctx *ctx, int hw_idx);
#endif  
