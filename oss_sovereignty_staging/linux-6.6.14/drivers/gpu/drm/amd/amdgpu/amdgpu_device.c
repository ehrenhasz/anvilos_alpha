 
#include <linux/power_supply.h>
#include <linux/kthread.h>
#include <linux/module.h>
#include <linux/console.h>
#include <linux/slab.h>
#include <linux/iommu.h>
#include <linux/pci.h>
#include <linux/devcoredump.h>
#include <generated/utsrelease.h>
#include <linux/pci-p2pdma.h>
#include <linux/apple-gmux.h>

#include <drm/drm_aperture.h>
#include <drm/drm_atomic_helper.h>
#include <drm/drm_crtc_helper.h>
#include <drm/drm_fb_helper.h>
#include <drm/drm_probe_helper.h>
#include <drm/amdgpu_drm.h>
#include <linux/device.h>
#include <linux/vgaarb.h>
#include <linux/vga_switcheroo.h>
#include <linux/efi.h>
#include "amdgpu.h"
#include "amdgpu_trace.h"
#include "amdgpu_i2c.h"
#include "atom.h"
#include "amdgpu_atombios.h"
#include "amdgpu_atomfirmware.h"
#include "amd_pcie.h"
#ifdef CONFIG_DRM_AMDGPU_SI
#include "si.h"
#endif
#ifdef CONFIG_DRM_AMDGPU_CIK
#include "cik.h"
#endif
#include "vi.h"
#include "soc15.h"
#include "nv.h"
#include "bif/bif_4_1_d.h"
#include <linux/firmware.h>
#include "amdgpu_vf_error.h"

#include "amdgpu_amdkfd.h"
#include "amdgpu_pm.h"

#include "amdgpu_xgmi.h"
#include "amdgpu_ras.h"
#include "amdgpu_pmu.h"
#include "amdgpu_fru_eeprom.h"
#include "amdgpu_reset.h"

#include <linux/suspend.h>
#include <drm/task_barrier.h>
#include <linux/pm_runtime.h>

#include <drm/drm_drv.h>

#if IS_ENABLED(CONFIG_X86)
#include <asm/intel-family.h>
#endif

MODULE_FIRMWARE("amdgpu/vega10_gpu_info.bin");
MODULE_FIRMWARE("amdgpu/vega12_gpu_info.bin");
MODULE_FIRMWARE("amdgpu/raven_gpu_info.bin");
MODULE_FIRMWARE("amdgpu/picasso_gpu_info.bin");
MODULE_FIRMWARE("amdgpu/raven2_gpu_info.bin");
MODULE_FIRMWARE("amdgpu/arcturus_gpu_info.bin");
MODULE_FIRMWARE("amdgpu/navi12_gpu_info.bin");

#define AMDGPU_RESUME_MS		2000
#define AMDGPU_MAX_RETRY_LIMIT		2
#define AMDGPU_RETRY_SRIOV_RESET(r) ((r) == -EBUSY || (r) == -ETIMEDOUT || (r) == -EINVAL)

static const struct drm_driver amdgpu_kms_driver;

const char *amdgpu_asic_name[] = {
	"TAHITI",
	"PITCAIRN",
	"VERDE",
	"OLAND",
	"HAINAN",
	"BONAIRE",
	"KAVERI",
	"KABINI",
	"HAWAII",
	"MULLINS",
	"TOPAZ",
	"TONGA",
	"FIJI",
	"CARRIZO",
	"STONEY",
	"POLARIS10",
	"POLARIS11",
	"POLARIS12",
	"VEGAM",
	"VEGA10",
	"VEGA12",
	"VEGA20",
	"RAVEN",
	"ARCTURUS",
	"RENOIR",
	"ALDEBARAN",
	"NAVI10",
	"CYAN_SKILLFISH",
	"NAVI14",
	"NAVI12",
	"SIENNA_CICHLID",
	"NAVY_FLOUNDER",
	"VANGOGH",
	"DIMGREY_CAVEFISH",
	"BEIGE_GOBY",
	"YELLOW_CARP",
	"IP DISCOVERY",
	"LAST",
};

 

static ssize_t amdgpu_device_get_pcie_replay_count(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct drm_device *ddev = dev_get_drvdata(dev);
	struct amdgpu_device *adev = drm_to_adev(ddev);
	uint64_t cnt = amdgpu_asic_get_pcie_replay_count(adev);

	return sysfs_emit(buf, "%llu\n", cnt);
}

static DEVICE_ATTR(pcie_replay_count, 0444,
		amdgpu_device_get_pcie_replay_count, NULL);

static void amdgpu_device_get_pcie_info(struct amdgpu_device *adev);


 
bool amdgpu_device_supports_px(struct drm_device *dev)
{
	struct amdgpu_device *adev = drm_to_adev(dev);

	if ((adev->flags & AMD_IS_PX) && !amdgpu_is_atpx_hybrid())
		return true;
	return false;
}

 
bool amdgpu_device_supports_boco(struct drm_device *dev)
{
	struct amdgpu_device *adev = drm_to_adev(dev);

	if (adev->has_pr3 ||
	    ((adev->flags & AMD_IS_PX) && amdgpu_is_atpx_hybrid()))
		return true;
	return false;
}

 
bool amdgpu_device_supports_baco(struct drm_device *dev)
{
	struct amdgpu_device *adev = drm_to_adev(dev);

	return amdgpu_asic_supports_baco(adev);
}

 
bool amdgpu_device_supports_smart_shift(struct drm_device *dev)
{
	return (amdgpu_device_supports_boco(dev) &&
		amdgpu_acpi_is_power_shift_control_supported());
}

 

 
void amdgpu_device_mm_access(struct amdgpu_device *adev, loff_t pos,
			     void *buf, size_t size, bool write)
{
	unsigned long flags;
	uint32_t hi = ~0, tmp = 0;
	uint32_t *data = buf;
	uint64_t last;
	int idx;

	if (!drm_dev_enter(adev_to_drm(adev), &idx))
		return;

	BUG_ON(!IS_ALIGNED(pos, 4) || !IS_ALIGNED(size, 4));

	spin_lock_irqsave(&adev->mmio_idx_lock, flags);
	for (last = pos + size; pos < last; pos += 4) {
		tmp = pos >> 31;

		WREG32_NO_KIQ(mmMM_INDEX, ((uint32_t)pos) | 0x80000000);
		if (tmp != hi) {
			WREG32_NO_KIQ(mmMM_INDEX_HI, tmp);
			hi = tmp;
		}
		if (write)
			WREG32_NO_KIQ(mmMM_DATA, *data++);
		else
			*data++ = RREG32_NO_KIQ(mmMM_DATA);
	}

	spin_unlock_irqrestore(&adev->mmio_idx_lock, flags);
	drm_dev_exit(idx);
}

 
size_t amdgpu_device_aper_access(struct amdgpu_device *adev, loff_t pos,
				 void *buf, size_t size, bool write)
{
#ifdef CONFIG_64BIT
	void __iomem *addr;
	size_t count = 0;
	uint64_t last;

	if (!adev->mman.aper_base_kaddr)
		return 0;

	last = min(pos + size, adev->gmc.visible_vram_size);
	if (last > pos) {
		addr = adev->mman.aper_base_kaddr + pos;
		count = last - pos;

		if (write) {
			memcpy_toio(addr, buf, count);
			 
			mb();
			amdgpu_device_flush_hdp(adev, NULL);
		} else {
			amdgpu_device_invalidate_hdp(adev, NULL);
			 
			mb();
			memcpy_fromio(buf, addr, count);
		}

	}

	return count;
#else
	return 0;
#endif
}

 
void amdgpu_device_vram_access(struct amdgpu_device *adev, loff_t pos,
			       void *buf, size_t size, bool write)
{
	size_t count;

	 
	count = amdgpu_device_aper_access(adev, pos, buf, size, write);
	size -= count;
	if (size) {
		 
		pos += count;
		buf += count;
		amdgpu_device_mm_access(adev, pos, buf, size, write);
	}
}

 

 
bool amdgpu_device_skip_hw_access(struct amdgpu_device *adev)
{
	if (adev->no_hw_access)
		return true;

#ifdef CONFIG_LOCKDEP
	 
	if (in_task()) {
		if (down_read_trylock(&adev->reset_domain->sem))
			up_read(&adev->reset_domain->sem);
		else
			lockdep_assert_held(&adev->reset_domain->sem);
	}
#endif
	return false;
}

 
uint32_t amdgpu_device_rreg(struct amdgpu_device *adev,
			    uint32_t reg, uint32_t acc_flags)
{
	uint32_t ret;

	if (amdgpu_device_skip_hw_access(adev))
		return 0;

	if ((reg * 4) < adev->rmmio_size) {
		if (!(acc_flags & AMDGPU_REGS_NO_KIQ) &&
		    amdgpu_sriov_runtime(adev) &&
		    down_read_trylock(&adev->reset_domain->sem)) {
			ret = amdgpu_kiq_rreg(adev, reg);
			up_read(&adev->reset_domain->sem);
		} else {
			ret = readl(((void __iomem *)adev->rmmio) + (reg * 4));
		}
	} else {
		ret = adev->pcie_rreg(adev, reg * 4);
	}

	trace_amdgpu_device_rreg(adev->pdev->device, reg, ret);

	return ret;
}

 

 
uint8_t amdgpu_mm_rreg8(struct amdgpu_device *adev, uint32_t offset)
{
	if (amdgpu_device_skip_hw_access(adev))
		return 0;

	if (offset < adev->rmmio_size)
		return (readb(adev->rmmio + offset));
	BUG();
}

 

 
void amdgpu_mm_wreg8(struct amdgpu_device *adev, uint32_t offset, uint8_t value)
{
	if (amdgpu_device_skip_hw_access(adev))
		return;

	if (offset < adev->rmmio_size)
		writeb(value, adev->rmmio + offset);
	else
		BUG();
}

 
void amdgpu_device_wreg(struct amdgpu_device *adev,
			uint32_t reg, uint32_t v,
			uint32_t acc_flags)
{
	if (amdgpu_device_skip_hw_access(adev))
		return;

	if ((reg * 4) < adev->rmmio_size) {
		if (!(acc_flags & AMDGPU_REGS_NO_KIQ) &&
		    amdgpu_sriov_runtime(adev) &&
		    down_read_trylock(&adev->reset_domain->sem)) {
			amdgpu_kiq_wreg(adev, reg, v);
			up_read(&adev->reset_domain->sem);
		} else {
			writel(v, ((void __iomem *)adev->rmmio) + (reg * 4));
		}
	} else {
		adev->pcie_wreg(adev, reg * 4, v);
	}

	trace_amdgpu_device_wreg(adev->pdev->device, reg, v);
}

 
void amdgpu_mm_wreg_mmio_rlc(struct amdgpu_device *adev,
			     uint32_t reg, uint32_t v,
			     uint32_t xcc_id)
{
	if (amdgpu_device_skip_hw_access(adev))
		return;

	if (amdgpu_sriov_fullaccess(adev) &&
	    adev->gfx.rlc.funcs &&
	    adev->gfx.rlc.funcs->is_rlcg_access_range) {
		if (adev->gfx.rlc.funcs->is_rlcg_access_range(adev, reg))
			return amdgpu_sriov_wreg(adev, reg, v, 0, 0, xcc_id);
	} else if ((reg * 4) >= adev->rmmio_size) {
		adev->pcie_wreg(adev, reg * 4, v);
	} else {
		writel(v, ((void __iomem *)adev->rmmio) + (reg * 4));
	}
}

 
u32 amdgpu_device_indirect_rreg(struct amdgpu_device *adev,
				u32 reg_addr)
{
	unsigned long flags, pcie_index, pcie_data;
	void __iomem *pcie_index_offset;
	void __iomem *pcie_data_offset;
	u32 r;

	pcie_index = adev->nbio.funcs->get_pcie_index_offset(adev);
	pcie_data = adev->nbio.funcs->get_pcie_data_offset(adev);

	spin_lock_irqsave(&adev->pcie_idx_lock, flags);
	pcie_index_offset = (void __iomem *)adev->rmmio + pcie_index * 4;
	pcie_data_offset = (void __iomem *)adev->rmmio + pcie_data * 4;

	writel(reg_addr, pcie_index_offset);
	readl(pcie_index_offset);
	r = readl(pcie_data_offset);
	spin_unlock_irqrestore(&adev->pcie_idx_lock, flags);

	return r;
}

u32 amdgpu_device_indirect_rreg_ext(struct amdgpu_device *adev,
				    u64 reg_addr)
{
	unsigned long flags, pcie_index, pcie_index_hi, pcie_data;
	u32 r;
	void __iomem *pcie_index_offset;
	void __iomem *pcie_index_hi_offset;
	void __iomem *pcie_data_offset;

	pcie_index = adev->nbio.funcs->get_pcie_index_offset(adev);
	pcie_data = adev->nbio.funcs->get_pcie_data_offset(adev);
	if (adev->nbio.funcs->get_pcie_index_hi_offset)
		pcie_index_hi = adev->nbio.funcs->get_pcie_index_hi_offset(adev);
	else
		pcie_index_hi = 0;

	spin_lock_irqsave(&adev->pcie_idx_lock, flags);
	pcie_index_offset = (void __iomem *)adev->rmmio + pcie_index * 4;
	pcie_data_offset = (void __iomem *)adev->rmmio + pcie_data * 4;
	if (pcie_index_hi != 0)
		pcie_index_hi_offset = (void __iomem *)adev->rmmio +
				pcie_index_hi * 4;

	writel(reg_addr, pcie_index_offset);
	readl(pcie_index_offset);
	if (pcie_index_hi != 0) {
		writel((reg_addr >> 32) & 0xff, pcie_index_hi_offset);
		readl(pcie_index_hi_offset);
	}
	r = readl(pcie_data_offset);

	 
	if (pcie_index_hi != 0) {
		writel(0, pcie_index_hi_offset);
		readl(pcie_index_hi_offset);
	}

	spin_unlock_irqrestore(&adev->pcie_idx_lock, flags);

	return r;
}

 
u64 amdgpu_device_indirect_rreg64(struct amdgpu_device *adev,
				  u32 reg_addr)
{
	unsigned long flags, pcie_index, pcie_data;
	void __iomem *pcie_index_offset;
	void __iomem *pcie_data_offset;
	u64 r;

	pcie_index = adev->nbio.funcs->get_pcie_index_offset(adev);
	pcie_data = adev->nbio.funcs->get_pcie_data_offset(adev);

	spin_lock_irqsave(&adev->pcie_idx_lock, flags);
	pcie_index_offset = (void __iomem *)adev->rmmio + pcie_index * 4;
	pcie_data_offset = (void __iomem *)adev->rmmio + pcie_data * 4;

	 
	writel(reg_addr, pcie_index_offset);
	readl(pcie_index_offset);
	r = readl(pcie_data_offset);
	 
	writel(reg_addr + 4, pcie_index_offset);
	readl(pcie_index_offset);
	r |= ((u64)readl(pcie_data_offset) << 32);
	spin_unlock_irqrestore(&adev->pcie_idx_lock, flags);

	return r;
}

 
void amdgpu_device_indirect_wreg(struct amdgpu_device *adev,
				 u32 reg_addr, u32 reg_data)
{
	unsigned long flags, pcie_index, pcie_data;
	void __iomem *pcie_index_offset;
	void __iomem *pcie_data_offset;

	pcie_index = adev->nbio.funcs->get_pcie_index_offset(adev);
	pcie_data = adev->nbio.funcs->get_pcie_data_offset(adev);

	spin_lock_irqsave(&adev->pcie_idx_lock, flags);
	pcie_index_offset = (void __iomem *)adev->rmmio + pcie_index * 4;
	pcie_data_offset = (void __iomem *)adev->rmmio + pcie_data * 4;

	writel(reg_addr, pcie_index_offset);
	readl(pcie_index_offset);
	writel(reg_data, pcie_data_offset);
	readl(pcie_data_offset);
	spin_unlock_irqrestore(&adev->pcie_idx_lock, flags);
}

void amdgpu_device_indirect_wreg_ext(struct amdgpu_device *adev,
				     u64 reg_addr, u32 reg_data)
{
	unsigned long flags, pcie_index, pcie_index_hi, pcie_data;
	void __iomem *pcie_index_offset;
	void __iomem *pcie_index_hi_offset;
	void __iomem *pcie_data_offset;

	pcie_index = adev->nbio.funcs->get_pcie_index_offset(adev);
	pcie_data = adev->nbio.funcs->get_pcie_data_offset(adev);
	if (adev->nbio.funcs->get_pcie_index_hi_offset)
		pcie_index_hi = adev->nbio.funcs->get_pcie_index_hi_offset(adev);
	else
		pcie_index_hi = 0;

	spin_lock_irqsave(&adev->pcie_idx_lock, flags);
	pcie_index_offset = (void __iomem *)adev->rmmio + pcie_index * 4;
	pcie_data_offset = (void __iomem *)adev->rmmio + pcie_data * 4;
	if (pcie_index_hi != 0)
		pcie_index_hi_offset = (void __iomem *)adev->rmmio +
				pcie_index_hi * 4;

	writel(reg_addr, pcie_index_offset);
	readl(pcie_index_offset);
	if (pcie_index_hi != 0) {
		writel((reg_addr >> 32) & 0xff, pcie_index_hi_offset);
		readl(pcie_index_hi_offset);
	}
	writel(reg_data, pcie_data_offset);
	readl(pcie_data_offset);

	 
	if (pcie_index_hi != 0) {
		writel(0, pcie_index_hi_offset);
		readl(pcie_index_hi_offset);
	}

	spin_unlock_irqrestore(&adev->pcie_idx_lock, flags);
}

 
void amdgpu_device_indirect_wreg64(struct amdgpu_device *adev,
				   u32 reg_addr, u64 reg_data)
{
	unsigned long flags, pcie_index, pcie_data;
	void __iomem *pcie_index_offset;
	void __iomem *pcie_data_offset;

	pcie_index = adev->nbio.funcs->get_pcie_index_offset(adev);
	pcie_data = adev->nbio.funcs->get_pcie_data_offset(adev);

	spin_lock_irqsave(&adev->pcie_idx_lock, flags);
	pcie_index_offset = (void __iomem *)adev->rmmio + pcie_index * 4;
	pcie_data_offset = (void __iomem *)adev->rmmio + pcie_data * 4;

	 
	writel(reg_addr, pcie_index_offset);
	readl(pcie_index_offset);
	writel((u32)(reg_data & 0xffffffffULL), pcie_data_offset);
	readl(pcie_data_offset);
	 
	writel(reg_addr + 4, pcie_index_offset);
	readl(pcie_index_offset);
	writel((u32)(reg_data >> 32), pcie_data_offset);
	readl(pcie_data_offset);
	spin_unlock_irqrestore(&adev->pcie_idx_lock, flags);
}

 
u32 amdgpu_device_get_rev_id(struct amdgpu_device *adev)
{
	return adev->nbio.funcs->get_rev_id(adev);
}

 
static uint32_t amdgpu_invalid_rreg(struct amdgpu_device *adev, uint32_t reg)
{
	DRM_ERROR("Invalid callback to read register 0x%04X\n", reg);
	BUG();
	return 0;
}

static uint32_t amdgpu_invalid_rreg_ext(struct amdgpu_device *adev, uint64_t reg)
{
	DRM_ERROR("Invalid callback to read register 0x%llX\n", reg);
	BUG();
	return 0;
}

 
static void amdgpu_invalid_wreg(struct amdgpu_device *adev, uint32_t reg, uint32_t v)
{
	DRM_ERROR("Invalid callback to write register 0x%04X with 0x%08X\n",
		  reg, v);
	BUG();
}

