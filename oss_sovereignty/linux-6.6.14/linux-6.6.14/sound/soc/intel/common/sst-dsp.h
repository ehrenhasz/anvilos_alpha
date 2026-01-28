#ifndef __SOUND_SOC_SST_DSP_H
#define __SOUND_SOC_SST_DSP_H
#include <linux/kernel.h>
#include <linux/types.h>
#include <linux/interrupt.h>
struct sst_dsp;
struct sst_dsp_device {
	struct sst_ops *ops;
	irqreturn_t (*thread)(int irq, void *context);
	void *thread_context;
};
void sst_dsp_shim_write(struct sst_dsp *sst, u32 offset, u32 value);
u32 sst_dsp_shim_read(struct sst_dsp *sst, u32 offset);
int sst_dsp_shim_update_bits(struct sst_dsp *sst, u32 offset,
				u32 mask, u32 value);
void sst_dsp_shim_update_bits_forced(struct sst_dsp *sst, u32 offset,
				u32 mask, u32 value);
void sst_dsp_shim_write_unlocked(struct sst_dsp *sst, u32 offset, u32 value);
u32 sst_dsp_shim_read_unlocked(struct sst_dsp *sst, u32 offset);
int sst_dsp_shim_update_bits_unlocked(struct sst_dsp *sst, u32 offset,
				u32 mask, u32 value);
void sst_dsp_shim_update_bits_forced_unlocked(struct sst_dsp *sst, u32 offset,
				u32 mask, u32 value);
void sst_shim32_write(void __iomem *addr, u32 offset, u32 value);
u32 sst_shim32_read(void __iomem *addr, u32 offset);
void sst_shim32_write64(void __iomem *addr, u32 offset, u64 value);
u64 sst_shim32_read64(void __iomem *addr, u32 offset);
int sst_dsp_mailbox_init(struct sst_dsp *sst, u32 inbox_offset,
	size_t inbox_size, u32 outbox_offset, size_t outbox_size);
void sst_dsp_inbox_write(struct sst_dsp *sst, void *message, size_t bytes);
void sst_dsp_inbox_read(struct sst_dsp *sst, void *message, size_t bytes);
void sst_dsp_outbox_write(struct sst_dsp *sst, void *message, size_t bytes);
void sst_dsp_outbox_read(struct sst_dsp *sst, void *message, size_t bytes);
int sst_dsp_register_poll(struct sst_dsp  *ctx, u32 offset, u32 mask,
		 u32 target, u32 time, char *operation);
#endif
