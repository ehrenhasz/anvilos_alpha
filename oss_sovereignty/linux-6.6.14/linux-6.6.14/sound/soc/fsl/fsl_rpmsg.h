#ifndef __FSL_RPMSG_H
#define __FSL_RPMSG_H
struct fsl_rpmsg_soc_data {
	int rates;
	u64 formats;
};
struct fsl_rpmsg {
	struct clk *ipg;
	struct clk *mclk;
	struct clk *dma;
	struct clk *pll8k;
	struct clk *pll11k;
	struct platform_device *card_pdev;
	const struct fsl_rpmsg_soc_data *soc_data;
	unsigned int mclk_streams;
	int force_lpa;
	int enable_lpa;
	int buffer_size;
};
#endif  