static void amdgpu_invalid_wreg_ext(struct amdgpu_device *adev, uint64_t reg, uint32_t v)
{
	DRM_ERROR("Invalid callback to write register 0x%llX with 0x%08X\n",
		  reg, v);
	BUG();
}

 
static uint64_t amdgpu_invalid_rreg64(struct amdgpu_device *adev, uint32_t reg)
{
	DRM_ERROR("Invalid callback to read 64 bit register 0x%04X\n", reg);
	BUG();
	return 0;
}

 
static void amdgpu_invalid_wreg64(struct amdgpu_device *adev, uint32_t reg, uint64_t v)
{
	DRM_ERROR("Invalid callback to write 64 bit register 0x%04X with 0x%08llX\n",
		  reg, v);
	BUG();
}

 
static uint32_t amdgpu_block_invalid_rreg(struct amdgpu_device *adev,
					  uint32_t block, uint32_t reg)
{
	DRM_ERROR("Invalid callback to read register 0x%04X in block 0x%04X\n",
		  reg, block);
	BUG();
	return 0;
}

 
static void amdgpu_block_invalid_wreg(struct amdgpu_device *adev,
				      uint32_t block,
				      uint32_t reg, uint32_t v)
{
	DRM_ERROR("Invalid block callback to write register 0x%04X in block 0x%04X with 0x%08X\n",
		  reg, block, v);
	BUG();
}

 
static int amdgpu_device_asic_init(struct amdgpu_device *adev)
{
	int ret;

	amdgpu_asic_pre_asic_init(adev);

	if (adev->ip_versions[GC_HWIP][0] == IP_VERSION(9, 4, 3) ||
	    adev->ip_versions[GC_HWIP][0] >= IP_VERSION(11, 0, 0)) {
		amdgpu_psp_wait_for_bootloader(adev);
		ret = amdgpu_atomfirmware_asic_init(adev, true);
		return ret;
	} else {
		return amdgpu_atom_asic_init(adev->mode_info.atom_context);
	}

	return 0;
}

 
static int amdgpu_device_mem_scratch_init(struct amdgpu_device *adev)
{
	return amdgpu_bo_create_kernel(adev, AMDGPU_GPU_PAGE_SIZE, PAGE_SIZE,
				       AMDGPU_GEM_DOMAIN_VRAM |
				       AMDGPU_GEM_DOMAIN_GTT,
				       &adev->mem_scratch.robj,
				       &adev->mem_scratch.gpu_addr,
				       (void **)&adev->mem_scratch.ptr);
}

 
static void amdgpu_device_mem_scratch_fini(struct amdgpu_device *adev)
{
	amdgpu_bo_free_kernel(&adev->mem_scratch.robj, NULL, NULL);
}

 
void amdgpu_device_program_register_sequence(struct amdgpu_device *adev,
					     const u32 *registers,
					     const u32 array_size)
{
	u32 tmp, reg, and_mask, or_mask;
	int i;

	if (array_size % 3)
		return;

	for (i = 0; i < array_size; i += 3) {
		reg = registers[i + 0];
		and_mask = registers[i + 1];
		or_mask = registers[i + 2];

		if (and_mask == 0xffffffff) {
			tmp = or_mask;
		} else {
			tmp = RREG32(reg);
			tmp &= ~and_mask;
			if (adev->family >= AMDGPU_FAMILY_AI)
				tmp |= (or_mask & and_mask);
			else
				tmp |= or_mask;
		}
		WREG32(reg, tmp);
	}
}

 
void amdgpu_device_pci_config_reset(struct amdgpu_device *adev)
{
	pci_write_config_dword(adev->pdev, 0x7c, AMDGPU_ASIC_RESET_DATA);
}

 
int amdgpu_device_pci_reset(struct amdgpu_device *adev)
{
	return pci_reset_function(adev->pdev);
}

 

 
static void amdgpu_device_wb_fini(struct amdgpu_device *adev)
{
	if (adev->wb.wb_obj) {
		amdgpu_bo_free_kernel(&adev->wb.wb_obj,
				      &adev->wb.gpu_addr,
				      (void **)&adev->wb.wb);
		adev->wb.wb_obj = NULL;
	}
}

 
static int amdgpu_device_wb_init(struct amdgpu_device *adev)
{
	int r;

	if (adev->wb.wb_obj == NULL) {
		 
		r = amdgpu_bo_create_kernel(adev, AMDGPU_MAX_WB * sizeof(uint32_t) * 8,
					    PAGE_SIZE, AMDGPU_GEM_DOMAIN_GTT,
					    &adev->wb.wb_obj, &adev->wb.gpu_addr,
					    (void **)&adev->wb.wb);
		if (r) {
			dev_warn(adev->dev, "(%d) create WB bo failed\n", r);
			return r;
		}

		adev->wb.num_wb = AMDGPU_MAX_WB;
		memset(&adev->wb.used, 0, sizeof(adev->wb.used));

		 
		memset((char *)adev->wb.wb, 0, AMDGPU_MAX_WB * sizeof(uint32_t) * 8);
	}

	return 0;
}

 
int amdgpu_device_wb_get(struct amdgpu_device *adev, u32 *wb)
{
	unsigned long offset = find_first_zero_bit(adev->wb.used, adev->wb.num_wb);

	if (offset < adev->wb.num_wb) {
		__set_bit(offset, adev->wb.used);
		*wb = offset << 3;  
		return 0;
	} else {
		return -EINVAL;
	}
}

 
void amdgpu_device_wb_free(struct amdgpu_device *adev, u32 wb)
{
	wb >>= 3;
	if (wb < adev->wb.num_wb)
		__clear_bit(wb, adev->wb.used);
}

 
int amdgpu_device_resize_fb_bar(struct amdgpu_device *adev)
{
	int rbar_size = pci_rebar_bytes_to_size(adev->gmc.real_vram_size);
	struct pci_bus *root;
	struct resource *res;
	unsigned int i;
	u16 cmd;
	int r;

	if (!IS_ENABLED(CONFIG_PHYS_ADDR_T_64BIT))
		return 0;

	 
	if (amdgpu_sriov_vf(adev))
		return 0;

	 
	if (adev->gmc.real_vram_size &&
	    (pci_resource_len(adev->pdev, 0) >= adev->gmc.real_vram_size))
		return 0;

	 
	root = adev->pdev->bus;
	while (root->parent)
		root = root->parent;

	pci_bus_for_each_resource(root, res, i) {
		if (res && res->flags & (IORESOURCE_MEM | IORESOURCE_MEM_64) &&
		    res->start > 0x100000000ull)
			break;
	}

	 
	if (!res)
		return 0;

	 
	rbar_size = min(fls(pci_rebar_get_possible_sizes(adev->pdev, 0)) - 1,
			rbar_size);

	 
	pci_read_config_word(adev->pdev, PCI_COMMAND, &cmd);
	pci_write_config_word(adev->pdev, PCI_COMMAND,
			      cmd & ~PCI_COMMAND_MEMORY);

	 
	amdgpu_doorbell_fini(adev);
	if (adev->asic_type >= CHIP_BONAIRE)
		pci_release_resource(adev->pdev, 2);

	pci_release_resource(adev->pdev, 0);

	r = pci_resize_resource(adev->pdev, 0, rbar_size);
	if (r == -ENOSPC)
		DRM_INFO("Not enough PCI address space for a large BAR.");
	else if (r && r != -ENOTSUPP)
		DRM_ERROR("Problem resizing BAR0 (%d).", r);

	pci_assign_unassigned_bus_resources(adev->pdev->bus);

	 
	r = amdgpu_doorbell_init(adev);
	if (r || (pci_resource_flags(adev->pdev, 0) & IORESOURCE_UNSET))
		return -ENODEV;

	pci_write_config_word(adev->pdev, PCI_COMMAND, cmd);

	return 0;
}

static bool amdgpu_device_read_bios(struct amdgpu_device *adev)
{
	if (hweight32(adev->aid_mask) && (adev->flags & AMD_IS_APU))
		return false;

	return true;
}

 
 
bool amdgpu_device_need_post(struct amdgpu_device *adev)
{
	uint32_t reg;

	if (amdgpu_sriov_vf(adev))
		return false;

	if (!amdgpu_device_read_bios(adev))
		return false;

	if (amdgpu_passthrough(adev)) {
		 
		if (adev->asic_type == CHIP_FIJI) {
			int err;
			uint32_t fw_ver;

			err = request_firmware(&adev->pm.fw, "amdgpu/fiji_smc.bin", adev->dev);
			 
			if (err)
				return true;

			fw_ver = *((uint32_t *)adev->pm.fw->data + 69);
			if (fw_ver < 0x00160e00)
				return true;
		}
	}

	 
	if (adev->gmc.xgmi.pending_reset)
		return false;

	if (adev->has_hw_reset) {
		adev->has_hw_reset = false;
		return true;
	}

	 
	if (adev->asic_type >= CHIP_BONAIRE)
		return amdgpu_atombios_scratch_need_asic_init(adev);

	 
	reg = amdgpu_asic_get_config_memsize(adev);

	if ((reg != 0) && (reg != 0xffffffff))
		return false;

	return true;
}

 
bool amdgpu_device_pcie_dynamic_switching_supported(void)
{
#if IS_ENABLED(CONFIG_X86)
	struct cpuinfo_x86 *c = &cpu_data(0);

	if (c->x86_vendor == X86_VENDOR_INTEL)
		return false;
#endif
	return true;
}

 
bool amdgpu_device_should_use_aspm(struct amdgpu_device *adev)
{
	switch (amdgpu_aspm) {
	case -1:
		break;
	case 0:
		return false;
	case 1:
		return true;
	default:
		return false;
	}
	return pcie_aspm_enabled(adev->pdev);
}

bool amdgpu_device_aspm_support_quirk(void)
{
#if IS_ENABLED(CONFIG_X86)
	struct cpuinfo_x86 *c = &cpu_data(0);

	return !(c->x86 == 6 && c->x86_model == INTEL_FAM6_ALDERLAKE);
#else
	return true;
#endif
}

 
 
static unsigned int amdgpu_device_vga_set_decode(struct pci_dev *pdev,
		bool state)
{
	struct amdgpu_device *adev = drm_to_adev(pci_get_drvdata(pdev));

	amdgpu_asic_set_vga_state(adev, state);
	if (state)
		return VGA_RSRC_LEGACY_IO | VGA_RSRC_LEGACY_MEM |
		       VGA_RSRC_NORMAL_IO | VGA_RSRC_NORMAL_MEM;
	else
		return VGA_RSRC_NORMAL_IO | VGA_RSRC_NORMAL_MEM;
}

 
static void amdgpu_device_check_block_size(struct amdgpu_device *adev)
{
	 
	if (amdgpu_vm_block_size == -1)
		return;

	if (amdgpu_vm_block_size < 9) {
		dev_warn(adev->dev, "VM page table size (%d) too small\n",
			 amdgpu_vm_block_size);
		amdgpu_vm_block_size = -1;
	}
}

 
static void amdgpu_device_check_vm_size(struct amdgpu_device *adev)
{
	 
	if (amdgpu_vm_size == -1)
		return;

	if (amdgpu_vm_size < 1) {
		dev_warn(adev->dev, "VM size (%d) too small, min is 1GB\n",
			 amdgpu_vm_size);
		amdgpu_vm_size = -1;
	}
}

static void amdgpu_device_check_smu_prv_buffer_size(struct amdgpu_device *adev)
{
	struct sysinfo si;
	bool is_os_64 = (sizeof(void *) == 8);
	uint64_t total_memory;
	uint64_t dram_size_seven_GB = 0x1B8000000;
	uint64_t dram_size_three_GB = 0xB8000000;

	if (amdgpu_smu_memory_pool_size == 0)
		return;

	if (!is_os_64) {
		DRM_WARN("Not 64-bit OS, feature not supported\n");
		goto def_value;
	}
	si_meminfo(&si);
	total_memory = (uint64_t)si.totalram * si.mem_unit;

	if ((amdgpu_smu_memory_pool_size == 1) ||
		(amdgpu_smu_memory_pool_size == 2)) {
		if (total_memory < dram_size_three_GB)
			goto def_value1;
	} else if ((amdgpu_smu_memory_pool_size == 4) ||
		(amdgpu_smu_memory_pool_size == 8)) {
		if (total_memory < dram_size_seven_GB)
			goto def_value1;
	} else {
		DRM_WARN("Smu memory pool size not supported\n");
		goto def_value;
	}
	adev->pm.smu_prv_buffer_size = amdgpu_smu_memory_pool_size << 28;

	return;

def_value1:
	DRM_WARN("No enough system memory\n");
def_value:
	adev->pm.smu_prv_buffer_size = 0;
}

static int amdgpu_device_init_apu_flags(struct amdgpu_device *adev)
{
	if (!(adev->flags & AMD_IS_APU) ||
	    adev->asic_type < CHIP_RAVEN)
		return 0;

	switch (adev->asic_type) {
	case CHIP_RAVEN:
		if (adev->pdev->device == 0x15dd)
			adev->apu_flags |= AMD_APU_IS_RAVEN;
		if (adev->pdev->device == 0x15d8)
			adev->apu_flags |= AMD_APU_IS_PICASSO;
		break;
	case CHIP_RENOIR:
		if ((adev->pdev->device == 0x1636) ||
		    (adev->pdev->device == 0x164c))
			adev->apu_flags |= AMD_APU_IS_RENOIR;
		else
			adev->apu_flags |= AMD_APU_IS_GREEN_SARDINE;
		break;
	case CHIP_VANGOGH:
		adev->apu_flags |= AMD_APU_IS_VANGOGH;
		break;
	case CHIP_YELLOW_CARP:
		break;
	case CHIP_CYAN_SKILLFISH:
		if ((adev->pdev->device == 0x13FE) ||
		    (adev->pdev->device == 0x143F))
			adev->apu_flags |= AMD_APU_IS_CYAN_SKILLFISH2;
		break;
	default:
		break;
	}

	return 0;
}

 
static int amdgpu_device_check_arguments(struct amdgpu_device *adev)
{
	if (amdgpu_sched_jobs < 4) {
		dev_warn(adev->dev, "sched jobs (%d) must be at least 4\n",
			 amdgpu_sched_jobs);
		amdgpu_sched_jobs = 4;
	} else if (!is_power_of_2(amdgpu_sched_jobs)) {
		dev_warn(adev->dev, "sched jobs (%d) must be a power of 2\n",
			 amdgpu_sched_jobs);
		amdgpu_sched_jobs = roundup_pow_of_two(amdgpu_sched_jobs);
	}

	if (amdgpu_gart_size != -1 && amdgpu_gart_size < 32) {
		 
		dev_warn(adev->dev, "gart size (%d) too small\n",
			 amdgpu_gart_size);
		amdgpu_gart_size = -1;
	}

	if (amdgpu_gtt_size != -1 && amdgpu_gtt_size < 32) {
		 
		dev_warn(adev->dev, "gtt size (%d) too small\n",
				 amdgpu_gtt_size);
		amdgpu_gtt_size = -1;
	}

	 
	if (amdgpu_vm_fragment_size != -1 &&
	    (amdgpu_vm_fragment_size > 9 || amdgpu_vm_fragment_size < 4)) {
		dev_warn(adev->dev, "valid range is between 4 and 9\n");
		amdgpu_vm_fragment_size = -1;
	}

	if (amdgpu_sched_hw_submission < 2) {
		dev_warn(adev->dev, "sched hw submission jobs (%d) must be at least 2\n",
			 amdgpu_sched_hw_submission);
		amdgpu_sched_hw_submission = 2;
	} else if (!is_power_of_2(amdgpu_sched_hw_submission)) {
		dev_warn(adev->dev, "sched hw submission jobs (%d) must be a power of 2\n",
			 amdgpu_sched_hw_submission);
		amdgpu_sched_hw_submission = roundup_pow_of_two(amdgpu_sched_hw_submission);
	}

	if (amdgpu_reset_method < -1 || amdgpu_reset_method > 4) {
		dev_warn(adev->dev, "invalid option for reset method, reverting to default\n");
		amdgpu_reset_method = -1;
	}

	amdgpu_device_check_smu_prv_buffer_size(adev);

	amdgpu_device_check_vm_size(adev);

	amdgpu_device_check_block_size(adev);

	adev->firmware.load_type = amdgpu_ucode_get_load_type(adev, amdgpu_fw_load_type);

	return 0;
}

 
static void amdgpu_switcheroo_set_state(struct pci_dev *pdev,
					enum vga_switcheroo_state state)
{
	struct drm_device *dev = pci_get_drvdata(pdev);
	int r;

	if (amdgpu_device_supports_px(dev) && state == VGA_SWITCHEROO_OFF)
		return;

	if (state == VGA_SWITCHEROO_ON) {
		pr_info("switched on\n");
		 
		dev->switch_power_state = DRM_SWITCH_POWER_CHANGING;

		pci_set_power_state(pdev, PCI_D0);
		amdgpu_device_load_pci_state(pdev);
		r = pci_enable_device(pdev);
		if (r)
			DRM_WARN("pci_enable_device failed (%d)\n", r);
		amdgpu_device_resume(dev, true);

		dev->switch_power_state = DRM_SWITCH_POWER_ON;
	} else {
		pr_info("switched off\n");
		dev->switch_power_state = DRM_SWITCH_POWER_CHANGING;
		amdgpu_device_suspend(dev, true);
		amdgpu_device_cache_pci_state(pdev);
		 
		pci_disable_device(pdev);
		pci_set_power_state(pdev, PCI_D3cold);
		dev->switch_power_state = DRM_SWITCH_POWER_OFF;
	}
}

 
static bool amdgpu_switcheroo_can_switch(struct pci_dev *pdev)
{
	struct drm_device *dev = pci_get_drvdata(pdev);

        
	return atomic_read(&dev->open_count) == 0;
}

static const struct vga_switcheroo_client_ops amdgpu_switcheroo_ops = {
	.set_gpu_state = amdgpu_switcheroo_set_state,
	.reprobe = NULL,
	.can_switch = amdgpu_switcheroo_can_switch,
};

 
int amdgpu_device_ip_set_clockgating_state(void *dev,
					   enum amd_ip_block_type block_type,
					   enum amd_clockgating_state state)
{
	struct amdgpu_device *adev = dev;
	int i, r = 0;

	for (i = 0; i < adev->num_ip_blocks; i++) {
		if (!adev->ip_blocks[i].status.valid)
			continue;
		if (adev->ip_blocks[i].version->type != block_type)
			continue;
		if (!adev->ip_blocks[i].version->funcs->set_clockgating_state)
			continue;
		r = adev->ip_blocks[i].version->funcs->set_clockgating_state(
			(void *)adev, state);
		if (r)
			DRM_ERROR("set_clockgating_state of IP block <%s> failed %d\n",
				  adev->ip_blocks[i].version->funcs->name, r);
	}
	return r;
}

 
int amdgpu_device_ip_set_powergating_state(void *dev,
					   enum amd_ip_block_type block_type,
					   enum amd_powergating_state state)
{
	struct amdgpu_device *adev = dev;
	int i, r = 0;

	for (i = 0; i < adev->num_ip_blocks; i++) {
		if (!adev->ip_blocks[i].status.valid)
			continue;
		if (adev->ip_blocks[i].version->type != block_type)
			continue;
		if (!adev->ip_blocks[i].version->funcs->set_powergating_state)
			continue;
		r = adev->ip_blocks[i].version->funcs->set_powergating_state(
			(void *)adev, state);
		if (r)
			DRM_ERROR("set_powergating_state of IP block <%s> failed %d\n",
				  adev->ip_blocks[i].version->funcs->name, r);
	}
	return r;
}

 
void amdgpu_device_ip_get_clockgating_state(struct amdgpu_device *adev,
					    u64 *flags)
{
	int i;

	for (i = 0; i < adev->num_ip_blocks; i++) {
		if (!adev->ip_blocks[i].status.valid)
			continue;
		if (adev->ip_blocks[i].version->funcs->get_clockgating_state)
			adev->ip_blocks[i].version->funcs->get_clockgating_state((void *)adev, flags);
	}
}

 
int amdgpu_device_ip_wait_for_idle(struct amdgpu_device *adev,
				   enum amd_ip_block_type block_type)
{
	int i, r;

	for (i = 0; i < adev->num_ip_blocks; i++) {
		if (!adev->ip_blocks[i].status.valid)
			continue;
		if (adev->ip_blocks[i].version->type == block_type) {
			r = adev->ip_blocks[i].version->funcs->wait_for_idle((void *)adev);
			if (r)
				return r;
			break;
		}
	}
	return 0;

}

 
bool amdgpu_device_ip_is_idle(struct amdgpu_device *adev,
			      enum amd_ip_block_type block_type)
{
	int i;

	for (i = 0; i < adev->num_ip_blocks; i++) {
		if (!adev->ip_blocks[i].status.valid)
			continue;
		if (adev->ip_blocks[i].version->type == block_type)
			return adev->ip_blocks[i].version->funcs->is_idle((void *)adev);
	}
	return true;

}

 
struct amdgpu_ip_block *
amdgpu_device_ip_get_ip_block(struct amdgpu_device *adev,
			      enum amd_ip_block_type type)
{
	int i;

	for (i = 0; i < adev->num_ip_blocks; i++)
		if (adev->ip_blocks[i].version->type == type)
			return &adev->ip_blocks[i];

