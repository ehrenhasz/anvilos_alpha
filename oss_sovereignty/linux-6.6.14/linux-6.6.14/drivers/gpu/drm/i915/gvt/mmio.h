#ifndef _GVT_MMIO_H_
#define _GVT_MMIO_H_
#include <linux/types.h>
struct intel_gvt;
struct intel_vgpu;
#define D_BDW   (1 << 0)
#define D_SKL	(1 << 1)
#define D_KBL	(1 << 2)
#define D_BXT	(1 << 3)
#define D_CFL	(1 << 4)
#define D_GEN9PLUS	(D_SKL | D_KBL | D_BXT | D_CFL)
#define D_GEN8PLUS	(D_BDW | D_SKL | D_KBL | D_BXT | D_CFL)
#define D_SKL_PLUS	(D_SKL | D_KBL | D_BXT | D_CFL)
#define D_BDW_PLUS	(D_BDW | D_SKL | D_KBL | D_BXT | D_CFL)
#define D_PRE_SKL	(D_BDW)
#define D_ALL		(D_BDW | D_SKL | D_KBL | D_BXT | D_CFL)
typedef int (*gvt_mmio_func)(struct intel_vgpu *, unsigned int, void *,
			     unsigned int);
struct intel_gvt_mmio_info {
	u32 offset;
	u64 ro_mask;
	u32 device;
	gvt_mmio_func read;
	gvt_mmio_func write;
	u32 addr_range;
	struct hlist_node node;
};
const struct intel_engine_cs *
intel_gvt_render_mmio_to_engine(struct intel_gvt *gvt, unsigned int reg);
unsigned long intel_gvt_get_device_type(struct intel_gvt *gvt);
int intel_gvt_setup_mmio_info(struct intel_gvt *gvt);
void intel_gvt_clean_mmio_info(struct intel_gvt *gvt);
int intel_gvt_for_each_tracked_mmio(struct intel_gvt *gvt,
	int (*handler)(struct intel_gvt *gvt, u32 offset, void *data),
	void *data);
struct intel_gvt_mmio_info *intel_gvt_find_mmio_info(struct intel_gvt *gvt,
						  unsigned int offset);
int intel_vgpu_init_mmio(struct intel_vgpu *vgpu);
void intel_vgpu_reset_mmio(struct intel_vgpu *vgpu, bool dmlr);
void intel_vgpu_clean_mmio(struct intel_vgpu *vgpu);
int intel_vgpu_gpa_to_mmio_offset(struct intel_vgpu *vgpu, u64 gpa);
int intel_vgpu_emulate_mmio_read(struct intel_vgpu *vgpu, u64 pa,
				void *p_data, unsigned int bytes);
int intel_vgpu_emulate_mmio_write(struct intel_vgpu *vgpu, u64 pa,
				void *p_data, unsigned int bytes);
int intel_vgpu_default_mmio_read(struct intel_vgpu *vgpu, unsigned int offset,
				 void *p_data, unsigned int bytes);
int intel_vgpu_default_mmio_write(struct intel_vgpu *vgpu, unsigned int offset,
				  void *p_data, unsigned int bytes);
bool intel_gvt_in_force_nonpriv_whitelist(struct intel_gvt *gvt,
					  unsigned int offset);
int intel_vgpu_mmio_reg_rw(struct intel_vgpu *vgpu, unsigned int offset,
			   void *pdata, unsigned int bytes, bool is_read);
int intel_vgpu_mask_mmio_write(struct intel_vgpu *vgpu, unsigned int offset,
				  void *p_data, unsigned int bytes);
void intel_gvt_restore_fence(struct intel_gvt *gvt);
void intel_gvt_restore_mmio(struct intel_gvt *gvt);
#endif
