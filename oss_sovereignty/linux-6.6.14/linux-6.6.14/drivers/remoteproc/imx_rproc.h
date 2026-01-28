#ifndef _IMX_RPROC_H
#define _IMX_RPROC_H
struct imx_rproc_att {
	u32 da;	 
	u32 sa;	 
	u32 size;  
	int flags;
};
enum imx_rproc_method {
	IMX_RPROC_NONE,
	IMX_RPROC_MMIO,
	IMX_RPROC_SMC,
	IMX_RPROC_SCU_API,
};
struct imx_rproc_dcfg {
	u32				src_reg;
	u32				src_mask;
	u32				src_start;
	u32				src_stop;
	u32				gpr_reg;
	u32				gpr_wait;
	const struct imx_rproc_att	*att;
	size_t				att_size;
	enum imx_rproc_method		method;
};
#endif  
