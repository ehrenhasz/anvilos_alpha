#ifndef __ADRENO_GPU_H__
#define __ADRENO_GPU_H__
#include <linux/firmware.h>
#include <linux/iopoll.h>
#include "msm_gpu.h"
#include "adreno_common.xml.h"
#include "adreno_pm4.xml.h"
extern bool snapshot_debugbus;
extern bool allow_vram_carveout;
enum {
	ADRENO_FW_PM4 = 0,
	ADRENO_FW_SQE = 0,  
	ADRENO_FW_PFP = 1,
	ADRENO_FW_GMU = 1,  
	ADRENO_FW_GPMU = 2,
	ADRENO_FW_MAX,
};
enum adreno_family {
	ADRENO_2XX_GEN1,   
	ADRENO_2XX_GEN2,   
	ADRENO_3XX,
	ADRENO_4XX,
	ADRENO_5XX,
	ADRENO_6XX_GEN1,   
	ADRENO_6XX_GEN2,   
	ADRENO_6XX_GEN3,   
	ADRENO_6XX_GEN4,   
};
#define ADRENO_QUIRK_TWO_PASS_USE_WFI		BIT(0)
#define ADRENO_QUIRK_FAULT_DETECT_MASK		BIT(1)
#define ADRENO_QUIRK_LMLOADKILL_DISABLE		BIT(2)
#define ADRENO_QUIRK_HAS_HW_APRIV		BIT(3)
#define ADRENO_QUIRK_HAS_CACHED_COHERENT	BIT(4)
#define ADRENO_CHIPID_FMT "u.%u.%u.%u"
#define ADRENO_CHIPID_ARGS(_c) \
	(((_c) >> 24) & 0xff), \
	(((_c) >> 16) & 0xff), \
	(((_c) >> 8)  & 0xff), \
	((_c) & 0xff)