	return NULL;
}

 
int amdgpu_device_ip_block_version_cmp(struct amdgpu_device *adev,
				       enum amd_ip_block_type type,
				       u32 major, u32 minor)
{
	struct amdgpu_ip_block *ip_block = amdgpu_device_ip_get_ip_block(adev, type);

	if (ip_block && ((ip_block->version->major > major) ||
			((ip_block->version->major == major) &&
			(ip_block->version->minor >= minor))))
		return 0;

	return 1;
}

 
int amdgpu_device_ip_block_add(struct amdgpu_device *adev,
			       const struct amdgpu_ip_block_version *ip_block_version)
{
	if (!ip_block_version)
		return -EINVAL;

	switch (ip_block_version->type) {
	case AMD_IP_BLOCK_TYPE_VCN:
		if (adev->harvest_ip_mask & AMD_HARVEST_IP_VCN_MASK)
			return 0;
		break;
	case AMD_IP_BLOCK_TYPE_JPEG:
		if (adev->harvest_ip_mask & AMD_HARVEST_IP_JPEG_MASK)
			return 0;
		break;
	default:
		break;
	}

	DRM_INFO("add ip block number %d <%s>\n", adev->num_ip_blocks,
		  ip_block_version->funcs->name);

	adev->ip_blocks[adev->num_ip_blocks++].version = ip_block_version;

	return 0;
}

 
static void amdgpu_device_enable_virtual_display(struct amdgpu_device *adev)
{
	adev->enable_virtual_display = false;

	if (amdgpu_virtual_display) {
		const char *pci_address_name = pci_name(adev->pdev);
		char *pciaddstr, *pciaddstr_tmp, *pciaddname_tmp, *pciaddname;

		pciaddstr = kstrdup(amdgpu_virtual_display, GFP_KERNEL);
		pciaddstr_tmp = pciaddstr;
		while ((pciaddname_tmp = strsep(&pciaddstr_tmp, ";"))) {
			pciaddname = strsep(&pciaddname_tmp, ",");
			if (!strcmp("all", pciaddname)
			    || !strcmp(pci_address_name, pciaddname)) {
				long num_crtc;
				int res = -1;

				adev->enable_virtual_display = true;

				if (pciaddname_tmp)
					res = kstrtol(pciaddname_tmp, 10,
						      &num_crtc);

				if (!res) {
					if (num_crtc < 1)
						num_crtc = 1;
					if (num_crtc > 6)
						num_crtc = 6;
					adev->mode_info.num_crtc = num_crtc;
				} else {
					adev->mode_info.num_crtc = 1;
				}
				break;
			}
		}

		DRM_INFO("virtual display string:%s, %s:virtual_display:%d, num_crtc:%d\n",
			 amdgpu_virtual_display, pci_address_name,
			 adev->enable_virtual_display, adev->mode_info.num_crtc);

		kfree(pciaddstr);
	}
}

void amdgpu_device_set_sriov_virtual_display(struct amdgpu_device *adev)
{
	if (amdgpu_sriov_vf(adev) && !adev->enable_virtual_display) {
		adev->mode_info.num_crtc = 1;
		adev->enable_virtual_display = true;
		DRM_INFO("virtual_display:%d, num_crtc:%d\n",
			 adev->enable_virtual_display, adev->mode_info.num_crtc);
	}
}

 
static int amdgpu_device_parse_gpu_info_fw(struct amdgpu_device *adev)
{
	const char *chip_name;
	char fw_name[40];
	int err;
	const struct gpu_info_firmware_header_v1_0 *hdr;

	adev->firmware.gpu_info_fw = NULL;

	if (adev->mman.discovery_bin)
		return 0;

	switch (adev->asic_type) {
	default:
		return 0;
	case CHIP_VEGA10:
		chip_name = "vega10";
		break;
	case CHIP_VEGA12:
		chip_name = "vega12";
		break;
	case CHIP_RAVEN:
		if (adev->apu_flags & AMD_APU_IS_RAVEN2)
			chip_name = "raven2";
		else if (adev->apu_flags & AMD_APU_IS_PICASSO)
			chip_name = "picasso";
		else
			chip_name = "raven";
		break;
	case CHIP_ARCTURUS:
		chip_name = "arcturus";
		break;
	case CHIP_NAVI12:
		chip_name = "navi12";
		break;
	}

	snprintf(fw_name, sizeof(fw_name), "amdgpu/%s_gpu_info.bin", chip_name);
	err = amdgpu_ucode_request(adev, &adev->firmware.gpu_info_fw, fw_name);
	if (err) {
		dev_err(adev->dev,
			"Failed to get gpu_info firmware \"%s\"\n",
			fw_name);
		goto out;
	}

	hdr = (const struct gpu_info_firmware_header_v1_0 *)adev->firmware.gpu_info_fw->data;
	amdgpu_ucode_print_gpu_info_hdr(&hdr->header);

	switch (hdr->version_major) {
	case 1:
	{
		const struct gpu_info_firmware_v1_0 *gpu_info_fw =
			(const struct gpu_info_firmware_v1_0 *)(adev->firmware.gpu_info_fw->data +
								le32_to_cpu(hdr->header.ucode_array_offset_bytes));

		 
		if (adev->asic_type == CHIP_NAVI12)
			goto parse_soc_bounding_box;

		adev->gfx.config.max_shader_engines = le32_to_cpu(gpu_info_fw->gc_num_se);
		adev->gfx.config.max_cu_per_sh = le32_to_cpu(gpu_info_fw->gc_num_cu_per_sh);
		adev->gfx.config.max_sh_per_se = le32_to_cpu(gpu_info_fw->gc_num_sh_per_se);
		adev->gfx.config.max_backends_per_se = le32_to_cpu(gpu_info_fw->gc_num_rb_per_se);
		adev->gfx.config.max_texture_channel_caches =
			le32_to_cpu(gpu_info_fw->gc_num_tccs);
		adev->gfx.config.max_gprs = le32_to_cpu(gpu_info_fw->gc_num_gprs);
		adev->gfx.config.max_gs_threads = le32_to_cpu(gpu_info_fw->gc_num_max_gs_thds);
		adev->gfx.config.gs_vgt_table_depth = le32_to_cpu(gpu_info_fw->gc_gs_table_depth);
		adev->gfx.config.gs_prim_buffer_depth = le32_to_cpu(gpu_info_fw->gc_gsprim_buff_depth);
		adev->gfx.config.double_offchip_lds_buf =
			le32_to_cpu(gpu_info_fw->gc_double_offchip_lds_buffer);
		adev->gfx.cu_info.wave_front_size = le32_to_cpu(gpu_info_fw->gc_wave_size);
		adev->gfx.cu_info.max_waves_per_simd =
			le32_to_cpu(gpu_info_fw->gc_max_waves_per_simd);
		adev->gfx.cu_info.max_scratch_slots_per_cu =
			le32_to_cpu(gpu_info_fw->gc_max_scratch_slots_per_cu);
		adev->gfx.cu_info.lds_size = le32_to_cpu(gpu_info_fw->gc_lds_size);
		if (hdr->version_minor >= 1) {
			const struct gpu_info_firmware_v1_1 *gpu_info_fw =
				(const struct gpu_info_firmware_v1_1 *)(adev->firmware.gpu_info_fw->data +
									le32_to_cpu(hdr->header.ucode_array_offset_bytes));
			adev->gfx.config.num_sc_per_sh =
				le32_to_cpu(gpu_info_fw->num_sc_per_sh);
			adev->gfx.config.num_packer_per_sc =
				le32_to_cpu(gpu_info_fw->num_packer_per_sc);
		}

parse_soc_bounding_box:
		 
		if (hdr->version_minor == 2) {
			const struct gpu_info_firmware_v1_2 *gpu_info_fw =
				(const struct gpu_info_firmware_v1_2 *)(adev->firmware.gpu_info_fw->data +
									le32_to_cpu(hdr->header.ucode_array_offset_bytes));
			adev->dm.soc_bounding_box = &gpu_info_fw->soc_bounding_box;
		}
		break;
	}
	default:
		dev_err(adev->dev,
			"Unsupported gpu_info table %d\n", hdr->header.ucode_version);
		err = -EINVAL;
		goto out;
	}
out:
	return err;
}

 
static int amdgpu_device_ip_early_init(struct amdgpu_device *adev)
{
	struct pci_dev *parent;
	int i, r;
	bool total;

	amdgpu_device_enable_virtual_display(adev);

	if (amdgpu_sriov_vf(adev)) {
		r = amdgpu_virt_request_full_gpu(adev, true);
		if (r)
			return r;
	}

	switch (adev->asic_type) {
#ifdef CONFIG_DRM_AMDGPU_SI
	case CHIP_VERDE:
	case CHIP_TAHITI:
	case CHIP_PITCAIRN:
	case CHIP_OLAND:
	case CHIP_HAINAN:
		adev->family = AMDGPU_FAMILY_SI;
		r = si_set_ip_blocks(adev);
		if (r)
			return r;
		break;
#endif
#ifdef CONFIG_DRM_AMDGPU_CIK
	case CHIP_BONAIRE:
	case CHIP_HAWAII:
	case CHIP_KAVERI:
	case CHIP_KABINI:
	case CHIP_MULLINS:
		if (adev->flags & AMD_IS_APU)
			adev->family = AMDGPU_FAMILY_KV;
		else
			adev->family = AMDGPU_FAMILY_CI;

		r = cik_set_ip_blocks(adev);
		if (r)
			return r;
		break;
#endif
	case CHIP_TOPAZ:
	case CHIP_TONGA:
	case CHIP_FIJI:
	case CHIP_POLARIS10:
	case CHIP_POLARIS11:
	case CHIP_POLARIS12:
	case CHIP_VEGAM:
	case CHIP_CARRIZO:
	case CHIP_STONEY:
		if (adev->flags & AMD_IS_APU)
			adev->family = AMDGPU_FAMILY_CZ;
		else
			adev->family = AMDGPU_FAMILY_VI;

		r = vi_set_ip_blocks(adev);
		if (r)
			return r;
		break;
	default:
		r = amdgpu_discovery_set_ip_blocks(adev);
		if (r)
			return r;
		break;
	}

	if (amdgpu_has_atpx() &&
	    (amdgpu_is_atpx_hybrid() ||
	     amdgpu_has_atpx_dgpu_power_cntl()) &&
	    ((adev->flags & AMD_IS_APU) == 0) &&
	    !dev_is_removable(&adev->pdev->dev))
		adev->flags |= AMD_IS_PX;

	if (!(adev->flags & AMD_IS_APU)) {
		parent = pcie_find_root_port(adev->pdev);
		adev->has_pr3 = parent ? pci_pr3_present(parent) : false;
	}


	adev->pm.pp_feature = amdgpu_pp_feature_mask;
	if (amdgpu_sriov_vf(adev) || sched_policy == KFD_SCHED_POLICY_NO_HWS)
		adev->pm.pp_feature &= ~PP_GFXOFF_MASK;
	if (amdgpu_sriov_vf(adev) && adev->asic_type == CHIP_SIENNA_CICHLID)
		adev->pm.pp_feature &= ~PP_OVERDRIVE_MASK;
	if (!amdgpu_device_pcie_dynamic_switching_supported())
		adev->pm.pp_feature &= ~PP_PCIE_DPM_MASK;

	total = true;
	for (i = 0; i < adev->num_ip_blocks; i++) {
		if ((amdgpu_ip_block_mask & (1 << i)) == 0) {
			DRM_WARN("disabled ip block: %d <%s>\n",
				  i, adev->ip_blocks[i].version->funcs->name);
			adev->ip_blocks[i].status.valid = false;
		} else {
			if (adev->ip_blocks[i].version->funcs->early_init) {
				r = adev->ip_blocks[i].version->funcs->early_init((void *)adev);
				if (r == -ENOENT) {
					adev->ip_blocks[i].status.valid = false;
				} else if (r) {
					DRM_ERROR("early_init of IP block <%s> failed %d\n",
						  adev->ip_blocks[i].version->funcs->name, r);
					total = false;
				} else {
					adev->ip_blocks[i].status.valid = true;
				}
			} else {
				adev->ip_blocks[i].status.valid = true;
			}
		}
		 
		if (adev->ip_blocks[i].version->type == AMD_IP_BLOCK_TYPE_COMMON) {
			r = amdgpu_device_parse_gpu_info_fw(adev);
			if (r)
				return r;

			 
			if (amdgpu_device_read_bios(adev)) {
				if (!amdgpu_get_bios(adev))
					return -EINVAL;

				r = amdgpu_atombios_init(adev);
				if (r) {
					dev_err(adev->dev, "amdgpu_atombios_init failed\n");
					amdgpu_vf_error_put(adev, AMDGIM_ERROR_VF_ATOMBIOS_INIT_FAIL, 0, 0);
					return r;
				}
			}

			 
			if (amdgpu_sriov_vf(adev))
				amdgpu_virt_init_data_exchange(adev);

		}
	}
	if (!total)
		return -ENODEV;

	amdgpu_amdkfd_device_probe(adev);
	adev->cg_flags &= amdgpu_cg_mask;
	adev->pg_flags &= amdgpu_pg_mask;

	return 0;
}

static int amdgpu_device_ip_hw_init_phase1(struct amdgpu_device *adev)
{
	int i, r;

	for (i = 0; i < adev->num_ip_blocks; i++) {
		if (!adev->ip_blocks[i].status.sw)
			continue;
		if (adev->ip_blocks[i].status.hw)
			continue;
		if (adev->ip_blocks[i].version->type == AMD_IP_BLOCK_TYPE_COMMON ||
		    (amdgpu_sriov_vf(adev) && (adev->ip_blocks[i].version->type == AMD_IP_BLOCK_TYPE_PSP)) ||
		    adev->ip_blocks[i].version->type == AMD_IP_BLOCK_TYPE_IH) {
			r = adev->ip_blocks[i].version->funcs->hw_init(adev);
			if (r) {
				DRM_ERROR("hw_init of IP block <%s> failed %d\n",
					  adev->ip_blocks[i].version->funcs->name, r);
				return r;
			}
			adev->ip_blocks[i].status.hw = true;
		}
	}

	return 0;
}

static int amdgpu_device_ip_hw_init_phase2(struct amdgpu_device *adev)
{
	int i, r;

	for (i = 0; i < adev->num_ip_blocks; i++) {
		if (!adev->ip_blocks[i].status.sw)
			continue;
		if (adev->ip_blocks[i].status.hw)
			continue;
		r = adev->ip_blocks[i].version->funcs->hw_init(adev);
		if (r) {
			DRM_ERROR("hw_init of IP block <%s> failed %d\n",
				  adev->ip_blocks[i].version->funcs->name, r);
			return r;
		}
		adev->ip_blocks[i].status.hw = true;
	}

	return 0;
}

static int amdgpu_device_fw_loading(struct amdgpu_device *adev)
{
	int r = 0;
	int i;
	uint32_t smu_version;

	if (adev->asic_type >= CHIP_VEGA10) {
		for (i = 0; i < adev->num_ip_blocks; i++) {
			if (adev->ip_blocks[i].version->type != AMD_IP_BLOCK_TYPE_PSP)
				continue;

			if (!adev->ip_blocks[i].status.sw)
				continue;

			 
			if (adev->ip_blocks[i].status.hw == true)
				break;

			if (amdgpu_in_reset(adev) || adev->in_suspend) {
				r = adev->ip_blocks[i].version->funcs->resume(adev);
				if (r) {
					DRM_ERROR("resume of IP block <%s> failed %d\n",
							  adev->ip_blocks[i].version->funcs->name, r);
					return r;
				}
			} else {
				r = adev->ip_blocks[i].version->funcs->hw_init(adev);
				if (r) {
					DRM_ERROR("hw_init of IP block <%s> failed %d\n",
							  adev->ip_blocks[i].version->funcs->name, r);
					return r;
				}
			}

			adev->ip_blocks[i].status.hw = true;
			break;
		}
	}

	if (!amdgpu_sriov_vf(adev) || adev->asic_type == CHIP_TONGA)
		r = amdgpu_pm_load_smu_firmware(adev, &smu_version);

	return r;
}

static int amdgpu_device_init_schedulers(struct amdgpu_device *adev)
{
	long timeout;
	int r, i;

	for (i = 0; i < AMDGPU_MAX_RINGS; ++i) {
		struct amdgpu_ring *ring = adev->rings[i];

		 
		if (!ring || ring->no_scheduler)
			continue;

		switch (ring->funcs->type) {
		case AMDGPU_RING_TYPE_GFX:
			timeout = adev->gfx_timeout;
			break;
		case AMDGPU_RING_TYPE_COMPUTE:
			timeout = adev->compute_timeout;
			break;
		case AMDGPU_RING_TYPE_SDMA:
			timeout = adev->sdma_timeout;
			break;
		default:
			timeout = adev->video_timeout;
			break;
		}

		r = drm_sched_init(&ring->sched, &amdgpu_sched_ops,
				   ring->num_hw_submission, 0,
				   timeout, adev->reset_domain->wq,
				   ring->sched_score, ring->name,
				   adev->dev);
		if (r) {
			DRM_ERROR("Failed to create scheduler on ring %s.\n",
				  ring->name);
			return r;
		}
	}

	amdgpu_xcp_update_partition_sched_list(adev);

	return 0;
}


 
static int amdgpu_device_ip_init(struct amdgpu_device *adev)
{
	int i, r;

	r = amdgpu_ras_init(adev);
	if (r)
		return r;

	for (i = 0; i < adev->num_ip_blocks; i++) {
		if (!adev->ip_blocks[i].status.valid)
			continue;
		r = adev->ip_blocks[i].version->funcs->sw_init((void *)adev);
		if (r) {
			DRM_ERROR("sw_init of IP block <%s> failed %d\n",
				  adev->ip_blocks[i].version->funcs->name, r);
			goto init_failed;
		}
		adev->ip_blocks[i].status.sw = true;

		if (adev->ip_blocks[i].version->type == AMD_IP_BLOCK_TYPE_COMMON) {
			 
			r = adev->ip_blocks[i].version->funcs->hw_init((void *)adev);
			if (r) {
				DRM_ERROR("hw_init %d failed %d\n", i, r);
				goto init_failed;
			}
			adev->ip_blocks[i].status.hw = true;
		} else if (adev->ip_blocks[i].version->type == AMD_IP_BLOCK_TYPE_GMC) {
			 
			 
			if (amdgpu_sriov_vf(adev))
				amdgpu_virt_exchange_data(adev);

			r = amdgpu_device_mem_scratch_init(adev);
			if (r) {
				DRM_ERROR("amdgpu_mem_scratch_init failed %d\n", r);
				goto init_failed;
			}
			r = adev->ip_blocks[i].version->funcs->hw_init((void *)adev);
			if (r) {
				DRM_ERROR("hw_init %d failed %d\n", i, r);
				goto init_failed;
			}
			r = amdgpu_device_wb_init(adev);
			if (r) {
				DRM_ERROR("amdgpu_device_wb_init failed %d\n", r);
				goto init_failed;
			}
			adev->ip_blocks[i].status.hw = true;

			 
			if (adev->gfx.mcbp) {
				r = amdgpu_allocate_static_csa(adev, &adev->virt.csa_obj,
							       AMDGPU_GEM_DOMAIN_VRAM |
							       AMDGPU_GEM_DOMAIN_GTT,
							       AMDGPU_CSA_SIZE);
				if (r) {
					DRM_ERROR("allocate CSA failed %d\n", r);
					goto init_failed;
				}
			}
		}
	}

	if (amdgpu_sriov_vf(adev))
		amdgpu_virt_init_data_exchange(adev);

	r = amdgpu_ib_pool_init(adev);
	if (r) {
		dev_err(adev->dev, "IB initialization failed (%d).\n", r);
		amdgpu_vf_error_put(adev, AMDGIM_ERROR_VF_IB_INIT_FAIL, 0, r);
		goto init_failed;
	}

	r = amdgpu_ucode_create_bo(adev);  
	if (r)
		goto init_failed;

	r = amdgpu_device_ip_hw_init_phase1(adev);
	if (r)
		goto init_failed;

	r = amdgpu_device_fw_loading(adev);
	if (r)
		goto init_failed;

	r = amdgpu_device_ip_hw_init_phase2(adev);
	if (r)
		goto init_failed;

	 
	r = amdgpu_ras_recovery_init(adev);
	if (r)
		goto init_failed;

	 
	if (adev->gmc.xgmi.num_physical_nodes > 1) {
		if (amdgpu_xgmi_add_device(adev) == 0) {
			if (!amdgpu_sriov_vf(adev)) {
				struct amdgpu_hive_info *hive = amdgpu_get_xgmi_hive(adev);

				if (WARN_ON(!hive)) {
					r = -ENOENT;
					goto init_failed;
				}

				if (!hive->reset_domain ||
				    !amdgpu_reset_get_reset_domain(hive->reset_domain)) {
					r = -ENOENT;
					amdgpu_put_xgmi_hive(hive);
					goto init_failed;
				}

				 
				amdgpu_reset_put_reset_domain(adev->reset_domain);
				adev->reset_domain = hive->reset_domain;
				amdgpu_put_xgmi_hive(hive);
			}
		}
	}

	r = amdgpu_device_init_schedulers(adev);
	if (r)
		goto init_failed;

	 
	if (!adev->gmc.xgmi.pending_reset) {
		kgd2kfd_init_zone_device(adev);
		amdgpu_amdkfd_device_init(adev);
	}

	amdgpu_fru_get_product_info(adev);

init_failed:

	return r;
}

 
static void amdgpu_device_fill_reset_magic(struct amdgpu_device *adev)
{
	memcpy(adev->reset_magic, adev->gart.ptr, AMDGPU_RESET_MAGIC_NUM);
}

 
static bool amdgpu_device_check_vram_lost(struct amdgpu_device *adev)
{
	if (memcmp(adev->gart.ptr, adev->reset_magic,
			AMDGPU_RESET_MAGIC_NUM))
		return true;

	if (!amdgpu_in_reset(adev))
		return false;

	 
	switch (amdgpu_asic_reset_method(adev)) {
	case AMD_RESET_METHOD_BACO:
	case AMD_RESET_METHOD_MODE1:
		return true;
	default:
		return false;
	}
}

 

int amdgpu_device_set_cg_state(struct amdgpu_device *adev,
			       enum amd_clockgating_state state)
{
	int i, j, r;

	if (amdgpu_emu_mode == 1)
		return 0;

