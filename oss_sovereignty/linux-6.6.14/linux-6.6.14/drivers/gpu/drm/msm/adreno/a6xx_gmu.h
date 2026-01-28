#ifndef _A6XX_GMU_H_
#define _A6XX_GMU_H_
#include <linux/completion.h>
#include <linux/iopoll.h>
#include <linux/interrupt.h>
#include <linux/notifier.h>
#include "msm_drv.h"
#include "a6xx_hfi.h"
struct a6xx_gmu_bo {
	struct drm_gem_object *obj;
	void *virt;
	size_t size;
	u64 iova;
};
#define GMU_WARM_BOOT 0
#define GMU_COLD_BOOT 1
#define GMU_IDLE_STATE_ACTIVE 0
#define GMU_IDLE_STATE_SPTP 2
#define GMU_IDLE_STATE_IFPC 3
struct a6xx_gmu {
	struct device *dev;
	struct mutex lock;
	struct msm_gem_address_space *aspace;
	void __iomem *mmio;
	void __iomem *rscc;
	int hfi_irq;
	int gmu_irq;
	struct device *gxpd;
	struct device *cxpd;
	int idle_level;
	struct a6xx_gmu_bo hfi;
	struct a6xx_gmu_bo debug;
	struct a6xx_gmu_bo icache;
	struct a6xx_gmu_bo dcache;
	struct a6xx_gmu_bo dummy;
	struct a6xx_gmu_bo log;
	int nr_clocks;
	struct clk_bulk_data *clocks;
	struct clk *core_clk;
	struct clk *hub_clk;
	int current_perf_index;
	int nr_gpu_freqs;
	unsigned long gpu_freqs[16];
	u32 gx_arc_votes[16];
	int nr_gmu_freqs;
	unsigned long gmu_freqs[4];
	u32 cx_arc_votes[4];
	unsigned long freq;
	struct a6xx_hfi_queue queues[2];
	bool initialized;
	bool hung;
	bool legacy;  
	struct notifier_block pd_nb;
	struct completion pd_gate;
};
static inline u32 gmu_read(struct a6xx_gmu *gmu, u32 offset)
{
	return msm_readl(gmu->mmio + (offset << 2));
}
static inline void gmu_write(struct a6xx_gmu *gmu, u32 offset, u32 value)
{
	msm_writel(value, gmu->mmio + (offset << 2));
}
static inline void
gmu_write_bulk(struct a6xx_gmu *gmu, u32 offset, const u32 *data, u32 size)
{
	memcpy_toio(gmu->mmio + (offset << 2), data, size);
	wmb();
}
static inline void gmu_rmw(struct a6xx_gmu *gmu, u32 reg, u32 mask, u32 or)
{
	u32 val = gmu_read(gmu, reg);
	val &= ~mask;
	gmu_write(gmu, reg, val | or);
}
static inline u64 gmu_read64(struct a6xx_gmu *gmu, u32 lo, u32 hi)
{
	u64 val;
	val = (u64) msm_readl(gmu->mmio + (lo << 2));
	val |= ((u64) msm_readl(gmu->mmio + (hi << 2)) << 32);
	return val;
}
#define gmu_poll_timeout(gmu, addr, val, cond, interval, timeout) \
	readl_poll_timeout((gmu)->mmio + ((addr) << 2), val, cond, \
		interval, timeout)
static inline u32 gmu_read_rscc(struct a6xx_gmu *gmu, u32 offset)
{
	return msm_readl(gmu->rscc + (offset << 2));
}
static inline void gmu_write_rscc(struct a6xx_gmu *gmu, u32 offset, u32 value)
{
	msm_writel(value, gmu->rscc + (offset << 2));
}
#define gmu_poll_timeout_rscc(gmu, addr, val, cond, interval, timeout) \
	readl_poll_timeout((gmu)->rscc + ((addr) << 2), val, cond, \
		interval, timeout)
enum a6xx_gmu_oob_state {
	GMU_OOB_BOOT_SLUMBER = 0,
	GMU_OOB_GPU_SET,
	GMU_OOB_DCVS_SET,
	GMU_OOB_PERFCOUNTER_SET,
};
void a6xx_hfi_init(struct a6xx_gmu *gmu);
int a6xx_hfi_start(struct a6xx_gmu *gmu, int boot_state);
void a6xx_hfi_stop(struct a6xx_gmu *gmu);
int a6xx_hfi_send_prep_slumber(struct a6xx_gmu *gmu);
int a6xx_hfi_set_freq(struct a6xx_gmu *gmu, int index);
bool a6xx_gmu_gx_is_on(struct a6xx_gmu *gmu);
bool a6xx_gmu_sptprac_is_on(struct a6xx_gmu *gmu);
void a6xx_sptprac_disable(struct a6xx_gmu *gmu);
int a6xx_sptprac_enable(struct a6xx_gmu *gmu);
#endif