struct adreno_gpu_funcs {
	struct msm_gpu_funcs base;
	int (*get_timestamp)(struct msm_gpu *gpu, uint64_t *value);
};
struct adreno_reglist {
	u32 offset;
	u32 value;
};
extern const struct adreno_reglist a612_hwcg[], a615_hwcg[], a630_hwcg[], a640_hwcg[], a650_hwcg[];
extern const struct adreno_reglist a660_hwcg[], a690_hwcg[];
struct adreno_speedbin {
	uint16_t fuse;
	uint16_t speedbin;
};
struct adreno_info {
	const char *machine;
	uint32_t *chip_ids;
	enum adreno_family family;
	uint32_t revn;
	const char *fw[ADRENO_FW_MAX];
	uint32_t gmem;
	u64 quirks;
	struct msm_gpu *(*init)(struct drm_device *dev);
	const char *zapfw;
	u32 inactive_period;
	const struct adreno_reglist *hwcg;
	u64 address_space_size;
	struct adreno_speedbin *speedbins;
};
#define ADRENO_CHIP_IDS(tbl...) (uint32_t[]) { tbl, 0 }
#define ADRENO_SPEEDBINS(tbl...) (struct adreno_speedbin[]) { tbl {SHRT_MAX, 0} }
struct adreno_gpu {
	struct msm_gpu base;
	const struct adreno_info *info;
	uint32_t chip_id;
	uint16_t speedbin;
	const struct adreno_gpu_funcs *funcs;
	const unsigned int *registers;
	enum {
		FW_LOCATION_UNKNOWN = 0,
		FW_LOCATION_NEW,        
		FW_LOCATION_LEGACY,     
		FW_LOCATION_HELPER,
	} fwloc;
	const struct firmware *fw[ADRENO_FW_MAX];
	const unsigned int *reg_offsets;
	bool gmu_is_wrapper;
};
#define to_adreno_gpu(x) container_of(x, struct adreno_gpu, base)
struct adreno_ocmem {
	struct ocmem *ocmem;
	unsigned long base;
	void *hdl;
};
struct adreno_platform_config {
	uint32_t chip_id;
	const struct adreno_info *info;
};
#define ADRENO_IDLE_TIMEOUT msecs_to_jiffies(1000)
#define spin_until(X) ({                                   \
	int __ret = -ETIMEDOUT;                            \
	unsigned long __t = jiffies + ADRENO_IDLE_TIMEOUT; \
	do {                                               \
		if (X) {                                   \
			__ret = 0;                         \
			break;                             \
		}                                          \
	} while (time_before(jiffies, __t));               \
	__ret;                                             \
})
static inline uint8_t adreno_patchid(const struct adreno_gpu *gpu)
{
	WARN_ON_ONCE(gpu->info->family >= ADRENO_6XX_GEN1);
	return gpu->chip_id & 0xff;
}
static inline bool adreno_is_revn(const struct adreno_gpu *gpu, uint32_t revn)
{
	if (WARN_ON_ONCE(!gpu->info))
		return false;
	return gpu->info->revn == revn;
}
static inline bool adreno_has_gmu_wrapper(const struct adreno_gpu *gpu)
{
	return gpu->gmu_is_wrapper;
}
static inline bool adreno_is_a2xx(const struct adreno_gpu *gpu)
{
	if (WARN_ON_ONCE(!gpu->info))
		return false;
	return gpu->info->family <= ADRENO_2XX_GEN2;
}
static inline bool adreno_is_a20x(const struct adreno_gpu *gpu)
{
	if (WARN_ON_ONCE(!gpu->info))
		return false;
	return gpu->info->family == ADRENO_2XX_GEN1;
}
static inline bool adreno_is_a225(const struct adreno_gpu *gpu)
{
	return adreno_is_revn(gpu, 225);
}
static inline bool adreno_is_a305(const struct adreno_gpu *gpu)
{
	return adreno_is_revn(gpu, 305);
}
static inline bool adreno_is_a306(const struct adreno_gpu *gpu)
{
	return adreno_is_revn(gpu, 307);
}
static inline bool adreno_is_a320(const struct adreno_gpu *gpu)
{
	return adreno_is_revn(gpu, 320);
}
static inline bool adreno_is_a330(const struct adreno_gpu *gpu)
{
	return adreno_is_revn(gpu, 330);
}
static inline bool adreno_is_a330v2(const struct adreno_gpu *gpu)
{
	return adreno_is_a330(gpu) && (adreno_patchid(gpu) > 0);
}
static inline int adreno_is_a405(const struct adreno_gpu *gpu)
{
	return adreno_is_revn(gpu, 405);
}
static inline int adreno_is_a420(const struct adreno_gpu *gpu)
{
	return adreno_is_revn(gpu, 420);
}
static inline int adreno_is_a430(const struct adreno_gpu *gpu)
{
	return adreno_is_revn(gpu, 430);
}
static inline int adreno_is_a506(const struct adreno_gpu *gpu)
{
	return adreno_is_revn(gpu, 506);
}
static inline int adreno_is_a508(const struct adreno_gpu *gpu)
{
	return adreno_is_revn(gpu, 508);
}
static inline int adreno_is_a509(const struct adreno_gpu *gpu)
{
	return adreno_is_revn(gpu, 509);
}
static inline int adreno_is_a510(const struct adreno_gpu *gpu)
{
	return adreno_is_revn(gpu, 510);
}
static inline int adreno_is_a512(const struct adreno_gpu *gpu)
{
	return adreno_is_revn(gpu, 512);
}
static inline int adreno_is_a530(const struct adreno_gpu *gpu)
{
	return adreno_is_revn(gpu, 530);
}
static inline int adreno_is_a540(const struct adreno_gpu *gpu)
{
	return adreno_is_revn(gpu, 540);
}
static inline int adreno_is_a610(const struct adreno_gpu *gpu)
{
	return adreno_is_revn(gpu, 610);
}
static inline int adreno_is_a618(const struct adreno_gpu *gpu)
{
	return adreno_is_revn(gpu, 618);
}
static inline int adreno_is_a619(const struct adreno_gpu *gpu)
{
	return adreno_is_revn(gpu, 619);
}
static inline int adreno_is_a619_holi(const struct adreno_gpu *gpu)
{
	return adreno_is_a619(gpu) && adreno_has_gmu_wrapper(gpu);
}
static inline int adreno_is_a630(const struct adreno_gpu *gpu)
{
	return adreno_is_revn(gpu, 630);
}
static inline int adreno_is_a640(const struct adreno_gpu *gpu)
{
	return adreno_is_revn(gpu, 640);
}
static inline int adreno_is_a650(const struct adreno_gpu *gpu)
{
	return adreno_is_revn(gpu, 650);
}
static inline int adreno_is_7c3(const struct adreno_gpu *gpu)
{
	return gpu->info->chip_ids[0] == 0x06030500;
}
static inline int adreno_is_a660(const struct adreno_gpu *gpu)
{
	return adreno_is_revn(gpu, 660);
}
static inline int adreno_is_a680(const struct adreno_gpu *gpu)
{
	return adreno_is_revn(gpu, 680);
}
static inline int adreno_is_a690(const struct adreno_gpu *gpu)
{
	return gpu->info->chip_ids[0] == 0x06090000;
}
static inline int adreno_is_a630_family(const struct adreno_gpu *gpu)
{
	if (WARN_ON_ONCE(!gpu->info))
		return false;
	return gpu->info->family == ADRENO_6XX_GEN1;
}
static inline int adreno_is_a660_family(const struct adreno_gpu *gpu)
{
	if (WARN_ON_ONCE(!gpu->info))
		return false;
	return gpu->info->family == ADRENO_6XX_GEN4;
}
static inline int adreno_is_a650_family(const struct adreno_gpu *gpu)
{
	if (WARN_ON_ONCE(!gpu->info))
		return false;
	return gpu->info->family >= ADRENO_6XX_GEN3;
}
static inline int adreno_is_a640_family(const struct adreno_gpu *gpu)
{
	if (WARN_ON_ONCE(!gpu->info))
		return false;
	return gpu->info->family == ADRENO_6XX_GEN2;
}
u64 adreno_private_address_space_size(struct msm_gpu *gpu);
int adreno_get_param(struct msm_gpu *gpu, struct msm_file_private *ctx,
		     uint32_t param, uint64_t *value, uint32_t *len);