	for (j = 0; j < adev->num_ip_blocks; j++) {
		i = state == AMD_CG_STATE_GATE ? j : adev->num_ip_blocks - j - 1;
		if (!adev->ip_blocks[i].status.late_initialized)
			continue;
		 
		if (adev->in_s0ix &&
		    (adev->ip_blocks[i].version->type == AMD_IP_BLOCK_TYPE_GFX ||
		     adev->ip_blocks[i].version->type == AMD_IP_BLOCK_TYPE_SDMA))
			continue;
		 
		if (adev->ip_blocks[i].version->type != AMD_IP_BLOCK_TYPE_UVD &&
		    adev->ip_blocks[i].version->type != AMD_IP_BLOCK_TYPE_VCE &&
		    adev->ip_blocks[i].version->type != AMD_IP_BLOCK_TYPE_VCN &&
		    adev->ip_blocks[i].version->type != AMD_IP_BLOCK_TYPE_JPEG &&
		    adev->ip_blocks[i].version->funcs->set_clockgating_state) {
			 
			r = adev->ip_blocks[i].version->funcs->set_clockgating_state((void *)adev,
										     state);
			if (r) {
				DRM_ERROR("set_clockgating_state(gate) of IP block <%s> failed %d\n",
					  adev->ip_blocks[i].version->funcs->name, r);
				return r;
			}
		}
	}

	return 0;
}

int amdgpu_device_set_pg_state(struct amdgpu_device *adev,
			       enum amd_powergating_state state)
{
	int i, j, r;

	if (amdgpu_emu_mode == 1)
		return 0;

	for (j = 0; j < adev->num_ip_blocks; j++) {
		i = state == AMD_PG_STATE_GATE ? j : adev->num_ip_blocks - j - 1;
		if (!adev->ip_blocks[i].status.late_initialized)
			continue;
		 
		if (adev->in_s0ix &&
		    (adev->ip_blocks[i].version->type == AMD_IP_BLOCK_TYPE_GFX ||
		     adev->ip_blocks[i].version->type == AMD_IP_BLOCK_TYPE_SDMA))
			continue;
		 
		if (adev->ip_blocks[i].version->type != AMD_IP_BLOCK_TYPE_UVD &&
		    adev->ip_blocks[i].version->type != AMD_IP_BLOCK_TYPE_VCE &&
		    adev->ip_blocks[i].version->type != AMD_IP_BLOCK_TYPE_VCN &&
		    adev->ip_blocks[i].version->type != AMD_IP_BLOCK_TYPE_JPEG &&
		    adev->ip_blocks[i].version->funcs->set_powergating_state) {
			 
			r = adev->ip_blocks[i].version->funcs->set_powergating_state((void *)adev,
											state);
			if (r) {
				DRM_ERROR("set_powergating_state(gate) of IP block <%s> failed %d\n",
					  adev->ip_blocks[i].version->funcs->name, r);
				return r;
			}
		}
	}
	return 0;
}

static int amdgpu_device_enable_mgpu_fan_boost(void)
{
	struct amdgpu_gpu_instance *gpu_ins;
	struct amdgpu_device *adev;
	int i, ret = 0;

	mutex_lock(&mgpu_info.mutex);

	 
	if (mgpu_info.num_dgpu < 2)
		goto out;

	for (i = 0; i < mgpu_info.num_dgpu; i++) {
		gpu_ins = &(mgpu_info.gpu_ins[i]);
		adev = gpu_ins->adev;
		if (!(adev->flags & AMD_IS_APU) &&
		    !gpu_ins->mgpu_fan_enabled) {
			ret = amdgpu_dpm_enable_mgpu_fan_boost(adev);
			if (ret)
				break;

			gpu_ins->mgpu_fan_enabled = 1;
		}
	}

out:
	mutex_unlock(&mgpu_info.mutex);

	return ret;
}

 
static int amdgpu_device_ip_late_init(struct amdgpu_device *adev)
{
	struct amdgpu_gpu_instance *gpu_instance;
	int i = 0, r;

	for (i = 0; i < adev->num_ip_blocks; i++) {
		if (!adev->ip_blocks[i].status.hw)
			continue;
		if (adev->ip_blocks[i].version->funcs->late_init) {
			r = adev->ip_blocks[i].version->funcs->late_init((void *)adev);
			if (r) {
				DRM_ERROR("late_init of IP block <%s> failed %d\n",
					  adev->ip_blocks[i].version->funcs->name, r);
				return r;
			}
		}
		adev->ip_blocks[i].status.late_initialized = true;
	}

	r = amdgpu_ras_late_init(adev);
	if (r) {
		DRM_ERROR("amdgpu_ras_late_init failed %d", r);
		return r;
	}

	amdgpu_ras_set_error_query_ready(adev, true);

	amdgpu_device_set_cg_state(adev, AMD_CG_STATE_GATE);
	amdgpu_device_set_pg_state(adev, AMD_PG_STATE_GATE);

	amdgpu_device_fill_reset_magic(adev);

	r = amdgpu_device_enable_mgpu_fan_boost();
	if (r)
		DRM_ERROR("enable mgpu fan boost failed (%d).\n", r);

	 
	if (amdgpu_passthrough(adev) &&
	    ((adev->asic_type == CHIP_ARCTURUS && adev->gmc.xgmi.num_physical_nodes > 1) ||
	     adev->asic_type == CHIP_ALDEBARAN))
		amdgpu_dpm_handle_passthrough_sbr(adev, true);

	if (adev->gmc.xgmi.num_physical_nodes > 1) {
		mutex_lock(&mgpu_info.mutex);

		 
		if (mgpu_info.num_dgpu == adev->gmc.xgmi.num_physical_nodes) {
			for (i = 0; i < mgpu_info.num_gpu; i++) {
				gpu_instance = &(mgpu_info.gpu_ins[i]);
				if (gpu_instance->adev->flags & AMD_IS_APU)
					continue;

				r = amdgpu_xgmi_set_pstate(gpu_instance->adev,
						AMDGPU_XGMI_PSTATE_MIN);
				if (r) {
					DRM_ERROR("pstate setting failed (%d).\n", r);
					break;
				}
			}
		}

		mutex_unlock(&mgpu_info.mutex);
	}

	return 0;
}

 
static void amdgpu_device_smu_fini_early(struct amdgpu_device *adev)
{
	int i, r;

	if (adev->ip_versions[GC_HWIP][0] > IP_VERSION(9, 0, 0))
		return;

	for (i = 0; i < adev->num_ip_blocks; i++) {
		if (!adev->ip_blocks[i].status.hw)
			continue;
		if (adev->ip_blocks[i].version->type == AMD_IP_BLOCK_TYPE_SMC) {
			r = adev->ip_blocks[i].version->funcs->hw_fini((void *)adev);
			 
			if (r) {
				DRM_DEBUG("hw_fini of IP block <%s> failed %d\n",
					  adev->ip_blocks[i].version->funcs->name, r);
			}
			adev->ip_blocks[i].status.hw = false;
			break;
		}
	}
}

static int amdgpu_device_ip_fini_early(struct amdgpu_device *adev)
{
	int i, r;

	for (i = 0; i < adev->num_ip_blocks; i++) {
		if (!adev->ip_blocks[i].version->funcs->early_fini)
			continue;

		r = adev->ip_blocks[i].version->funcs->early_fini((void *)adev);
		if (r) {
			DRM_DEBUG("early_fini of IP block <%s> failed %d\n",
				  adev->ip_blocks[i].version->funcs->name, r);
		}
	}

	amdgpu_device_set_pg_state(adev, AMD_PG_STATE_UNGATE);
	amdgpu_device_set_cg_state(adev, AMD_CG_STATE_UNGATE);

	amdgpu_amdkfd_suspend(adev, false);

	 
	amdgpu_device_smu_fini_early(adev);

	for (i = adev->num_ip_blocks - 1; i >= 0; i--) {
		if (!adev->ip_blocks[i].status.hw)
			continue;

		r = adev->ip_blocks[i].version->funcs->hw_fini((void *)adev);
		 
		if (r) {
			DRM_DEBUG("hw_fini of IP block <%s> failed %d\n",
				  adev->ip_blocks[i].version->funcs->name, r);
		}

		adev->ip_blocks[i].status.hw = false;
	}

	if (amdgpu_sriov_vf(adev)) {
		if (amdgpu_virt_release_full_gpu(adev, false))
			DRM_ERROR("failed to release exclusive mode on fini\n");
	}

	return 0;
}

 
static int amdgpu_device_ip_fini(struct amdgpu_device *adev)
{
	int i, r;

	if (amdgpu_sriov_vf(adev) && adev->virt.ras_init_done)
		amdgpu_virt_release_ras_err_handler_data(adev);

	if (adev->gmc.xgmi.num_physical_nodes > 1)
		amdgpu_xgmi_remove_device(adev);

	amdgpu_amdkfd_device_fini_sw(adev);

	for (i = adev->num_ip_blocks - 1; i >= 0; i--) {
		if (!adev->ip_blocks[i].status.sw)
			continue;

		if (adev->ip_blocks[i].version->type == AMD_IP_BLOCK_TYPE_GMC) {
			amdgpu_ucode_free_bo(adev);
			amdgpu_free_static_csa(&adev->virt.csa_obj);
			amdgpu_device_wb_fini(adev);
			amdgpu_device_mem_scratch_fini(adev);
			amdgpu_ib_pool_fini(adev);
		}

		r = adev->ip_blocks[i].version->funcs->sw_fini((void *)adev);
		 
		if (r) {
			DRM_DEBUG("sw_fini of IP block <%s> failed %d\n",
				  adev->ip_blocks[i].version->funcs->name, r);
		}
		adev->ip_blocks[i].status.sw = false;
		adev->ip_blocks[i].status.valid = false;
	}

	for (i = adev->num_ip_blocks - 1; i >= 0; i--) {
		if (!adev->ip_blocks[i].status.late_initialized)
			continue;
		if (adev->ip_blocks[i].version->funcs->late_fini)
			adev->ip_blocks[i].version->funcs->late_fini((void *)adev);
		adev->ip_blocks[i].status.late_initialized = false;
	}

	amdgpu_ras_fini(adev);

	return 0;
}

 
static void amdgpu_device_delayed_init_work_handler(struct work_struct *work)
{
	struct amdgpu_device *adev =
		container_of(work, struct amdgpu_device, delayed_init_work.work);
	int r;

	r = amdgpu_ib_ring_tests(adev);
	if (r)
		DRM_ERROR("ib ring test failed (%d).\n", r);
}

static void amdgpu_device_delay_enable_gfx_off(struct work_struct *work)
{
	struct amdgpu_device *adev =
		container_of(work, struct amdgpu_device, gfx.gfx_off_delay_work.work);

	WARN_ON_ONCE(adev->gfx.gfx_off_state);
	WARN_ON_ONCE(adev->gfx.gfx_off_req_count);

	if (!amdgpu_dpm_set_powergating_by_smu(adev, AMD_IP_BLOCK_TYPE_GFX, true))
		adev->gfx.gfx_off_state = true;
}

 
static int amdgpu_device_ip_suspend_phase1(struct amdgpu_device *adev)
{
	int i, r;

	amdgpu_device_set_pg_state(adev, AMD_PG_STATE_UNGATE);
	amdgpu_device_set_cg_state(adev, AMD_CG_STATE_UNGATE);

	 
	if (amdgpu_dpm_set_df_cstate(adev, DF_CSTATE_DISALLOW))
		dev_warn(adev->dev, "Failed to disallow df cstate");

	for (i = adev->num_ip_blocks - 1; i >= 0; i--) {
		if (!adev->ip_blocks[i].status.valid)
			continue;

		 
		if (adev->ip_blocks[i].version->type != AMD_IP_BLOCK_TYPE_DCE)
			continue;

		 
		r = adev->ip_blocks[i].version->funcs->suspend(adev);
		 
		if (r) {
			DRM_ERROR("suspend of IP block <%s> failed %d\n",
				  adev->ip_blocks[i].version->funcs->name, r);
			return r;
		}

		adev->ip_blocks[i].status.hw = false;
	}

	return 0;
}

 
static int amdgpu_device_ip_suspend_phase2(struct amdgpu_device *adev)
{
	int i, r;

	if (adev->in_s0ix)
		amdgpu_dpm_gfx_state_change(adev, sGpuChangeState_D3Entry);

	for (i = adev->num_ip_blocks - 1; i >= 0; i--) {
		if (!adev->ip_blocks[i].status.valid)
			continue;
		 
		if (adev->ip_blocks[i].version->type == AMD_IP_BLOCK_TYPE_DCE)
			continue;
		 
		if (amdgpu_ras_intr_triggered() &&
		    adev->ip_blocks[i].version->type == AMD_IP_BLOCK_TYPE_PSP) {
			adev->ip_blocks[i].status.hw = false;
			continue;
		}

		 
		if (adev->gmc.xgmi.pending_reset &&
		    !(adev->ip_blocks[i].version->type == AMD_IP_BLOCK_TYPE_GMC ||
		      adev->ip_blocks[i].version->type == AMD_IP_BLOCK_TYPE_SMC ||
		      adev->ip_blocks[i].version->type == AMD_IP_BLOCK_TYPE_COMMON ||
		      adev->ip_blocks[i].version->type == AMD_IP_BLOCK_TYPE_IH)) {
			adev->ip_blocks[i].status.hw = false;
			continue;
		}

		 
		if (adev->in_s0ix &&
		    (adev->ip_blocks[i].version->type == AMD_IP_BLOCK_TYPE_PSP ||
		     adev->ip_blocks[i].version->type == AMD_IP_BLOCK_TYPE_GFX ||
		     adev->ip_blocks[i].version->type == AMD_IP_BLOCK_TYPE_MES))
			continue;

		 
		if (adev->in_s0ix &&
		    (adev->ip_versions[SDMA0_HWIP][0] >= IP_VERSION(5, 0, 0)) &&
		    (adev->ip_blocks[i].version->type == AMD_IP_BLOCK_TYPE_SDMA))
			continue;

		 
		if (amdgpu_in_reset(adev) &&
		    (adev->flags & AMD_IS_APU) && adev->gfx.imu.funcs &&
		    adev->ip_blocks[i].version->type == AMD_IP_BLOCK_TYPE_PSP)
			continue;

		 
		r = adev->ip_blocks[i].version->funcs->suspend(adev);
		 
		if (r) {
			DRM_ERROR("suspend of IP block <%s> failed %d\n",
				  adev->ip_blocks[i].version->funcs->name, r);
		}
		adev->ip_blocks[i].status.hw = false;
		 
		if (!amdgpu_sriov_vf(adev)) {
			if (adev->ip_blocks[i].version->type == AMD_IP_BLOCK_TYPE_SMC) {
				r = amdgpu_dpm_set_mp1_state(adev, adev->mp1_state);
				if (r) {
					DRM_ERROR("SMC failed to set mp1 state %d, %d\n",
							adev->mp1_state, r);
					return r;
				}
			}
		}
	}

	return 0;
}

 
int amdgpu_device_ip_suspend(struct amdgpu_device *adev)
{
	int r;

	if (amdgpu_sriov_vf(adev)) {
		amdgpu_virt_fini_data_exchange(adev);
		amdgpu_virt_request_full_gpu(adev, false);
	}

	r = amdgpu_device_ip_suspend_phase1(adev);
	if (r)
		return r;
	r = amdgpu_device_ip_suspend_phase2(adev);

	if (amdgpu_sriov_vf(adev))
		amdgpu_virt_release_full_gpu(adev, false);

	return r;
}

static int amdgpu_device_ip_reinit_early_sriov(struct amdgpu_device *adev)
{
	int i, r;

	static enum amd_ip_block_type ip_order[] = {
		AMD_IP_BLOCK_TYPE_COMMON,
		AMD_IP_BLOCK_TYPE_GMC,
		AMD_IP_BLOCK_TYPE_PSP,
		AMD_IP_BLOCK_TYPE_IH,
	};

	for (i = 0; i < adev->num_ip_blocks; i++) {
		int j;
		struct amdgpu_ip_block *block;

		block = &adev->ip_blocks[i];
		block->status.hw = false;

		for (j = 0; j < ARRAY_SIZE(ip_order); j++) {

			if (block->version->type != ip_order[j] ||
				!block->status.valid)
				continue;

			r = block->version->funcs->hw_init(adev);
			DRM_INFO("RE-INIT-early: %s %s\n", block->version->funcs->name, r?"failed":"succeeded");
			if (r)
				return r;
			block->status.hw = true;
		}
	}

	return 0;
}

static int amdgpu_device_ip_reinit_late_sriov(struct amdgpu_device *adev)
{
	int i, r;

	static enum amd_ip_block_type ip_order[] = {
		AMD_IP_BLOCK_TYPE_SMC,
		AMD_IP_BLOCK_TYPE_DCE,
		AMD_IP_BLOCK_TYPE_GFX,
		AMD_IP_BLOCK_TYPE_SDMA,
		AMD_IP_BLOCK_TYPE_MES,
		AMD_IP_BLOCK_TYPE_UVD,
		AMD_IP_BLOCK_TYPE_VCE,
		AMD_IP_BLOCK_TYPE_VCN,
		AMD_IP_BLOCK_TYPE_JPEG
	};

	for (i = 0; i < ARRAY_SIZE(ip_order); i++) {
		int j;
		struct amdgpu_ip_block *block;

		for (j = 0; j < adev->num_ip_blocks; j++) {
			block = &adev->ip_blocks[j];

			if (block->version->type != ip_order[i] ||
				!block->status.valid ||
				block->status.hw)
				continue;

			if (block->version->type == AMD_IP_BLOCK_TYPE_SMC)
				r = block->version->funcs->resume(adev);
			else
				r = block->version->funcs->hw_init(adev);

			DRM_INFO("RE-INIT-late: %s %s\n", block->version->funcs->name, r?"failed":"succeeded");
			if (r)
				return r;
			block->status.hw = true;
		}
	}

	return 0;
}

 
static int amdgpu_device_ip_resume_phase1(struct amdgpu_device *adev)
{
	int i, r;

	for (i = 0; i < adev->num_ip_blocks; i++) {
		if (!adev->ip_blocks[i].status.valid || adev->ip_blocks[i].status.hw)
			continue;
		if (adev->ip_blocks[i].version->type == AMD_IP_BLOCK_TYPE_COMMON ||
		    adev->ip_blocks[i].version->type == AMD_IP_BLOCK_TYPE_GMC ||
		    adev->ip_blocks[i].version->type == AMD_IP_BLOCK_TYPE_IH ||
		    (adev->ip_blocks[i].version->type == AMD_IP_BLOCK_TYPE_PSP && amdgpu_sriov_vf(adev))) {

			r = adev->ip_blocks[i].version->funcs->resume(adev);
			if (r) {
				DRM_ERROR("resume of IP block <%s> failed %d\n",
					  adev->ip_blocks[i].version->funcs->name, r);
				return r;
			}
			adev->ip_blocks[i].status.hw = true;
		}
	}

	return 0;
}

 
static int amdgpu_device_ip_resume_phase2(struct amdgpu_device *adev)
{
	int i, r;

	for (i = 0; i < adev->num_ip_blocks; i++) {
		if (!adev->ip_blocks[i].status.valid || adev->ip_blocks[i].status.hw)
			continue;
		if (adev->ip_blocks[i].version->type == AMD_IP_BLOCK_TYPE_COMMON ||
		    adev->ip_blocks[i].version->type == AMD_IP_BLOCK_TYPE_GMC ||
		    adev->ip_blocks[i].version->type == AMD_IP_BLOCK_TYPE_IH ||
		    adev->ip_blocks[i].version->type == AMD_IP_BLOCK_TYPE_PSP)
			continue;
		r = adev->ip_blocks[i].version->funcs->resume(adev);
		if (r) {
			DRM_ERROR("resume of IP block <%s> failed %d\n",
				  adev->ip_blocks[i].version->funcs->name, r);
			return r;
		}
		adev->ip_blocks[i].status.hw = true;
	}

	return 0;
}

 
static int amdgpu_device_ip_resume(struct amdgpu_device *adev)
{
	int r;

	r = amdgpu_device_ip_resume_phase1(adev);
	if (r)
		return r;

	r = amdgpu_device_fw_loading(adev);
	if (r)
		return r;

	r = amdgpu_device_ip_resume_phase2(adev);

	return r;
}

 
static void amdgpu_device_detect_sriov_bios(struct amdgpu_device *adev)
{
	if (amdgpu_sriov_vf(adev)) {
		if (adev->is_atom_fw) {
			if (amdgpu_atomfirmware_gpu_virtualization_supported(adev))
				adev->virt.caps |= AMDGPU_SRIOV_CAPS_SRIOV_VBIOS;
		} else {
			if (amdgpu_atombios_has_gpu_virtualization_table(adev))
				adev->virt.caps |= AMDGPU_SRIOV_CAPS_SRIOV_VBIOS;
		}

		if (!(adev->virt.caps & AMDGPU_SRIOV_CAPS_SRIOV_VBIOS))
			amdgpu_vf_error_put(adev, AMDGIM_ERROR_VF_NO_VBIOS, 0, 0);
	}
}

 
bool amdgpu_device_asic_has_dc_support(enum amd_asic_type asic_type)
{
	switch (asic_type) {
#ifdef CONFIG_DRM_AMDGPU_SI
	case CHIP_HAINAN:
#endif
	case CHIP_TOPAZ:
		 
		return false;
#if defined(CONFIG_DRM_AMD_DC)
	case CHIP_TAHITI:
	case CHIP_PITCAIRN:
	case CHIP_VERDE:
	case CHIP_OLAND:
		 
#if defined(CONFIG_DRM_AMD_DC_SI)
		return amdgpu_dc > 0;
#else
		return false;
#endif
	case CHIP_BONAIRE:
	case CHIP_KAVERI:
	case CHIP_KABINI:
	case CHIP_MULLINS:
		 
		return amdgpu_dc > 0;
	default:
		return amdgpu_dc != 0;
#else
	default:
		if (amdgpu_dc > 0)
			DRM_INFO_ONCE("Display Core has been requested via kernel parameter but isn't supported by ASIC, ignoring\n");
		return false;
#endif
	}
}

 
bool amdgpu_device_has_dc_support(struct amdgpu_device *adev)
{
	if (adev->enable_virtual_display ||
	    (adev->harvest_ip_mask & AMD_HARVEST_IP_DMU_MASK))
		return false;

	return amdgpu_device_asic_has_dc_support(adev->asic_type);
}

