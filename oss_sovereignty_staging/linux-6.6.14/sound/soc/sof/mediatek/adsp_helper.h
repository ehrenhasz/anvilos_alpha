 

 

#ifndef __MTK_ADSP_HELPER_H__
#define __MTK_ADSP_HELPER_H__

#include <linux/firmware/mediatek/mtk-adsp-ipc.h>

 
struct mtk_adsp_chip_info {
	phys_addr_t pa_sram;
	phys_addr_t pa_dram;  
	phys_addr_t pa_shared_dram;  
	phys_addr_t pa_cfgreg;
	u32 sramsize;
	u32 dramsize;
	u32 cfgregsize;
	u32 shared_size;
	void __iomem *va_sram;  
	void __iomem *va_dram;  
	void __iomem *va_cfgreg;
	void __iomem *shared_sram;  
	void __iomem *shared_dram;  
	phys_addr_t adsp_bootup_addr;
	int dram_offset;  

	phys_addr_t pa_secreg;
	u32 secregsize;
	void __iomem *va_secreg;

	phys_addr_t pa_busreg;
	u32 busregsize;
	void __iomem *va_busreg;
};

struct adsp_priv {
	struct device *dev;
	struct snd_sof_dev *sdev;
	struct mtk_adsp_ipc *dsp_ipc;
	struct platform_device *ipc_dev;
	struct mtk_adsp_chip_info *adsp;
	struct clk **clk;
	u32 (*ap2adsp_addr)(u32 addr, void *data);
	u32 (*adsp2ap_addr)(u32 addr, void *data);

	void *private_data;
};

#endif