int adreno_set_param(struct msm_gpu *gpu, struct msm_file_private *ctx,
		     uint32_t param, uint64_t value, uint32_t len);
const struct firmware *adreno_request_fw(struct adreno_gpu *adreno_gpu,
		const char *fwname);
struct drm_gem_object *adreno_fw_create_bo(struct msm_gpu *gpu,
		const struct firmware *fw, u64 *iova);
int adreno_hw_init(struct msm_gpu *gpu);
void adreno_recover(struct msm_gpu *gpu);
void adreno_flush(struct msm_gpu *gpu, struct msm_ringbuffer *ring, u32 reg);
bool adreno_idle(struct msm_gpu *gpu, struct msm_ringbuffer *ring);
#if defined(CONFIG_DEBUG_FS) || defined(CONFIG_DEV_COREDUMP)
void adreno_show(struct msm_gpu *gpu, struct msm_gpu_state *state,
		struct drm_printer *p);
#endif
void adreno_dump_info(struct msm_gpu *gpu);
void adreno_dump(struct msm_gpu *gpu);
void adreno_wait_ring(struct msm_ringbuffer *ring, uint32_t ndwords);
struct msm_ringbuffer *adreno_active_ring(struct msm_gpu *gpu);
int adreno_gpu_ocmem_init(struct device *dev, struct adreno_gpu *adreno_gpu,
			  struct adreno_ocmem *ocmem);
void adreno_gpu_ocmem_cleanup(struct adreno_ocmem *ocmem);
int adreno_gpu_init(struct drm_device *drm, struct platform_device *pdev,
		struct adreno_gpu *gpu, const struct adreno_gpu_funcs *funcs,
		int nr_rings);
void adreno_gpu_cleanup(struct adreno_gpu *gpu);
int adreno_load_fw(struct adreno_gpu *adreno_gpu);
void adreno_gpu_state_destroy(struct msm_gpu_state *state);
int adreno_gpu_state_get(struct msm_gpu *gpu, struct msm_gpu_state *state);
int adreno_gpu_state_put(struct msm_gpu_state *state);
void adreno_show_object(struct drm_printer *p, void **ptr, int len,
		bool *encoded);