static void amdgpu_device_xgmi_reset_func(struct work_struct *__work)
{
	struct amdgpu_device *adev =
		container_of(__work, struct amdgpu_device, xgmi_reset_work);
	struct amdgpu_hive_info *hive = amdgpu_get_xgmi_hive(adev);

	 
	if (WARN_ON(!hive))
		return;

	 
	if (amdgpu_asic_reset_method(adev) == AMD_RESET_METHOD_BACO) {

		task_barrier_enter(&hive->tb);
		adev->asic_reset_res = amdgpu_device_baco_enter(adev_to_drm(adev));

		if (adev->asic_reset_res)
			goto fail;

		task_barrier_exit(&hive->tb);
		adev->asic_reset_res = amdgpu_device_baco_exit(adev_to_drm(adev));

		if (adev->asic_reset_res)
			goto fail;

		if (adev->mmhub.ras && adev->mmhub.ras->ras_block.hw_ops &&
		    adev->mmhub.ras->ras_block.hw_ops->reset_ras_error_count)
			adev->mmhub.ras->ras_block.hw_ops->reset_ras_error_count(adev);
	} else {

		task_barrier_full(&hive->tb);
		adev->asic_reset_res =  amdgpu_asic_reset(adev);
	}

fail:
	if (adev->asic_reset_res)
		DRM_WARN("ASIC reset failed with error, %d for drm dev, %s",
			 adev->asic_reset_res, adev_to_drm(adev)->unique);
	amdgpu_put_xgmi_hive(hive);
}

static int amdgpu_device_get_job_timeout_settings(struct amdgpu_device *adev)
{
	char *input = amdgpu_lockup_timeout;
	char *timeout_setting = NULL;
	int index = 0;
	long timeout;
	int ret = 0;

	 
	adev->gfx_timeout = msecs_to_jiffies(10000);
	adev->sdma_timeout = adev->video_timeout = adev->gfx_timeout;
	if (amdgpu_sriov_vf(adev))
		adev->compute_timeout = amdgpu_sriov_is_pp_one_vf(adev) ?
					msecs_to_jiffies(60000) : msecs_to_jiffies(10000);
	else
		adev->compute_timeout =  msecs_to_jiffies(60000);

	if (strnlen(input, AMDGPU_MAX_TIMEOUT_PARAM_LENGTH)) {
		while ((timeout_setting = strsep(&input, ",")) &&
				strnlen(timeout_setting, AMDGPU_MAX_TIMEOUT_PARAM_LENGTH)) {
			ret = kstrtol(timeout_setting, 0, &timeout);
			if (ret)
				return ret;

			if (timeout == 0) {
				index++;
				continue;
			} else if (timeout < 0) {
				timeout = MAX_SCHEDULE_TIMEOUT;
				dev_warn(adev->dev, "lockup timeout disabled");
				add_taint(TAINT_SOFTLOCKUP, LOCKDEP_STILL_OK);
			} else {
				timeout = msecs_to_jiffies(timeout);
			}

			switch (index++) {
			case 0:
				adev->gfx_timeout = timeout;
				break;
			case 1:
				adev->compute_timeout = timeout;
				break;
			case 2:
				adev->sdma_timeout = timeout;
				break;
			case 3:
				adev->video_timeout = timeout;
				break;
			default:
				break;
			}
		}
		 
		if (index == 1) {
			adev->sdma_timeout = adev->video_timeout = adev->gfx_timeout;
			if (amdgpu_sriov_vf(adev) || amdgpu_passthrough(adev))
				adev->compute_timeout = adev->gfx_timeout;
		}
	}

	return ret;
}

 
static void amdgpu_device_check_iommu_direct_map(struct amdgpu_device *adev)
{
	struct iommu_domain *domain;

	domain = iommu_get_domain_for_dev(adev->dev);
	if (!domain || domain->type == IOMMU_DOMAIN_IDENTITY)
		adev->ram_is_direct_mapped = true;
}

static const struct attribute *amdgpu_dev_attributes[] = {
	&dev_attr_pcie_replay_count.attr,
	NULL
};

static void amdgpu_device_set_mcbp(struct amdgpu_device *adev)
{
	if (amdgpu_mcbp == 1)
		adev->gfx.mcbp = true;
	else if (amdgpu_mcbp == 0)
		adev->gfx.mcbp = false;

	if (amdgpu_sriov_vf(adev))
		adev->gfx.mcbp = true;

	if (adev->gfx.mcbp)
		DRM_INFO("MCBP is enabled\n");
}

 
int amdgpu_device_init(struct amdgpu_device *adev,
		       uint32_t flags)
{
	struct drm_device *ddev = adev_to_drm(adev);
	struct pci_dev *pdev = adev->pdev;
	int r, i;
	bool px = false;
	u32 max_MBps;
	int tmp;

	adev->shutdown = false;
	adev->flags = flags;

	if (amdgpu_force_asic_type >= 0 && amdgpu_force_asic_type < CHIP_LAST)
		adev->asic_type = amdgpu_force_asic_type;
	else
		adev->asic_type = flags & AMD_ASIC_MASK;

	adev->usec_timeout = AMDGPU_MAX_USEC_TIMEOUT;
	if (amdgpu_emu_mode == 1)
		adev->usec_timeout *= 10;
	adev->gmc.gart_size = 512 * 1024 * 1024;
	adev->accel_working = false;
	adev->num_rings = 0;
	RCU_INIT_POINTER(adev->gang_submit, dma_fence_get_stub());
	adev->mman.buffer_funcs = NULL;
	adev->mman.buffer_funcs_ring = NULL;
	adev->vm_manager.vm_pte_funcs = NULL;
	adev->vm_manager.vm_pte_num_scheds = 0;
	adev->gmc.gmc_funcs = NULL;
	adev->harvest_ip_mask = 0x0;
	adev->fence_context = dma_fence_context_alloc(AMDGPU_MAX_RINGS);
	bitmap_zero(adev->gfx.pipe_reserve_bitmap, AMDGPU_MAX_COMPUTE_QUEUES);

	adev->smc_rreg = &amdgpu_invalid_rreg;
	adev->smc_wreg = &amdgpu_invalid_wreg;
	adev->pcie_rreg = &amdgpu_invalid_rreg;
	adev->pcie_wreg = &amdgpu_invalid_wreg;
	adev->pcie_rreg_ext = &amdgpu_invalid_rreg_ext;
	adev->pcie_wreg_ext = &amdgpu_invalid_wreg_ext;
	adev->pciep_rreg = &amdgpu_invalid_rreg;
	adev->pciep_wreg = &amdgpu_invalid_wreg;
	adev->pcie_rreg64 = &amdgpu_invalid_rreg64;
	adev->pcie_wreg64 = &amdgpu_invalid_wreg64;
	adev->uvd_ctx_rreg = &amdgpu_invalid_rreg;
	adev->uvd_ctx_wreg = &amdgpu_invalid_wreg;
	adev->didt_rreg = &amdgpu_invalid_rreg;
	adev->didt_wreg = &amdgpu_invalid_wreg;
	adev->gc_cac_rreg = &amdgpu_invalid_rreg;
	adev->gc_cac_wreg = &amdgpu_invalid_wreg;
	adev->audio_endpt_rreg = &amdgpu_block_invalid_rreg;
	adev->audio_endpt_wreg = &amdgpu_block_invalid_wreg;

	DRM_INFO("initializing kernel modesetting (%s 0x%04X:0x%04X 0x%04X:0x%04X 0x%02X).\n",
		 amdgpu_asic_name[adev->asic_type], pdev->vendor, pdev->device,
		 pdev->subsystem_vendor, pdev->subsystem_device, pdev->revision);

	 
	mutex_init(&adev->firmware.mutex);
	mutex_init(&adev->pm.mutex);
	mutex_init(&adev->gfx.gpu_clock_mutex);
	mutex_init(&adev->srbm_mutex);
	mutex_init(&adev->gfx.pipe_reserve_mutex);
	mutex_init(&adev->gfx.gfx_off_mutex);
	mutex_init(&adev->gfx.partition_mutex);
	mutex_init(&adev->grbm_idx_mutex);
	mutex_init(&adev->mn_lock);
	mutex_init(&adev->virt.vf_errors.lock);
	hash_init(adev->mn_hash);
	mutex_init(&adev->psp.mutex);
	mutex_init(&adev->notifier_lock);
	mutex_init(&adev->pm.stable_pstate_ctx_lock);
	mutex_init(&adev->benchmark_mutex);

	amdgpu_device_init_apu_flags(adev);

	r = amdgpu_device_check_arguments(adev);
	if (r)
		return r;

	spin_lock_init(&adev->mmio_idx_lock);
	spin_lock_init(&adev->smc_idx_lock);
	spin_lock_init(&adev->pcie_idx_lock);
	spin_lock_init(&adev->uvd_ctx_idx_lock);
	spin_lock_init(&adev->didt_idx_lock);
	spin_lock_init(&adev->gc_cac_idx_lock);
	spin_lock_init(&adev->se_cac_idx_lock);
	spin_lock_init(&adev->audio_endpt_idx_lock);
	spin_lock_init(&adev->mm_stats.lock);

	INIT_LIST_HEAD(&adev->shadow_list);
	mutex_init(&adev->shadow_list_lock);

	INIT_LIST_HEAD(&adev->reset_list);

	INIT_LIST_HEAD(&adev->ras_list);

	INIT_DELAYED_WORK(&adev->delayed_init_work,
			  amdgpu_device_delayed_init_work_handler);
	INIT_DELAYED_WORK(&adev->gfx.gfx_off_delay_work,
			  amdgpu_device_delay_enable_gfx_off);

	INIT_WORK(&adev->xgmi_reset_work, amdgpu_device_xgmi_reset_func);

	adev->gfx.gfx_off_req_count = 1;
	adev->gfx.gfx_off_residency = 0;
	adev->gfx.gfx_off_entrycount = 0;
	adev->pm.ac_power = power_supply_is_system_supplied() > 0;

	atomic_set(&adev->throttling_logging_enabled, 1);
	 
	ratelimit_state_init(&adev->throttling_logging_rs, (60 - 1) * HZ, 1);
	ratelimit_set_flags(&adev->throttling_logging_rs, RATELIMIT_MSG_ON_RELEASE);

	 
	 
	if (adev->asic_type >= CHIP_BONAIRE) {
		adev->rmmio_base = pci_resource_start(adev->pdev, 5);
		adev->rmmio_size = pci_resource_len(adev->pdev, 5);
	} else {
		adev->rmmio_base = pci_resource_start(adev->pdev, 2);
		adev->rmmio_size = pci_resource_len(adev->pdev, 2);
	}

	for (i = 0; i < AMD_IP_BLOCK_TYPE_NUM; i++)
		atomic_set(&adev->pm.pwr_state[i], POWER_STATE_UNKNOWN);

	adev->rmmio = ioremap(adev->rmmio_base, adev->rmmio_size);
	if (!adev->rmmio)
		return -ENOMEM;

	DRM_INFO("register mmio base: 0x%08X\n", (uint32_t)adev->rmmio_base);
	DRM_INFO("register mmio size: %u\n", (unsigned int)adev->rmmio_size);

	 
	adev->reset_domain = amdgpu_reset_create_reset_domain(SINGLE_DEVICE, "amdgpu-reset-dev");
	if (!adev->reset_domain)
		return -ENOMEM;

	 
	amdgpu_detect_virtualization(adev);

	amdgpu_device_get_pcie_info(adev);

	r = amdgpu_device_get_job_timeout_settings(adev);
	if (r) {
		dev_err(adev->dev, "invalid lockup_timeout parameter syntax\n");
		return r;
	}

	 
	r = amdgpu_device_ip_early_init(adev);
	if (r)
		return r;

	amdgpu_device_set_mcbp(adev);

	 
	r = drm_aperture_remove_conflicting_pci_framebuffers(adev->pdev, &amdgpu_kms_driver);
	if (r)
		return r;

	 
	amdgpu_gmc_tmz_set(adev);

	amdgpu_gmc_noretry_set(adev);
	 
	if (adev->gmc.xgmi.supported) {
		r = adev->gfxhub.funcs->get_xgmi_info(adev);
		if (r)
			return r;
	}

	 
	if (amdgpu_sriov_vf(adev)) {
		if (adev->virt.fw_reserve.p_pf2vf)
			adev->have_atomics_support = ((struct amd_sriov_msg_pf2vf_info *)
						      adev->virt.fw_reserve.p_pf2vf)->pcie_atomic_ops_support_flags ==
				(PCI_EXP_DEVCAP2_ATOMIC_COMP32 | PCI_EXP_DEVCAP2_ATOMIC_COMP64);
	 
	} else if ((adev->flags & AMD_IS_APU) &&
		   (adev->ip_versions[GC_HWIP][0] > IP_VERSION(9, 0, 0))) {
		adev->have_atomics_support = true;
	} else {
		adev->have_atomics_support =
			!pci_enable_atomic_ops_to_root(adev->pdev,
					  PCI_EXP_DEVCAP2_ATOMIC_COMP32 |
					  PCI_EXP_DEVCAP2_ATOMIC_COMP64);
	}

	if (!adev->have_atomics_support)
		dev_info(adev->dev, "PCIE atomic ops is not supported\n");

	 
	amdgpu_doorbell_init(adev);

	if (amdgpu_emu_mode == 1) {
		 
		emu_soc_asic_init(adev);
		goto fence_driver_init;
	}

	amdgpu_reset_init(adev);

	 
	if (adev->bios)
		amdgpu_device_detect_sriov_bios(adev);

	 
	if (!amdgpu_sriov_vf(adev) && amdgpu_asic_need_reset_on_init(adev)) {
		if (adev->gmc.xgmi.num_physical_nodes) {
			dev_info(adev->dev, "Pending hive reset.\n");
			adev->gmc.xgmi.pending_reset = true;
			 
			for (i = 0; i < adev->num_ip_blocks; i++) {
				if (!adev->ip_blocks[i].status.valid)
					continue;
				if (!(adev->ip_blocks[i].version->type == AMD_IP_BLOCK_TYPE_GMC ||
				      adev->ip_blocks[i].version->type == AMD_IP_BLOCK_TYPE_COMMON ||
				      adev->ip_blocks[i].version->type == AMD_IP_BLOCK_TYPE_IH ||
				      adev->ip_blocks[i].version->type == AMD_IP_BLOCK_TYPE_SMC)) {
					DRM_DEBUG("IP %s disabled for hw_init.\n",
						adev->ip_blocks[i].version->funcs->name);
					adev->ip_blocks[i].status.hw = true;
				}
			}
		} else {
			tmp = amdgpu_reset_method;
			 
			amdgpu_reset_method = AMD_RESET_METHOD_NONE;
			r = amdgpu_asic_reset(adev);
			amdgpu_reset_method = tmp;
			if (r) {
				dev_err(adev->dev, "asic reset on init failed\n");
				goto failed;
			}
		}
	}

	 
	if (amdgpu_device_need_post(adev)) {
		if (!adev->bios) {
			dev_err(adev->dev, "no vBIOS found\n");
			r = -EINVAL;
			goto failed;
		}
		DRM_INFO("GPU posting now...\n");
		r = amdgpu_device_asic_init(adev);
		if (r) {
			dev_err(adev->dev, "gpu post error!\n");
			goto failed;
		}
	}

	if (adev->bios) {
		if (adev->is_atom_fw) {
			 
			r = amdgpu_atomfirmware_get_clock_info(adev);
			if (r) {
				dev_err(adev->dev, "amdgpu_atomfirmware_get_clock_info failed\n");
				amdgpu_vf_error_put(adev, AMDGIM_ERROR_VF_ATOMBIOS_GET_CLOCK_FAIL, 0, 0);
				goto failed;
			}
		} else {
			 
			r = amdgpu_atombios_get_clock_info(adev);
			if (r) {
				dev_err(adev->dev, "amdgpu_atombios_get_clock_info failed\n");
				amdgpu_vf_error_put(adev, AMDGIM_ERROR_VF_ATOMBIOS_GET_CLOCK_FAIL, 0, 0);
				goto failed;
			}
			 
			if (!amdgpu_device_has_dc_support(adev))
				amdgpu_atombios_i2c_init(adev);
		}
	}

fence_driver_init:
	 
	r = amdgpu_fence_driver_sw_init(adev);
	if (r) {
		dev_err(adev->dev, "amdgpu_fence_driver_sw_init failed\n");
		amdgpu_vf_error_put(adev, AMDGIM_ERROR_VF_FENCE_INIT_FAIL, 0, 0);
		goto failed;
	}

	 
	drm_mode_config_init(adev_to_drm(adev));

	r = amdgpu_device_ip_init(adev);
	if (r) {
		dev_err(adev->dev, "amdgpu_device_ip_init failed\n");
		amdgpu_vf_error_put(adev, AMDGIM_ERROR_VF_AMDGPU_INIT_FAIL, 0, 0);
		goto release_ras_con;
	}

	amdgpu_fence_driver_hw_init(adev);

	dev_info(adev->dev,
		"SE %d, SH per SE %d, CU per SH %d, active_cu_number %d\n",
			adev->gfx.config.max_shader_engines,
			adev->gfx.config.max_sh_per_se,
			adev->gfx.config.max_cu_per_sh,
			adev->gfx.cu_info.number);

	adev->accel_working = true;

	amdgpu_vm_check_compute_bug(adev);

	 
	if (amdgpu_moverate >= 0)
		max_MBps = amdgpu_moverate;
	else
		max_MBps = 8;  
	 
	adev->mm_stats.log2_max_MBps = ilog2(max(1u, max_MBps));

	r = amdgpu_atombios_sysfs_init(adev);
	if (r)
		drm_err(&adev->ddev,
			"registering atombios sysfs failed (%d).\n", r);

	r = amdgpu_pm_sysfs_init(adev);
	if (r)
		DRM_ERROR("registering pm sysfs failed (%d).\n", r);

	r = amdgpu_ucode_sysfs_init(adev);
	if (r) {
		adev->ucode_sysfs_en = false;
		DRM_ERROR("Creating firmware sysfs failed (%d).\n", r);
	} else
		adev->ucode_sysfs_en = true;

	 
	amdgpu_register_gpu_instance(adev);

	 
	if (!adev->gmc.xgmi.pending_reset) {
		r = amdgpu_device_ip_late_init(adev);
		if (r) {
			dev_err(adev->dev, "amdgpu_device_ip_late_init failed\n");
			amdgpu_vf_error_put(adev, AMDGIM_ERROR_VF_AMDGPU_LATE_INIT_FAIL, 0, r);
			goto release_ras_con;
		}
		 
		amdgpu_ras_resume(adev);
		queue_delayed_work(system_wq, &adev->delayed_init_work,
				   msecs_to_jiffies(AMDGPU_RESUME_MS));
	}

	if (amdgpu_sriov_vf(adev)) {
		amdgpu_virt_release_full_gpu(adev, true);
		flush_delayed_work(&adev->delayed_init_work);
	}

	r = sysfs_create_files(&adev->dev->kobj, amdgpu_dev_attributes);
	if (r)
		dev_err(adev->dev, "Could not create amdgpu device attr\n");

	amdgpu_fru_sysfs_init(adev);

	if (IS_ENABLED(CONFIG_PERF_EVENTS))
		r = amdgpu_pmu_init(adev);
	if (r)
		dev_err(adev->dev, "amdgpu_pmu_init failed\n");

	 
	if (amdgpu_device_cache_pci_state(adev->pdev))
		pci_restore_state(pdev);

	 
	 
	if ((adev->pdev->class >> 8) == PCI_CLASS_DISPLAY_VGA)
		vga_client_register(adev->pdev, amdgpu_device_vga_set_decode);

	px = amdgpu_device_supports_px(ddev);

	if (px || (!dev_is_removable(&adev->pdev->dev) &&
				apple_gmux_detect(NULL, NULL)))
		vga_switcheroo_register_client(adev->pdev,
					       &amdgpu_switcheroo_ops, px);

	if (px)
		vga_switcheroo_init_domain_pm_ops(adev->dev, &adev->vga_pm_domain);

	if (adev->gmc.xgmi.pending_reset)
		queue_delayed_work(system_wq, &mgpu_info.delayed_reset_work,
				   msecs_to_jiffies(AMDGPU_RESUME_MS));

	amdgpu_device_check_iommu_direct_map(adev);

	return 0;

release_ras_con:
	if (amdgpu_sriov_vf(adev))
		amdgpu_virt_release_full_gpu(adev, true);

	 
	if (amdgpu_sriov_vf(adev) &&
		!amdgpu_sriov_runtime(adev) &&
		amdgpu_virt_mmio_blocked(adev) &&
		!amdgpu_virt_wait_reset(adev)) {
		dev_err(adev->dev, "VF exclusive mode timeout\n");
		 
		adev->virt.caps &= ~AMDGPU_SRIOV_CAPS_RUNTIME;
		adev->virt.ops = NULL;
		r = -EAGAIN;
	}
	amdgpu_release_ras_context(adev);

failed:
	amdgpu_vf_error_trans_all(adev);

	return r;
}

