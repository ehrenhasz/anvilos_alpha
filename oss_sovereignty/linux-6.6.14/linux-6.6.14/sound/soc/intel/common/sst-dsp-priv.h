#ifndef __SOUND_SOC_SST_DSP_PRIV_H
#define __SOUND_SOC_SST_DSP_PRIV_H
#include <linux/kernel.h>
#include <linux/types.h>
#include <linux/interrupt.h>
#include <linux/firmware.h>
#include "../skylake/skl-sst-dsp.h"
struct sst_ops {
	void (*write)(void __iomem *addr, u32 offset, u32 value);
	u32 (*read)(void __iomem *addr, u32 offset);
	irqreturn_t (*irq_handler)(int irq, void *context);
	int (*init)(struct sst_dsp *sst);
	void (*free)(struct sst_dsp *sst);
};
struct sst_addr {
	u32 sram0_base;
	u32 sram1_base;
	u32 w0_stat_sz;
	u32 w0_up_sz;
	void __iomem *lpe;
	void __iomem *shim;
};
struct sst_mailbox {
	void __iomem *in_base;
	void __iomem *out_base;
	size_t in_size;
	size_t out_size;
};
struct sst_dsp {
	struct sst_dsp_device *sst_dev;
	spinlock_t spinlock;	 
	struct mutex mutex;	 
	struct device *dev;
	void *thread_context;
	int irq;
	u32 id;
	struct sst_ops *ops;
	struct dentry *debugfs_root;
	struct sst_addr addr;
	struct sst_mailbox mailbox;
	struct list_head module_list;
	const char *fw_name;
	struct skl_dsp_loader_ops dsp_ops;
	struct skl_dsp_fw_ops fw_ops;
	int sst_state;
	struct skl_cl_dev cl_dev;
	u32 intr_status;
	const struct firmware *fw;
	struct snd_dma_buffer dmab;
};
#endif