struct msm_gem_address_space *
adreno_create_address_space(struct msm_gpu *gpu,
			    struct platform_device *pdev);
struct msm_gem_address_space *
adreno_iommu_create_address_space(struct msm_gpu *gpu,
				  struct platform_device *pdev,
				  unsigned long quirks);
int adreno_fault_handler(struct msm_gpu *gpu, unsigned long iova, int flags,
			 struct adreno_smmu_fault_info *info, const char *block,
			 u32 scratch[4]);
int adreno_read_speedbin(struct device *dev, u32 *speedbin);
int adreno_zap_shader_load(struct msm_gpu *gpu, u32 pasid);
static inline void
OUT_PKT0(struct msm_ringbuffer *ring, uint16_t regindx, uint16_t cnt)
{
	adreno_wait_ring(ring, cnt+1);
	OUT_RING(ring, CP_TYPE0_PKT | ((cnt-1) << 16) | (regindx & 0x7FFF));
}
static inline void
OUT_PKT2(struct msm_ringbuffer *ring)
{
	adreno_wait_ring(ring, 1);
	OUT_RING(ring, CP_TYPE2_PKT);
}
static inline void
OUT_PKT3(struct msm_ringbuffer *ring, uint8_t opcode, uint16_t cnt)
{
	adreno_wait_ring(ring, cnt+1);
	OUT_RING(ring, CP_TYPE3_PKT | ((cnt-1) << 16) | ((opcode & 0xFF) << 8));
}
static inline u32 PM4_PARITY(u32 val)
{
	return (0x9669 >> (0xF & (val ^
		(val >> 4) ^ (val >> 8) ^ (val >> 12) ^
		(val >> 16) ^ ((val) >> 20) ^ (val >> 24) ^
		(val >> 28)))) & 1;
}
#define TYPE4_MAX_PAYLOAD 127
#define PKT4(_reg, _cnt) \
	(CP_TYPE4_PKT | ((_cnt) << 0) | (PM4_PARITY((_cnt)) << 7) | \
	 (((_reg) & 0x3FFFF) << 8) | (PM4_PARITY((_reg)) << 27))
static inline void
OUT_PKT4(struct msm_ringbuffer *ring, uint16_t regindx, uint16_t cnt)
{
	adreno_wait_ring(ring, cnt + 1);
	OUT_RING(ring, PKT4(regindx, cnt));
}
static inline void
OUT_PKT7(struct msm_ringbuffer *ring, uint8_t opcode, uint16_t cnt)
{
	adreno_wait_ring(ring, cnt + 1);
	OUT_RING(ring, CP_TYPE7_PKT | (cnt << 0) | (PM4_PARITY(cnt) << 15) |
		((opcode & 0x7F) << 16) | (PM4_PARITY(opcode) << 23));
}
struct msm_gpu *a2xx_gpu_init(struct drm_device *dev);
struct msm_gpu *a3xx_gpu_init(struct drm_device *dev);
struct msm_gpu *a4xx_gpu_init(struct drm_device *dev);
struct msm_gpu *a5xx_gpu_init(struct drm_device *dev);
struct msm_gpu *a6xx_gpu_init(struct drm_device *dev);
static inline uint32_t get_wptr(struct msm_ringbuffer *ring)
{
	return (ring->cur - ring->start) % (MSM_GPU_RINGBUFFER_SZ >> 2);
}
#define ADRENO_PROTECT_RW(_reg, _len) \
	((1 << 30) | (1 << 29) | \
	((ilog2((_len)) & 0x1F) << 24) | (((_reg) << 2) & 0xFFFFF))
#define ADRENO_PROTECT_RDONLY(_reg, _len) \
	((1 << 29) \
	((ilog2((_len)) & 0x1F) << 24) | (((_reg) << 2) & 0xFFFFF))
#define gpu_poll_timeout(gpu, addr, val, cond, interval, timeout) \
	readl_poll_timeout((gpu)->mmio + ((addr) << 2), val, cond, \
		interval, timeout)
#endif  