static void amdgpu_device_unmap_mmio(struct amdgpu_device *adev)
{

	 
	unmap_mapping_range(adev->ddev.anon_inode->i_mapping, 0, 0, 1);

	 
	amdgpu_doorbell_fini(adev);

	iounmap(adev->rmmio);
	adev->rmmio = NULL;
	if (adev->mman.aper_base_kaddr)
		iounmap(adev->mman.aper_base_kaddr);
	adev->mman.aper_base_kaddr = NULL;

	 
	if (!adev->gmc.xgmi.connected_to_cpu && !adev->gmc.is_app_apu) {
		arch_phys_wc_del(adev->gmc.vram_mtrr);
		arch_io_free_memtype_wc(adev->gmc.aper_base, adev->gmc.aper_size);
	}
}

 
void amdgpu_device_fini_hw(struct amdgpu_device *adev)
{
	dev_info(adev->dev, "amdgpu: finishing device.\n");
	flush_delayed_work(&adev->delayed_init_work);
	adev->shutdown = true;

	 
	if (amdgpu_sriov_vf(adev)) {
		amdgpu_virt_request_full_gpu(adev, false);
		amdgpu_virt_fini_data_exchange(adev);
	}

	 
	amdgpu_irq_disable_all(adev);
	if (adev->mode_info.mode_config_initialized) {
		if (!drm_drv_uses_atomic_modeset(adev_to_drm(adev)))
			drm_helper_force_disable_all(adev_to_drm(adev));
		else
			drm_atomic_helper_shutdown(adev_to_drm(adev));
	}
	amdgpu_fence_driver_hw_fini(adev);

	if (adev->mman.initialized)
		drain_workqueue(adev->mman.bdev.wq);

	if (adev->pm.sysfs_initialized)
		amdgpu_pm_sysfs_fini(adev);
	if (adev->ucode_sysfs_en)
		amdgpu_ucode_sysfs_fini(adev);
	sysfs_remove_files(&adev->dev->kobj, amdgpu_dev_attributes);
	amdgpu_fru_sysfs_fini(adev);

	 
	amdgpu_ras_pre_fini(adev);

	amdgpu_device_ip_fini_early(adev);

	amdgpu_irq_fini_hw(adev);

	if (adev->mman.initialized)
		ttm_device_clear_dma_mappings(&adev->mman.bdev);

	amdgpu_gart_dummy_page_fini(adev);

	if (drm_dev_is_unplugged(adev_to_drm(adev)))
		amdgpu_device_unmap_mmio(adev);

}

void amdgpu_device_fini_sw(struct amdgpu_device *adev)
{
	int idx;
	bool px;

	amdgpu_fence_driver_sw_fini(adev);
	amdgpu_device_ip_fini(adev);
	amdgpu_ucode_release(&adev->firmware.gpu_info_fw);
	adev->accel_working = false;
	dma_fence_put(rcu_dereference_protected(adev->gang_submit, true));

	amdgpu_reset_fini(adev);

	 
	if (!amdgpu_device_has_dc_support(adev))
		amdgpu_i2c_fini(adev);

	if (amdgpu_emu_mode != 1)
		amdgpu_atombios_fini(adev);

	kfree(adev->bios);
	adev->bios = NULL;

	px = amdgpu_device_supports_px(adev_to_drm(adev));

	if (px || (!dev_is_removable(&adev->pdev->dev) &&
				apple_gmux_detect(NULL, NULL)))
		vga_switcheroo_unregister_client(adev->pdev);

	if (px)
		vga_switcheroo_fini_domain_pm_ops(adev->dev);

	if ((adev->pdev->class >> 8) == PCI_CLASS_DISPLAY_VGA)
		vga_client_unregister(adev->pdev);

	if (drm_dev_enter(adev_to_drm(adev), &idx)) {

		iounmap(adev->rmmio);
		adev->rmmio = NULL;
		amdgpu_doorbell_fini(adev);
		drm_dev_exit(idx);
	}

	if (IS_ENABLED(CONFIG_PERF_EVENTS))
		amdgpu_pmu_fini(adev);
	if (adev->mman.discovery_bin)
		amdgpu_discovery_fini(adev);

	amdgpu_reset_put_reset_domain(adev->reset_domain);
	adev->reset_domain = NULL;

	kfree(adev->pci_state);

}

 
static int amdgpu_device_evict_resources(struct amdgpu_device *adev)
{
	int ret;

	 
	if ((adev->in_s3 || adev->in_s0ix) && (adev->flags & AMD_IS_APU))
		return 0;

	ret = amdgpu_ttm_evict_resources(adev, TTM_PL_VRAM);
	if (ret)
		DRM_WARN("evicting device resources failed\n");
	return ret;
}

 
 
int amdgpu_device_suspend(struct drm_device *dev, bool fbcon)
{
	struct amdgpu_device *adev = drm_to_adev(dev);
	int r = 0;

	if (dev->switch_power_state == DRM_SWITCH_POWER_OFF)
		return 0;

	adev->in_suspend = true;

	 
	r = amdgpu_device_evict_resources(adev);
	if (r)
		return r;

	if (amdgpu_sriov_vf(adev)) {
		amdgpu_virt_fini_data_exchange(adev);
		r = amdgpu_virt_request_full_gpu(adev, false);
		if (r)
			return r;
	}

	if (amdgpu_acpi_smart_shift_update(dev, AMDGPU_SS_DEV_D3))
		DRM_WARN("smart shift update failed\n");

	if (fbcon)
		drm_fb_helper_set_suspend_unlocked(adev_to_drm(adev)->fb_helper, true);

	cancel_delayed_work_sync(&adev->delayed_init_work);
	flush_delayed_work(&adev->gfx.gfx_off_delay_work);

	amdgpu_ras_suspend(adev);

	amdgpu_device_ip_suspend_phase1(adev);

	if (!adev->in_s0ix)
		amdgpu_amdkfd_suspend(adev, adev->in_runpm);

	r = amdgpu_device_evict_resources(adev);
	if (r)
		return r;

	amdgpu_fence_driver_hw_fini(adev);

	amdgpu_device_ip_suspend_phase2(adev);

	if (amdgpu_sriov_vf(adev))
		amdgpu_virt_release_full_gpu(adev, false);

	return 0;
}

 
int amdgpu_device_resume(struct drm_device *dev, bool fbcon)
{
	struct amdgpu_device *adev = drm_to_adev(dev);
	int r = 0;

	if (amdgpu_sriov_vf(adev)) {
		r = amdgpu_virt_request_full_gpu(adev, true);
		if (r)
			return r;
	}

	if (dev->switch_power_state == DRM_SWITCH_POWER_OFF)
		return 0;

	if (adev->in_s0ix)
		amdgpu_dpm_gfx_state_change(adev, sGpuChangeState_D0Entry);

	 
	if (amdgpu_device_need_post(adev)) {
		r = amdgpu_device_asic_init(adev);
		if (r)
			dev_err(adev->dev, "amdgpu asic init failed\n");
	}

	r = amdgpu_device_ip_resume(adev);

	if (r) {
		dev_err(adev->dev, "amdgpu_device_ip_resume failed (%d).\n", r);
		goto exit;
	}
	amdgpu_fence_driver_hw_init(adev);

	r = amdgpu_device_ip_late_init(adev);
	if (r)
		goto exit;

	queue_delayed_work(system_wq, &adev->delayed_init_work,
			   msecs_to_jiffies(AMDGPU_RESUME_MS));

	if (!adev->in_s0ix) {
		r = amdgpu_amdkfd_resume(adev, adev->in_runpm);
		if (r)
			goto exit;
	}

exit:
	if (amdgpu_sriov_vf(adev)) {
		amdgpu_virt_init_data_exchange(adev);
		amdgpu_virt_release_full_gpu(adev, true);
	}

	if (r)
		return r;

	 
	flush_delayed_work(&adev->delayed_init_work);

	if (fbcon)
		drm_fb_helper_set_suspend_unlocked(adev_to_drm(adev)->fb_helper, false);

	amdgpu_ras_resume(adev);

	if (adev->mode_info.num_crtc) {
		 
#ifdef CONFIG_PM
		dev->dev->power.disable_depth++;
#endif
		if (!adev->dc_enabled)
			drm_helper_hpd_irq_event(dev);
		else
			drm_kms_helper_hotplug_event(dev);
#ifdef CONFIG_PM
		dev->dev->power.disable_depth--;
#endif
	}
	adev->in_suspend = false;

	if (adev->enable_mes)
		amdgpu_mes_self_test(adev);

	if (amdgpu_acpi_smart_shift_update(dev, AMDGPU_SS_DEV_D0))
		DRM_WARN("smart shift update failed\n");

	return 0;
}

 
static bool amdgpu_device_ip_check_soft_reset(struct amdgpu_device *adev)
{
	int i;
	bool asic_hang = false;

	if (amdgpu_sriov_vf(adev))
		return true;

	if (amdgpu_asic_need_full_reset(adev))
		return true;

	for (i = 0; i < adev->num_ip_blocks; i++) {
		if (!adev->ip_blocks[i].status.valid)
			continue;
		if (adev->ip_blocks[i].version->funcs->check_soft_reset)
			adev->ip_blocks[i].status.hang =
				adev->ip_blocks[i].version->funcs->check_soft_reset(adev);
		if (adev->ip_blocks[i].status.hang) {
			dev_info(adev->dev, "IP block:%s is hung!\n", adev->ip_blocks[i].version->funcs->name);
			asic_hang = true;
		}
	}
	return asic_hang;
}

 
static int amdgpu_device_ip_pre_soft_reset(struct amdgpu_device *adev)
{
	int i, r = 0;

	for (i = 0; i < adev->num_ip_blocks; i++) {
		if (!adev->ip_blocks[i].status.valid)
			continue;
		if (adev->ip_blocks[i].status.hang &&
		    adev->ip_blocks[i].version->funcs->pre_soft_reset) {
			r = adev->ip_blocks[i].version->funcs->pre_soft_reset(adev);
			if (r)
				return r;
		}
	}

	return 0;
}

 
static bool amdgpu_device_ip_need_full_reset(struct amdgpu_device *adev)
{
	int i;

	if (amdgpu_asic_need_full_reset(adev))
		return true;

	for (i = 0; i < adev->num_ip_blocks; i++) {
		if (!adev->ip_blocks[i].status.valid)
			continue;
		if ((adev->ip_blocks[i].version->type == AMD_IP_BLOCK_TYPE_GMC) ||
		    (adev->ip_blocks[i].version->type == AMD_IP_BLOCK_TYPE_SMC) ||
		    (adev->ip_blocks[i].version->type == AMD_IP_BLOCK_TYPE_ACP) ||
		    (adev->ip_blocks[i].version->type == AMD_IP_BLOCK_TYPE_DCE) ||
		     adev->ip_blocks[i].version->type == AMD_IP_BLOCK_TYPE_PSP) {
			if (adev->ip_blocks[i].status.hang) {
				dev_info(adev->dev, "Some block need full reset!\n");
				return true;
			}
		}
	}
	return false;
}

 
static int amdgpu_device_ip_soft_reset(struct amdgpu_device *adev)
{
	int i, r = 0;

	for (i = 0; i < adev->num_ip_blocks; i++) {
		if (!adev->ip_blocks[i].status.valid)
			continue;
		if (adev->ip_blocks[i].status.hang &&
		    adev->ip_blocks[i].version->funcs->soft_reset) {
			r = adev->ip_blocks[i].version->funcs->soft_reset(adev);
			if (r)
				return r;
		}
	}

	return 0;
}

 
static int amdgpu_device_ip_post_soft_reset(struct amdgpu_device *adev)
{
	int i, r = 0;

	for (i = 0; i < adev->num_ip_blocks; i++) {
		if (!adev->ip_blocks[i].status.valid)
			continue;
		if (adev->ip_blocks[i].status.hang &&
		    adev->ip_blocks[i].version->funcs->post_soft_reset)
			r = adev->ip_blocks[i].version->funcs->post_soft_reset(adev);
		if (r)
			return r;
	}

	return 0;
}

 
static int amdgpu_device_recover_vram(struct amdgpu_device *adev)
{
	struct dma_fence *fence = NULL, *next = NULL;
	struct amdgpu_bo *shadow;
	struct amdgpu_bo_vm *vmbo;
	long r = 1, tmo;

	if (amdgpu_sriov_runtime(adev))
		tmo = msecs_to_jiffies(8000);
	else
		tmo = msecs_to_jiffies(100);

	dev_info(adev->dev, "recover vram bo from shadow start\n");
	mutex_lock(&adev->shadow_list_lock);
	list_for_each_entry(vmbo, &adev->shadow_list, shadow_list) {
		 
		if (!vmbo->shadow)
			continue;
		shadow = vmbo->shadow;

		 
		if (shadow->tbo.resource->mem_type != TTM_PL_TT ||
		    shadow->tbo.resource->start == AMDGPU_BO_INVALID_OFFSET ||
		    shadow->parent->tbo.resource->mem_type != TTM_PL_VRAM)
			continue;

		r = amdgpu_bo_restore_shadow(shadow, &next);
		if (r)
			break;

		if (fence) {
			tmo = dma_fence_wait_timeout(fence, false, tmo);
			dma_fence_put(fence);
			fence = next;
			if (tmo == 0) {
				r = -ETIMEDOUT;
				break;
			} else if (tmo < 0) {
				r = tmo;
				break;
			}
		} else {
			fence = next;
		}
	}
	mutex_unlock(&adev->shadow_list_lock);

	if (fence)
		tmo = dma_fence_wait_timeout(fence, false, tmo);
	dma_fence_put(fence);

	if (r < 0 || tmo <= 0) {
		dev_err(adev->dev, "recover vram bo from shadow failed, r is %ld, tmo is %ld\n", r, tmo);
		return -EIO;
	}

	dev_info(adev->dev, "recover vram bo from shadow done\n");
	return 0;
}


 
static int amdgpu_device_reset_sriov(struct amdgpu_device *adev,
				     bool from_hypervisor)
{
	int r;
	struct amdgpu_hive_info *hive = NULL;
	int retry_limit = 0;

retry:
	amdgpu_amdkfd_pre_reset(adev);

	if (from_hypervisor)
		r = amdgpu_virt_request_full_gpu(adev, true);
	else
		r = amdgpu_virt_reset_gpu(adev);
	if (r)
		return r;
	amdgpu_irq_gpu_reset_resume_helper(adev);

	 
	amdgpu_virt_post_reset(adev);

	 
	r = amdgpu_device_ip_reinit_early_sriov(adev);
	if (r)
		goto error;

	amdgpu_virt_init_data_exchange(adev);

	r = amdgpu_device_fw_loading(adev);
	if (r)
		return r;

	 
	r = amdgpu_device_ip_reinit_late_sriov(adev);
	if (r)
		goto error;

	hive = amdgpu_get_xgmi_hive(adev);
	 
	if (hive && adev->gmc.xgmi.num_physical_nodes > 1)
		r = amdgpu_xgmi_update_topology(hive, adev);

	if (hive)
		amdgpu_put_xgmi_hive(hive);

	if (!r) {
		r = amdgpu_ib_ring_tests(adev);

		amdgpu_amdkfd_post_reset(adev);
	}

error:
	if (!r && adev->virt.gim_feature & AMDGIM_FEATURE_GIM_FLR_VRAMLOST) {
		amdgpu_inc_vram_lost(adev);
		r = amdgpu_device_recover_vram(adev);
	}
	amdgpu_virt_release_full_gpu(adev, true);

	if (AMDGPU_RETRY_SRIOV_RESET(r)) {
		if (retry_limit < AMDGPU_MAX_RETRY_LIMIT) {
			retry_limit++;
			goto retry;
		} else
			DRM_ERROR("GPU reset retry is beyond the retry limit\n");
	}

	return r;
}

 
bool amdgpu_device_has_job_running(struct amdgpu_device *adev)
{
	int i;
	struct drm_sched_job *job;

	for (i = 0; i < AMDGPU_MAX_RINGS; ++i) {
		struct amdgpu_ring *ring = adev->rings[i];

		if (!ring || !ring->sched.thread)
			continue;

		spin_lock(&ring->sched.job_list_lock);
		job = list_first_entry_or_null(&ring->sched.pending_list,
					       struct drm_sched_job, list);
		spin_unlock(&ring->sched.job_list_lock);
		if (job)
			return true;
	}
	return false;
}

 
bool amdgpu_device_should_recover_gpu(struct amdgpu_device *adev)
{

	if (amdgpu_gpu_recovery == 0)
		goto disabled;

	 
	if (!amdgpu_ras_is_poison_mode_supported(adev))
		return true;

	if (amdgpu_sriov_vf(adev))
		return true;

	if (amdgpu_gpu_recovery == -1) {
		switch (adev->asic_type) {
#ifdef CONFIG_DRM_AMDGPU_SI
		case CHIP_VERDE:
		case CHIP_TAHITI:
		case CHIP_PITCAIRN:
		case CHIP_OLAND:
		case CHIP_HAINAN:
#endif
#ifdef CONFIG_DRM_AMDGPU_CIK
		case CHIP_KAVERI:
		case CHIP_KABINI:
		case CHIP_MULLINS:
#endif
		case CHIP_CARRIZO:
		case CHIP_STONEY:
		case CHIP_CYAN_SKILLFISH:
			goto disabled;
		default:
			break;
		}
	}

	return true;

disabled:
		dev_info(adev->dev, "GPU recovery disabled.\n");
		return false;
}

int amdgpu_device_mode1_reset(struct amdgpu_device *adev)
{
	u32 i;
	int ret = 0;

	amdgpu_atombios_scratch_regs_engine_hung(adev, true);

	dev_info(adev->dev, "GPU mode1 reset\n");

	 
	pci_clear_master(adev->pdev);

	amdgpu_device_cache_pci_state(adev->pdev);

	if (amdgpu_dpm_is_mode1_reset_supported(adev)) {
		dev_info(adev->dev, "GPU smu mode1 reset\n");
		ret = amdgpu_dpm_mode1_reset(adev);
	} else {
		dev_info(adev->dev, "GPU psp mode1 reset\n");
		ret = psp_gpu_reset(adev);
	}

	if (ret)
		goto mode1_reset_failed;

	amdgpu_device_load_pci_state(adev->pdev);
	ret = amdgpu_psp_wait_for_bootloader(adev);
	if (ret)
		goto mode1_reset_failed;

	 
	for (i = 0; i < adev->usec_timeout; i++) {
		u32 memsize = adev->nbio.funcs->get_memsize(adev);

		if (memsize != 0xffffffff)
			break;
		udelay(1);
	}

	if (i >= adev->usec_timeout) {
		ret = -ETIMEDOUT;
		goto mode1_reset_failed;
	}

	amdgpu_atombios_scratch_regs_engine_hung(adev, false);

	return 0;

mode1_reset_failed:
	dev_err(adev->dev, "GPU mode1 reset failed\n");
	return ret;
}

int amdgpu_device_pre_asic_reset(struct amdgpu_device *adev,
				 struct amdgpu_reset_context *reset_context)
{
	int i, r = 0;
	struct amdgpu_job *job = NULL;
	bool need_full_reset =
		test_bit(AMDGPU_NEED_FULL_RESET, &reset_context->flags);

	if (reset_context->reset_req_dev == adev)
		job = reset_context->job;

	if (amdgpu_sriov_vf(adev)) {
		 
		amdgpu_virt_fini_data_exchange(adev);
	}

	amdgpu_fence_driver_isr_toggle(adev, true);

	 
	for (i = 0; i < AMDGPU_MAX_RINGS; ++i) {
		struct amdgpu_ring *ring = adev->rings[i];

		if (!ring || !ring->sched.thread)
			continue;

		 
		amdgpu_fence_driver_clear_job_fences(ring);

		 
		amdgpu_fence_driver_force_completion(ring);
	}

	amdgpu_fence_driver_isr_toggle(adev, false);

	if (job && job->vm)
		drm_sched_increase_karma(&job->base);

	r = amdgpu_reset_prepare_hwcontext(adev, reset_context);
	 
