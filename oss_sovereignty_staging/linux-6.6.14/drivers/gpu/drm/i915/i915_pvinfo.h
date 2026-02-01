 

#ifndef _I915_PVINFO_H_
#define _I915_PVINFO_H_

#include <linux/types.h>

 
#define VGT_PVINFO_PAGE	0x78000
#define VGT_PVINFO_SIZE	0x1000

 
#define VGT_MAGIC         0x4776544776544776ULL	 
#define VGT_VERSION_MAJOR 1
#define VGT_VERSION_MINOR 0

 
enum vgt_g2v_type {
	VGT_G2V_PPGTT_L3_PAGE_TABLE_CREATE = 2,
	VGT_G2V_PPGTT_L3_PAGE_TABLE_DESTROY,
	VGT_G2V_PPGTT_L4_PAGE_TABLE_CREATE,
	VGT_G2V_PPGTT_L4_PAGE_TABLE_DESTROY,
	VGT_G2V_EXECLIST_CONTEXT_CREATE,
	VGT_G2V_EXECLIST_CONTEXT_DESTROY,
	VGT_G2V_MAX,
};

 
#define VGT_CAPS_FULL_PPGTT		BIT(2)
#define VGT_CAPS_HWSP_EMULATION		BIT(3)
#define VGT_CAPS_HUGE_GTT		BIT(4)

struct vgt_if {
	u64 magic;		 
	u16 version_major;
	u16 version_minor;
	u32 vgt_id;		 
	u32 vgt_caps;		 
	u32 rsv1[11];		 
	 
	struct {
		 
		struct {
			u32 base;
			u32 size;
		} mappable_gmadr;	 
		 
		struct {
			u32 base;
			u32 size;
		} nonmappable_gmadr;	 
		 
		u32 fence_num;
		u32 rsv2[3];
	} avail_rs;		 
	u32 rsv3[0x200 - 24];	 
	 
	u32 rsv4;
	u32 display_ready;	 

	u32 rsv5[4];

	u32 g2v_notify;
	u32 rsv6[5];

	u32 cursor_x_hot;
	u32 cursor_y_hot;

	struct {
		u32 lo;
		u32 hi;
	} pdp[4];

	u32 execlist_context_descriptor_lo;
	u32 execlist_context_descriptor_hi;

	u32  rsv7[0x200 - 24];     
} __packed;

#define vgtif_offset(x) (offsetof(struct vgt_if, x))

#define vgtif_reg(x) _MMIO(VGT_PVINFO_PAGE + vgtif_offset(x))

 
#define VGT_DRV_DISPLAY_NOT_READY 0
#define VGT_DRV_DISPLAY_READY     1   

#endif  
