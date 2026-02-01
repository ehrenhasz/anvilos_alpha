 
 

#ifndef __OMAP_MCBSP_H__
#define __OMAP_MCBSP_H__

#include <sound/dmaengine_pcm.h>

 
enum omap_mcbsp_clksrg_clk {
	OMAP_MCBSP_SYSCLK_CLKS_FCLK,	 
	OMAP_MCBSP_SYSCLK_CLKS_EXT,	 
	OMAP_MCBSP_SYSCLK_CLK,		 
	OMAP_MCBSP_SYSCLK_CLKX_EXT,	 
	OMAP_MCBSP_SYSCLK_CLKR_EXT,	 
};

 
enum omap_mcbsp_div {
	OMAP_MCBSP_CLKGDV,		 
};

int omap_mcbsp_st_add_controls(struct snd_soc_pcm_runtime *rtd, int port_id);

#endif  