	if (r == -EOPNOTSUPP)
		r = 0;
	else
		return r;

	 
	if (!amdgpu_sriov_vf(adev)) {

		if (!need_full_reset)
			need_full_reset = amdgpu_device_ip_need_full_reset(adev);

		if (!need_full_reset && amdgpu_gpu_recovery &&
		    amdgpu_device_ip_check_soft_reset(adev)) {
			amdgpu_device_ip_pre_soft_reset(adev);
			r = amdgpu_device_ip_soft_reset(adev);
			amdgpu_device_ip_post_soft_reset(adev);
			if (r || amdgpu_device_ip_check_soft_reset(adev)) {
				dev_info(adev->dev, "soft reset failed, will fallback to full reset!\n");
				need_full_reset = true;
			}
		}

		if (need_full_reset)
			r = amdgpu_device_ip_suspend(adev);
		if (need_full_reset)
			set_bit(AMDGPU_NEED_FULL_RESET, &reset_context->flags);
		else
			clear_bit(AMDGPU_NEED_FULL_RESET,
				  &reset_context->flags);
	}

	return r;
}

static int amdgpu_reset_reg_dumps(struct amdgpu_device *adev)
{
	int i;

	lockdep_assert_held(&adev->reset_domain->sem);

	for (i = 0; i < adev->num_regs; i++) {
		adev->reset_dump_reg_value[i] = RREG32(adev->reset_dump_reg_list[i]);
		trace_amdgpu_reset_reg_dumps(adev->reset_dump_reg_list[i],
					     adev->reset_dump_reg_value[i]);
	}

	return 0;
}

#ifdef CONFIG_DEV_COREDUMP
static ssize_t amdgpu_devcoredump_read(char *buffer, loff_t offset,
		size_t count, void *data, size_t datalen)
{
	struct drm_printer p;
	struct amdgpu_device *adev = data;
	struct drm_print_iterator iter;
	int i;

	iter.data = buffer;
	iter.offset = 0;
	iter.start = offset;
	iter.remain = count;

	p = drm_coredump_printer(&iter);

	drm_printf(&p, "**** AMDGPU Device Coredump ****\n");
	drm_printf(&p, "kernel: " UTS_RELEASE "\n");
	drm_printf(&p, "module: " KBUILD_MODNAME "\n");
	drm_printf(&p, "time: %lld.%09ld\n", adev->reset_time.tv_sec, adev->reset_time.tv_nsec);
	if (adev->reset_task_info.pid)
		drm_printf(&p, "process_name: %s PID: %d\n",
			   adev->reset_task_info.process_name,
			   adev->reset_task_info.pid);

	if (adev->reset_vram_lost)
		drm_printf(&p, "VRAM is lost due to GPU reset!\n");
	if (adev->num_regs) {
		drm_printf(&p, "AMDGPU register dumps:\nOffset:     Value:\n");

		for (i = 0; i < adev->num_regs; i++)
			drm_printf(&p, "0x%08x: 0x%08x\n",
				   adev->reset_dump_reg_list[i],
				   adev->reset_dump_reg_value[i]);
	}

	return count - iter.remain;
}

static void amdgpu_devcoredump_free(void *data)
{
}

static void amdgpu_reset_capture_coredumpm(struct amdgpu_device *adev)
{
	struct drm_device *dev = adev_to_drm(adev);

	ktime_get_ts64(&adev->reset_time);
	dev_coredumpm(dev->dev, THIS_MODULE, adev, 0, GFP_NOWAIT,
		      amdgpu_devcoredump_read, amdgpu_devcoredump_free);
}
#endif

int amdgpu_do_asic_reset(struct list_head *device_list_handle,
			 struct amdgpu_reset_context *reset_context)
{
	struct amdgpu_device *tmp_adev = NULL;
	bool need_full_reset, skip_hw_reset, vram_lost = false;
	int r = 0;
	bool gpu_reset_for_dev_remove = 0;

	 
	tmp_adev = list_first_entry(device_list_handle, struct amdgpu_device,
				    reset_list);
	amdgpu_reset_reg_dumps(tmp_adev);

	reset_context->reset_device_list = device_list_handle;
	r = amdgpu_reset_perform_reset(tmp_adev, reset_context);
	 
	if (r == -EOPNOTSUPP)
		r = 0;
	else
		return r;

	 
	need_full_reset =
		test_bit(AMDGPU_NEED_FULL_RESET, &reset_context->flags);
	skip_hw_reset = test_bit(AMDGPU_SKIP_HW_RESET, &reset_context->flags);

	gpu_reset_for_dev_remove =
		test_bit(AMDGPU_RESET_FOR_DEVICE_REMOVE, &reset_context->flags) &&
			test_bit(AMDGPU_NEED_FULL_RESET, &reset_context->flags);

	 
	if (!skip_hw_reset && need_full_reset) {
		list_for_each_entry(tmp_adev, device_list_handle, reset_list) {
			 
			if (tmp_adev->gmc.xgmi.num_physical_nodes > 1) {
				tmp_adev->gmc.xgmi.pending_reset = false;
				if (!queue_work(system_unbound_wq, &tmp_adev->xgmi_reset_work))
					r = -EALREADY;
			} else
				r = amdgpu_asic_reset(tmp_adev);

			if (r) {
				dev_err(tmp_adev->dev, "ASIC reset failed with error, %d for drm dev, %s",
					 r, adev_to_drm(tmp_adev)->unique);
				break;
			}
		}

		 
		if (!r) {
			list_for_each_entry(tmp_adev, device_list_handle, reset_list) {
				if (tmp_adev->gmc.xgmi.num_physical_nodes > 1) {
					flush_work(&tmp_adev->xgmi_reset_work);
					r = tmp_adev->asic_reset_res;
					if (r)
						break;
				}
			}
		}
	}

	if (!r && amdgpu_ras_intr_triggered()) {
		list_for_each_entry(tmp_adev, device_list_handle, reset_list) {
			if (tmp_adev->mmhub.ras && tmp_adev->mmhub.ras->ras_block.hw_ops &&
			    tmp_adev->mmhub.ras->ras_block.hw_ops->reset_ras_error_count)
				tmp_adev->mmhub.ras->ras_block.hw_ops->reset_ras_error_count(tmp_adev);
		}

		amdgpu_ras_intr_cleared();
	}

	 
	if (gpu_reset_for_dev_remove) {
		list_for_each_entry(tmp_adev, device_list_handle, reset_list)
			amdgpu_device_ip_resume_phase1(tmp_adev);

		goto end;
	}

	list_for_each_entry(tmp_adev, device_list_handle, reset_list) {
		if (need_full_reset) {
			 
			r = amdgpu_device_asic_init(tmp_adev);
			if (r) {
				dev_warn(tmp_adev->dev, "asic atom init failed!");
			} else {
				dev_info(tmp_adev->dev, "GPU reset succeeded, trying to resume\n");

				r = amdgpu_device_ip_resume_phase1(tmp_adev);
				if (r)
					goto out;

				vram_lost = amdgpu_device_check_vram_lost(tmp_adev);
#ifdef CONFIG_DEV_COREDUMP
				tmp_adev->reset_vram_lost = vram_lost;
				memset(&tmp_adev->reset_task_info, 0,
						sizeof(tmp_adev->reset_task_info));
				if (reset_context->job && reset_context->job->vm)
					tmp_adev->reset_task_info =
						reset_context->job->vm->task_info;
				amdgpu_reset_capture_coredumpm(tmp_adev);
#endif
				if (vram_lost) {
					DRM_INFO("VRAM is lost due to GPU reset!\n");
					amdgpu_inc_vram_lost(tmp_adev);
				}

				r = amdgpu_device_fw_loading(tmp_adev);
				if (r)
					return r;

				r = amdgpu_device_ip_resume_phase2(tmp_adev);
				if (r)
					goto out;

				if (vram_lost)
					amdgpu_device_fill_reset_magic(tmp_adev);

				 
				amdgpu_register_gpu_instance(tmp_adev);

				if (!reset_context->hive &&
				    tmp_adev->gmc.xgmi.num_physical_nodes > 1)
					amdgpu_xgmi_add_device(tmp_adev);

				r = amdgpu_device_ip_late_init(tmp_adev);
				if (r)
					goto out;

				drm_fb_helper_set_suspend_unlocked(adev_to_drm(tmp_adev)->fb_helper, false);

				 
				if (!amdgpu_ras_eeprom_check_err_threshold(tmp_adev)) {
					 
					amdgpu_ras_resume(tmp_adev);
				} else {
					r = -EINVAL;
					goto out;
				}

				 
				if (reset_context->hive &&
				    tmp_adev->gmc.xgmi.num_physical_nodes > 1)
					r = amdgpu_xgmi_update_topology(
						reset_context->hive, tmp_adev);
			}
		}

out:
		if (!r) {
			amdgpu_irq_gpu_reset_resume_helper(tmp_adev);
			r = amdgpu_ib_ring_tests(tmp_adev);
			if (r) {
				dev_err(tmp_adev->dev, "ib ring test failed (%d).\n", r);
				need_full_reset = true;
				r = -EAGAIN;
				goto end;
			}
		}

		if (!r)
			r = amdgpu_device_recover_vram(tmp_adev);
		else
			tmp_adev->asic_reset_res = r;
	}

end:
	if (need_full_reset)
		set_bit(AMDGPU_NEED_FULL_RESET, &reset_context->flags);
	else
		clear_bit(AMDGPU_NEED_FULL_RESET, &reset_context->flags);
	return r;
}

static void amdgpu_device_set_mp1_state(struct amdgpu_device *adev)
{

	switch (amdgpu_asic_reset_method(adev)) {
	case AMD_RESET_METHOD_MODE1:
		adev->mp1_state = PP_MP1_STATE_SHUTDOWN;
		break;
	case AMD_RESET_METHOD_MODE2:
		adev->mp1_state = PP_MP1_STATE_RESET;
		break;
	default:
		adev->mp1_state = PP_MP1_STATE_NONE;
		break;
	}
}

static void amdgpu_device_unset_mp1_state(struct amdgpu_device *adev)
{
	amdgpu_vf_error_trans_all(adev);
	adev->mp1_state = PP_MP1_STATE_NONE;
}

static void amdgpu_device_resume_display_audio(struct amdgpu_device *adev)
{
	struct pci_dev *p = NULL;

	p = pci_get_domain_bus_and_slot(pci_domain_nr(adev->pdev->bus),
			adev->pdev->bus->number, 1);
	if (p) {
		pm_runtime_enable(&(p->dev));
		pm_runtime_resume(&(p->dev));
	}

	pci_dev_put(p);
}

static int amdgpu_device_suspend_display_audio(struct amdgpu_device *adev)
{
	enum amd_reset_method reset_method;
	struct pci_dev *p = NULL;
	u64 expires;

	 
	reset_method = amdgpu_asic_reset_method(adev);
	if ((reset_method != AMD_RESET_METHOD_BACO) &&
	     (reset_method != AMD_RESET_METHOD_MODE1))
		return -EINVAL;

	p = pci_get_domain_bus_and_slot(pci_domain_nr(adev->pdev->bus),
			adev->pdev->bus->number, 1);
	if (!p)
		return -ENODEV;

	expires = pm_runtime_autosuspend_expiration(&(p->dev));
	if (!expires)
		 
		expires = ktime_get_mono_fast_ns() + NSEC_PER_SEC * 4ULL;

	while (!pm_runtime_status_suspended(&(p->dev))) {
		if (!pm_runtime_suspend(&(p->dev)))
			break;

		if (expires < ktime_get_mono_fast_ns()) {
			dev_warn(adev->dev, "failed to suspend display audio\n");
			pci_dev_put(p);
			 
			return -ETIMEDOUT;
		}
	}

	pm_runtime_disable(&(p->dev));

	pci_dev_put(p);
	return 0;
}

static inline void amdgpu_device_stop_pending_resets(struct amdgpu_device *adev)
{
	struct amdgpu_ras *con = amdgpu_ras_get_context(adev);

#if defined(CONFIG_DEBUG_FS)
	if (!amdgpu_sriov_vf(adev))
		cancel_work(&adev->reset_work);
#endif

	if (adev->kfd.dev)
		cancel_work(&adev->kfd.reset_work);

	if (amdgpu_sriov_vf(adev))
		cancel_work(&adev->virt.flr_work);

	if (con && adev->ras_enabled)
		cancel_work(&con->recovery_work);

}

 

int amdgpu_device_gpu_recover(struct amdgpu_device *adev,
			      struct amdgpu_job *job,
			      struct amdgpu_reset_context *reset_context)
{
	struct list_head device_list, *device_list_handle =  NULL;
	bool job_signaled = false;
	struct amdgpu_hive_info *hive = NULL;
	struct amdgpu_device *tmp_adev = NULL;
	int i, r = 0;
	bool need_emergency_restart = false;
	bool audio_suspended = false;
	bool gpu_reset_for_dev_remove = false;

	gpu_reset_for_dev_remove =
			test_bit(AMDGPU_RESET_FOR_DEVICE_REMOVE, &reset_context->flags) &&
				test_bit(AMDGPU_NEED_FULL_RESET, &reset_context->flags);

	 
	need_emergency_restart = amdgpu_ras_need_emergency_restart(adev);

	 
	if (need_emergency_restart && amdgpu_ras_get_context(adev) &&
		amdgpu_ras_get_context(adev)->reboot) {
		DRM_WARN("Emergency reboot.");

		ksys_sync_helper();
		emergency_restart();
	}

	dev_info(adev->dev, "GPU %s begin!\n",
		need_emergency_restart ? "jobs stop":"reset");

	if (!amdgpu_sriov_vf(adev))
		hive = amdgpu_get_xgmi_hive(adev);
	if (hive)
		mutex_lock(&hive->hive_lock);

	reset_context->job = job;
	reset_context->hive = hive;
	 
	INIT_LIST_HEAD(&device_list);
	if (!amdgpu_sriov_vf(adev) && (adev->gmc.xgmi.num_physical_nodes > 1)) {
		list_for_each_entry(tmp_adev, &hive->device_list, gmc.xgmi.head) {
			list_add_tail(&tmp_adev->reset_list, &device_list);
			if (gpu_reset_for_dev_remove && adev->shutdown)
				tmp_adev->shutdown = true;
		}
		if (!list_is_first(&adev->reset_list, &device_list))
			list_rotate_to_front(&adev->reset_list, &device_list);
		device_list_handle = &device_list;
	} else {
		list_add_tail(&adev->reset_list, &device_list);
		device_list_handle = &device_list;
	}

	 
	tmp_adev = list_first_entry(device_list_handle, struct amdgpu_device,
				    reset_list);
	amdgpu_device_lock_reset_domain(tmp_adev->reset_domain);

	 
	list_for_each_entry(tmp_adev, device_list_handle, reset_list) {

		amdgpu_device_set_mp1_state(tmp_adev);

		 
		if (!amdgpu_device_suspend_display_audio(tmp_adev))
			audio_suspended = true;

		amdgpu_ras_set_error_query_ready(tmp_adev, false);

		cancel_delayed_work_sync(&tmp_adev->delayed_init_work);

		if (!amdgpu_sriov_vf(tmp_adev))
			amdgpu_amdkfd_pre_reset(tmp_adev);

		 
		amdgpu_unregister_gpu_instance(tmp_adev);

		drm_fb_helper_set_suspend_unlocked(adev_to_drm(tmp_adev)->fb_helper, true);

		 
		if (!need_emergency_restart &&
		      amdgpu_device_ip_need_full_reset(tmp_adev))
			amdgpu_ras_suspend(tmp_adev);

		for (i = 0; i < AMDGPU_MAX_RINGS; ++i) {
			struct amdgpu_ring *ring = tmp_adev->rings[i];

			if (!ring || !ring->sched.thread)
				continue;

			drm_sched_stop(&ring->sched, job ? &job->base : NULL);

			if (need_emergency_restart)
				amdgpu_job_stop_all_jobs_on_sched(&ring->sched);
		}
		atomic_inc(&tmp_adev->gpu_reset_counter);
	}

	if (need_emergency_restart)
		goto skip_sched_resume;

	 
	if (job && dma_fence_is_signaled(&job->hw_fence)) {
		job_signaled = true;
		dev_info(adev->dev, "Guilty job already signaled, skipping HW reset");
		goto skip_hw_reset;
	}

retry:	 
	list_for_each_entry(tmp_adev, device_list_handle, reset_list) {
		if (gpu_reset_for_dev_remove) {
			 
			amdgpu_device_smu_fini_early(tmp_adev);
		}
		r = amdgpu_device_pre_asic_reset(tmp_adev, reset_context);
		 
		if (r) {
			dev_err(tmp_adev->dev, "GPU pre asic reset failed with err, %d for drm dev, %s ",
				  r, adev_to_drm(tmp_adev)->unique);
			tmp_adev->asic_reset_res = r;
		}

		 
		amdgpu_device_stop_pending_resets(tmp_adev);
	}

	 
	 
	if (amdgpu_sriov_vf(adev)) {
		r = amdgpu_device_reset_sriov(adev, job ? false : true);
		if (r)
			adev->asic_reset_res = r;

		 
		if (adev->ip_versions[GC_HWIP][0] == IP_VERSION(9, 4, 2) ||
		    adev->ip_versions[GC_HWIP][0] == IP_VERSION(11, 0, 3))
			amdgpu_ras_resume(adev);
	} else {
		r = amdgpu_do_asic_reset(device_list_handle, reset_context);
		if (r && r == -EAGAIN)
			goto retry;

		if (!r && gpu_reset_for_dev_remove)
			goto recover_end;
	}

skip_hw_reset:

	 
	list_for_each_entry(tmp_adev, device_list_handle, reset_list) {

		for (i = 0; i < AMDGPU_MAX_RINGS; ++i) {
			struct amdgpu_ring *ring = tmp_adev->rings[i];

			if (!ring || !ring->sched.thread)
				continue;

			drm_sched_start(&ring->sched, true);
		}

		if (adev->enable_mes && adev->ip_versions[GC_HWIP][0] != IP_VERSION(11, 0, 3))
			amdgpu_mes_self_test(tmp_adev);

		if (!drm_drv_uses_atomic_modeset(adev_to_drm(tmp_adev)) && !job_signaled)
			drm_helper_resume_force_mode(adev_to_drm(tmp_adev));

		if (tmp_adev->asic_reset_res)
			r = tmp_adev->asic_reset_res;

		tmp_adev->asic_reset_res = 0;

		if (r) {
			 
			dev_info(tmp_adev->dev, "GPU reset(%d) failed\n", atomic_read(&tmp_adev->gpu_reset_counter));
			amdgpu_vf_error_put(tmp_adev, AMDGIM_ERROR_VF_GPU_RESET_FAIL, 0, r);
		} else {
			dev_info(tmp_adev->dev, "GPU reset(%d) succeeded!\n", atomic_read(&tmp_adev->gpu_reset_counter));
			if (amdgpu_acpi_smart_shift_update(adev_to_drm(tmp_adev), AMDGPU_SS_DEV_D0))
				DRM_WARN("smart shift update failed\n");
		}
	}

skip_sched_resume:
	list_for_each_entry(tmp_adev, device_list_handle, reset_list) {
		 
		if (!need_emergency_restart && !amdgpu_sriov_vf(tmp_adev))
			amdgpu_amdkfd_post_reset(tmp_adev);

		 
		if (!adev->kfd.init_complete)
			amdgpu_amdkfd_device_init(adev);

		if (audio_suspended)
			amdgpu_device_resume_display_audio(tmp_adev);

		amdgpu_device_unset_mp1_state(tmp_adev);

		amdgpu_ras_set_error_query_ready(tmp_adev, true);
	}

recover_end:
	tmp_adev = list_first_entry(device_list_handle, struct amdgpu_device,
					    reset_list);
	amdgpu_device_unlock_reset_domain(tmp_adev->reset_domain);

	if (hive) {
		mutex_unlock(&hive->hive_lock);
		amdgpu_put_xgmi_hive(hive);
	}

	if (r)
		dev_info(adev->dev, "GPU reset end with ret = %d\n", r);

	atomic_set(&adev->reset_domain->reset_res, r);
	return r;
}

 
static void amdgpu_device_get_pcie_info(struct amdgpu_device *adev)
{
	struct pci_dev *pdev;
	enum pci_bus_speed speed_cap, platform_speed_cap;
	enum pcie_link_width platform_link_width;

	if (amdgpu_pcie_gen_cap)
		adev->pm.pcie_gen_mask = amdgpu_pcie_gen_cap;

	if (amdgpu_pcie_lane_cap)
		adev->pm.pcie_mlw_mask = amdgpu_pcie_lane_cap;

	 
	if (pci_is_root_bus(adev->pdev->bus) && !amdgpu_passthrough(adev)) {
		if (adev->pm.pcie_gen_mask == 0)
			adev->pm.pcie_gen_mask = AMDGPU_DEFAULT_PCIE_GEN_MASK;
		if (adev->pm.pcie_mlw_mask == 0)
			adev->pm.pcie_mlw_mask = AMDGPU_DEFAULT_PCIE_MLW_MASK;
		return;
	}

	if (adev->pm.pcie_gen_mask && adev->pm.pcie_mlw_mask)
		return;

	pcie_bandwidth_available(adev->pdev, NULL,
				 &platform_speed_cap, &platform_link_width);

	if (adev->pm.pcie_gen_mask == 0) {
		 
		pdev = adev->pdev;
		speed_cap = pcie_get_speed_cap(pdev);
		if (speed_cap == PCI_SPEED_UNKNOWN) {
			adev->pm.pcie_gen_mask |= (CAIL_ASIC_PCIE_LINK_SPEED_SUPPORT_GEN1 |
						  CAIL_ASIC_PCIE_LINK_SPEED_SUPPORT_GEN2 |
						  CAIL_ASIC_PCIE_LINK_SPEED_SUPPORT_GEN3);
		} else {
			if (speed_cap == PCIE_SPEED_32_0GT)
				adev->pm.pcie_gen_mask |= (CAIL_ASIC_PCIE_LINK_SPEED_SUPPORT_GEN1 |
							  CAIL_ASIC_PCIE_LINK_SPEED_SUPPORT_GEN2 |
							  CAIL_ASIC_PCIE_LINK_SPEED_SUPPORT_GEN3 |
							  CAIL_ASIC_PCIE_LINK_SPEED_SUPPORT_GEN4 |
							  CAIL_ASIC_PCIE_LINK_SPEED_SUPPORT_GEN5);
			else if (speed_cap == PCIE_SPEED_16_0GT)
				adev->pm.pcie_gen_mask |= (CAIL_ASIC_PCIE_LINK_SPEED_SUPPORT_GEN1 |
							  CAIL_ASIC_PCIE_LINK_SPEED_SUPPORT_GEN2 |
							  CAIL_ASIC_PCIE_LINK_SPEED_SUPPORT_GEN3 |
							  CAIL_ASIC_PCIE_LINK_SPEED_SUPPORT_GEN4);
			else if (speed_cap == PCIE_SPEED_8_0GT)
				adev->pm.pcie_gen_mask |= (CAIL_ASIC_PCIE_LINK_SPEED_SUPPORT_GEN1 |
							  CAIL_ASIC_PCIE_LINK_SPEED_SUPPORT_GEN2 |
							  CAIL_ASIC_PCIE_LINK_SPEED_SUPPORT_GEN3);
			else if (speed_cap == PCIE_SPEED_5_0GT)
				adev->pm.pcie_gen_mask |= (CAIL_ASIC_PCIE_LINK_SPEED_SUPPORT_GEN1 |
							  CAIL_ASIC_PCIE_LINK_SPEED_SUPPORT_GEN2);
			else
				adev->pm.pcie_gen_mask |= CAIL_ASIC_PCIE_LINK_SPEED_SUPPORT_GEN1;
		}
		 
		if (platform_speed_cap == PCI_SPEED_UNKNOWN) {
			adev->pm.pcie_gen_mask |= (CAIL_PCIE_LINK_SPEED_SUPPORT_GEN1 |
						   CAIL_PCIE_LINK_SPEED_SUPPORT_GEN2);
		} else {
			if (platform_speed_cap == PCIE_SPEED_32_0GT)
				adev->pm.pcie_gen_mask |= (CAIL_PCIE_LINK_SPEED_SUPPORT_GEN1 |
							   CAIL_PCIE_LINK_SPEED_SUPPORT_GEN2 |
							   CAIL_PCIE_LINK_SPEED_SUPPORT_GEN3 |
							   CAIL_PCIE_LINK_SPEED_SUPPORT_GEN4 |
							   CAIL_PCIE_LINK_SPEED_SUPPORT_GEN5);
			else if (platform_speed_cap == PCIE_SPEED_16_0GT)
				adev->pm.pcie_gen_mask |= (CAIL_PCIE_LINK_SPEED_SUPPORT_GEN1 |
							   CAIL_PCIE_LINK_SPEED_SUPPORT_GEN2 |
							   CAIL_PCIE_LINK_SPEED_SUPPORT_GEN3 |
							   CAIL_PCIE_LINK_SPEED_SUPPORT_GEN4);
			else if (platform_speed_cap == PCIE_SPEED_8_0GT)
				adev->pm.pcie_gen_mask |= (CAIL_PCIE_LINK_SPEED_SUPPORT_GEN1 |
							   CAIL_PCIE_LINK_SPEED_SUPPORT_GEN2 |
							   CAIL_PCIE_LINK_SPEED_SUPPORT_GEN3);
			else if (platform_speed_cap == PCIE_SPEED_5_0GT)
				adev->pm.pcie_gen_mask |= (CAIL_PCIE_LINK_SPEED_SUPPORT_GEN1 |
							   CAIL_PCIE_LINK_SPEED_SUPPORT_GEN2);
			else
				adev->pm.pcie_gen_mask |= CAIL_PCIE_LINK_SPEED_SUPPORT_GEN1;

		}
	}
	if (adev->pm.pcie_mlw_mask == 0) {
		if (platform_link_width == PCIE_LNK_WIDTH_UNKNOWN) {
			adev->pm.pcie_mlw_mask |= AMDGPU_DEFAULT_PCIE_MLW_MASK;
		} else {
			switch (platform_link_width) {
			case PCIE_LNK_X32:
				adev->pm.pcie_mlw_mask = (CAIL_PCIE_LINK_WIDTH_SUPPORT_X32 |
							  CAIL_PCIE_LINK_WIDTH_SUPPORT_X16 |
							  CAIL_PCIE_LINK_WIDTH_SUPPORT_X12 |
							  CAIL_PCIE_LINK_WIDTH_SUPPORT_X8 |
							  CAIL_PCIE_LINK_WIDTH_SUPPORT_X4 |
							  CAIL_PCIE_LINK_WIDTH_SUPPORT_X2 |
							  CAIL_PCIE_LINK_WIDTH_SUPPORT_X1);
				break;
			case PCIE_LNK_X16:
				adev->pm.pcie_mlw_mask = (CAIL_PCIE_LINK_WIDTH_SUPPORT_X16 |
							  CAIL_PCIE_LINK_WIDTH_SUPPORT_X12 |
							  CAIL_PCIE_LINK_WIDTH_SUPPORT_X8 |
							  CAIL_PCIE_LINK_WIDTH_SUPPORT_X4 |
							  CAIL_PCIE_LINK_WIDTH_SUPPORT_X2 |
							  CAIL_PCIE_LINK_WIDTH_SUPPORT_X1);
				break;
			case PCIE_LNK_X12:
				adev->pm.pcie_mlw_mask = (CAIL_PCIE_LINK_WIDTH_SUPPORT_X12 |
							  CAIL_PCIE_LINK_WIDTH_SUPPORT_X8 |
							  CAIL_PCIE_LINK_WIDTH_SUPPORT_X4 |
							  CAIL_PCIE_LINK_WIDTH_SUPPORT_X2 |
							  CAIL_PCIE_LINK_WIDTH_SUPPORT_X1);
				break;
			case PCIE_LNK_X8:
				adev->pm.pcie_mlw_mask = (CAIL_PCIE_LINK_WIDTH_SUPPORT_X8 |
							  CAIL_PCIE_LINK_WIDTH_SUPPORT_X4 |
							  CAIL_PCIE_LINK_WIDTH_SUPPORT_X2 |
							  CAIL_PCIE_LINK_WIDTH_SUPPORT_X1);
				break;
			case PCIE_LNK_X4:
				adev->pm.pcie_mlw_mask = (CAIL_PCIE_LINK_WIDTH_SUPPORT_X4 |
							  CAIL_PCIE_LINK_WIDTH_SUPPORT_X2 |
							  CAIL_PCIE_LINK_WIDTH_SUPPORT_X1);
				break;
			case PCIE_LNK_X2:
				adev->pm.pcie_mlw_mask = (CAIL_PCIE_LINK_WIDTH_SUPPORT_X2 |
							  CAIL_PCIE_LINK_WIDTH_SUPPORT_X1);
				break;
			case PCIE_LNK_X1:
				adev->pm.pcie_mlw_mask = CAIL_PCIE_LINK_WIDTH_SUPPORT_X1;
				break;
			default:
				break;
			}
		}
	}
}

 
bool amdgpu_device_is_peer_accessible(struct amdgpu_device *adev,
				      struct amdgpu_device *peer_adev)
{
#ifdef CONFIG_HSA_AMD_P2P
	uint64_t address_mask = peer_adev->dev->dma_mask ?
		~*peer_adev->dev->dma_mask : ~((1ULL << 32) - 1);
	resource_size_t aper_limit =
		adev->gmc.aper_base + adev->gmc.aper_size - 1;
	bool p2p_access =
		!adev->gmc.xgmi.connected_to_cpu &&
		!(pci_p2pdma_distance(adev->pdev, peer_adev->dev, false) < 0);

	return pcie_p2p && p2p_access && (adev->gmc.visible_vram_size &&
		adev->gmc.real_vram_size == adev->gmc.visible_vram_size &&
		!(adev->gmc.aper_base & address_mask ||
		  aper_limit & address_mask));
#else
	return false;
#endif
}

int amdgpu_device_baco_enter(struct drm_device *dev)
{
	struct amdgpu_device *adev = drm_to_adev(dev);
	struct amdgpu_ras *ras = amdgpu_ras_get_context(adev);

	if (!amdgpu_device_supports_baco(dev))
		return -ENOTSUPP;

	if (ras && adev->ras_enabled &&
	    adev->nbio.funcs->enable_doorbell_interrupt)
		adev->nbio.funcs->enable_doorbell_interrupt(adev, false);

	return amdgpu_dpm_baco_enter(adev);
}

int amdgpu_device_baco_exit(struct drm_device *dev)
{
	struct amdgpu_device *adev = drm_to_adev(dev);
	struct amdgpu_ras *ras = amdgpu_ras_get_context(adev);
	int ret = 0;

	if (!amdgpu_device_supports_baco(dev))
		return -ENOTSUPP;

	ret = amdgpu_dpm_baco_exit(adev);
	if (ret)
		return ret;

	if (ras && adev->ras_enabled &&
	    adev->nbio.funcs->enable_doorbell_interrupt)
		adev->nbio.funcs->enable_doorbell_interrupt(adev, true);

	if (amdgpu_passthrough(adev) &&
	    adev->nbio.funcs->clear_doorbell_interrupt)
		adev->nbio.funcs->clear_doorbell_interrupt(adev);

	return 0;
}

 
pci_ers_result_t amdgpu_pci_error_detected(struct pci_dev *pdev, pci_channel_state_t state)
{
	struct drm_device *dev = pci_get_drvdata(pdev);
	struct amdgpu_device *adev = drm_to_adev(dev);
	int i;

	DRM_INFO("PCI error: detected callback, state(%d)!!\n", state);

	if (adev->gmc.xgmi.num_physical_nodes > 1) {
		DRM_WARN("No support for XGMI hive yet...");
		return PCI_ERS_RESULT_DISCONNECT;
	}

	adev->pci_channel_state = state;

	switch (state) {
	case pci_channel_io_normal:
		return PCI_ERS_RESULT_CAN_RECOVER;
	 
	case pci_channel_io_frozen:
		 
		amdgpu_device_lock_reset_domain(adev->reset_domain);
		amdgpu_device_set_mp1_state(adev);

		 
		for (i = 0; i < AMDGPU_MAX_RINGS; ++i) {
			struct amdgpu_ring *ring = adev->rings[i];

			if (!ring || !ring->sched.thread)
				continue;

			drm_sched_stop(&ring->sched, NULL);
		}
		atomic_inc(&adev->gpu_reset_counter);
		return PCI_ERS_RESULT_NEED_RESET;
	case pci_channel_io_perm_failure:
		 
		return PCI_ERS_RESULT_DISCONNECT;
	}

	return PCI_ERS_RESULT_NEED_RESET;
}

 
pci_ers_result_t amdgpu_pci_mmio_enabled(struct pci_dev *pdev)
{

	DRM_INFO("PCI error: mmio enabled callback!!\n");

	 

	 

	return PCI_ERS_RESULT_RECOVERED;
}

 
pci_ers_result_t amdgpu_pci_slot_reset(struct pci_dev *pdev)
{
	struct drm_device *dev = pci_get_drvdata(pdev);
	struct amdgpu_device *adev = drm_to_adev(dev);
	int r, i;
	struct amdgpu_reset_context reset_context;
	u32 memsize;
	struct list_head device_list;

	DRM_INFO("PCI error: slot reset callback!!\n");

	memset(&reset_context, 0, sizeof(reset_context));

	INIT_LIST_HEAD(&device_list);
	list_add_tail(&adev->reset_list, &device_list);

	 
	msleep(500);

	 
	amdgpu_device_load_pci_state(pdev);

	 
	for (i = 0; i < adev->usec_timeout; i++) {
		memsize = amdgpu_asic_get_config_memsize(adev);

		if (memsize != 0xffffffff)
			break;
		udelay(1);
	}
	if (memsize == 0xffffffff) {
		r = -ETIME;
		goto out;
	}

	reset_context.method = AMD_RESET_METHOD_NONE;
	reset_context.reset_req_dev = adev;
	set_bit(AMDGPU_NEED_FULL_RESET, &reset_context.flags);
	set_bit(AMDGPU_SKIP_HW_RESET, &reset_context.flags);

	adev->no_hw_access = true;
	r = amdgpu_device_pre_asic_reset(adev, &reset_context);
	adev->no_hw_access = false;
	if (r)
		goto out;

	r = amdgpu_do_asic_reset(&device_list, &reset_context);

out:
	if (!r) {
		if (amdgpu_device_cache_pci_state(adev->pdev))
			pci_restore_state(adev->pdev);

		DRM_INFO("PCIe error recovery succeeded\n");
	} else {
		DRM_ERROR("PCIe error recovery failed, err:%d", r);
		amdgpu_device_unset_mp1_state(adev);
		amdgpu_device_unlock_reset_domain(adev->reset_domain);
	}

	return r ? PCI_ERS_RESULT_DISCONNECT : PCI_ERS_RESULT_RECOVERED;
}

 
void amdgpu_pci_resume(struct pci_dev *pdev)
{
	struct drm_device *dev = pci_get_drvdata(pdev);
	struct amdgpu_device *adev = drm_to_adev(dev);
	int i;


	DRM_INFO("PCI error: resume callback!!\n");

	 
	if (adev->pci_channel_state != pci_channel_io_frozen)
		return;

	for (i = 0; i < AMDGPU_MAX_RINGS; ++i) {
		struct amdgpu_ring *ring = adev->rings[i];

		if (!ring || !ring->sched.thread)
			continue;

		drm_sched_start(&ring->sched, true);
	}

	amdgpu_device_unset_mp1_state(adev);
	amdgpu_device_unlock_reset_domain(adev->reset_domain);
}

bool amdgpu_device_cache_pci_state(struct pci_dev *pdev)
{
	struct drm_device *dev = pci_get_drvdata(pdev);
	struct amdgpu_device *adev = drm_to_adev(dev);
	int r;

	r = pci_save_state(pdev);
	if (!r) {
		kfree(adev->pci_state);

		adev->pci_state = pci_store_saved_state(pdev);

		if (!adev->pci_state) {
			DRM_ERROR("Failed to store PCI saved state");
			return false;
		}
	} else {
		DRM_WARN("Failed to save PCI state, err:%d\n", r);
		return false;
	}

	return true;
}

bool amdgpu_device_load_pci_state(struct pci_dev *pdev)
{
	struct drm_device *dev = pci_get_drvdata(pdev);
	struct amdgpu_device *adev = drm_to_adev(dev);
	int r;

	if (!adev->pci_state)
		return false;

	r = pci_load_saved_state(pdev, adev->pci_state);

	if (!r) {
		pci_restore_state(pdev);
	} else {
		DRM_WARN("Failed to load PCI state, err:%d\n", r);
		return false;
	}

	return true;
}

void amdgpu_device_flush_hdp(struct amdgpu_device *adev,
		struct amdgpu_ring *ring)
{
#ifdef CONFIG_X86_64
	if ((adev->flags & AMD_IS_APU) && !amdgpu_passthrough(adev))
		return;
#endif
	if (adev->gmc.xgmi.connected_to_cpu)
		return;

	if (ring && ring->funcs->emit_hdp_flush)
		amdgpu_ring_emit_hdp_flush(ring);
	else
		amdgpu_asic_flush_hdp(adev, ring);
}

void amdgpu_device_invalidate_hdp(struct amdgpu_device *adev,
		struct amdgpu_ring *ring)
{
#ifdef CONFIG_X86_64
	if ((adev->flags & AMD_IS_APU) && !amdgpu_passthrough(adev))
		return;
#endif
	if (adev->gmc.xgmi.connected_to_cpu)
		return;

	amdgpu_asic_invalidate_hdp(adev, ring);
}

int amdgpu_in_reset(struct amdgpu_device *adev)
{
	return atomic_read(&adev->reset_domain->in_gpu_reset);
}

 
void amdgpu_device_halt(struct amdgpu_device *adev)
{
	struct pci_dev *pdev = adev->pdev;
	struct drm_device *ddev = adev_to_drm(adev);

	amdgpu_xcp_dev_unplug(adev);
	drm_dev_unplug(ddev);

	amdgpu_irq_disable_all(adev);

	amdgpu_fence_driver_hw_fini(adev);

	adev->no_hw_access = true;

	amdgpu_device_unmap_mmio(adev);

	pci_disable_device(pdev);
	pci_wait_for_pending_transaction(pdev);
}

u32 amdgpu_device_pcie_port_rreg(struct amdgpu_device *adev,
				u32 reg)
{
	unsigned long flags, address, data;
	u32 r;

	address = adev->nbio.funcs->get_pcie_port_index_offset(adev);
	data = adev->nbio.funcs->get_pcie_port_data_offset(adev);

	spin_lock_irqsave(&adev->pcie_idx_lock, flags);
	WREG32(address, reg * 4);
	(void)RREG32(address);
	r = RREG32(data);
	spin_unlock_irqrestore(&adev->pcie_idx_lock, flags);
	return r;
}

void amdgpu_device_pcie_port_wreg(struct amdgpu_device *adev,
				u32 reg, u32 v)
{
	unsigned long flags, address, data;

	address = adev->nbio.funcs->get_pcie_port_index_offset(adev);
	data = adev->nbio.funcs->get_pcie_port_data_offset(adev);

	spin_lock_irqsave(&adev->pcie_idx_lock, flags);
	WREG32(address, reg * 4);
	(void)RREG32(address);
	WREG32(data, v);
	(void)RREG32(data);
	spin_unlock_irqrestore(&adev->pcie_idx_lock, flags);
}

 
struct dma_fence *amdgpu_device_switch_gang(struct amdgpu_device *adev,
					    struct dma_fence *gang)
{
	struct dma_fence *old = NULL;

	do {
		dma_fence_put(old);
		rcu_read_lock();
		old = dma_fence_get_rcu_safe(&adev->gang_submit);
		rcu_read_unlock();

		if (old == gang)
			break;

		if (!dma_fence_is_signaled(old))
			return old;

	} while (cmpxchg((struct dma_fence __force **)&adev->gang_submit,
			 old, gang) != old);

	dma_fence_put(old);
	return NULL;
}

bool amdgpu_device_has_display_hardware(struct amdgpu_device *adev)
{
	switch (adev->asic_type) {
#ifdef CONFIG_DRM_AMDGPU_SI
	case CHIP_HAINAN:
#endif
	case CHIP_TOPAZ:
		 
		return false;
#ifdef CONFIG_DRM_AMDGPU_SI
	case CHIP_TAHITI:
	case CHIP_PITCAIRN:
	case CHIP_VERDE:
	case CHIP_OLAND:
#endif
#ifdef CONFIG_DRM_AMDGPU_CIK
	case CHIP_BONAIRE:
	case CHIP_HAWAII:
	case CHIP_KAVERI:
	case CHIP_KABINI:
	case CHIP_MULLINS:
#endif
	case CHIP_TONGA:
	case CHIP_FIJI:
	case CHIP_POLARIS10:
	case CHIP_POLARIS11:
	case CHIP_POLARIS12:
	case CHIP_VEGAM:
	case CHIP_CARRIZO:
	case CHIP_STONEY:
		 
		return true;
	default:
		 
		if (!adev->ip_versions[DCE_HWIP][0] ||
		    (adev->harvest_ip_mask & AMD_HARVEST_IP_DMU_MASK))
			return false;
		return true;
	}
}

uint32_t amdgpu_device_wait_on_rreg(struct amdgpu_device *adev,
		uint32_t inst, uint32_t reg_addr, char reg_name[],
		uint32_t expected_value, uint32_t mask)
{
	uint32_t ret = 0;
	uint32_t old_ = 0;
	uint32_t tmp_ = RREG32(reg_addr);
	uint32_t loop = adev->usec_timeout;

	while ((tmp_ & (mask)) != (expected_value)) {
		if (old_ != tmp_) {
			loop = adev->usec_timeout;
			old_ = tmp_;
		} else
			udelay(1);
		tmp_ = RREG32(reg_addr);
		loop--;
		if (!loop) {
			DRM_WARN("Register(%d) [%s] failed to reach value 0x%08x != 0x%08xn",
				  inst, reg_name, (uint32_t)expected_value,
				  (uint32_t)(tmp_ & (mask)));
			ret = -ETIMEDOUT;
			break;
		}
	}
	return ret;
}
