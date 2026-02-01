 
 
#ifndef _VMWGFX_BINDING_H_
#define _VMWGFX_BINDING_H_

#include <linux/list.h>

#include "device_include/svga3d_reg.h"

#define VMW_MAX_VIEW_BINDINGS 128

#define VMW_MAX_UAV_BIND_TYPE 2

struct vmw_private;
struct vmw_ctx_binding_state;

 
enum vmw_ctx_binding_type {
	vmw_ctx_binding_shader,
	vmw_ctx_binding_rt,
	vmw_ctx_binding_tex,
	vmw_ctx_binding_cb,
	vmw_ctx_binding_dx_shader,
	vmw_ctx_binding_dx_rt,
	vmw_ctx_binding_sr,
	vmw_ctx_binding_ds,
	vmw_ctx_binding_so_target,
	vmw_ctx_binding_vb,
	vmw_ctx_binding_ib,
	vmw_ctx_binding_uav,
	vmw_ctx_binding_cs_uav,
	vmw_ctx_binding_so,
	vmw_ctx_binding_max
};

 
struct vmw_ctx_bindinfo {
	struct list_head ctx_list;
	struct list_head res_list;
	struct vmw_resource *ctx;
	struct vmw_resource *res;
	enum vmw_ctx_binding_type bt;
	bool scrubbed;
};

 
struct vmw_ctx_bindinfo_tex {
	struct vmw_ctx_bindinfo bi;
	uint32 texture_stage;
};

 
struct vmw_ctx_bindinfo_shader {
	struct vmw_ctx_bindinfo bi;
	SVGA3dShaderType shader_slot;
};

 
struct vmw_ctx_bindinfo_cb {
	struct vmw_ctx_bindinfo bi;
	SVGA3dShaderType shader_slot;
	uint32 offset;
	uint32 size;
	uint32 slot;
};

 
struct vmw_ctx_bindinfo_view {
	struct vmw_ctx_bindinfo bi;
	SVGA3dShaderType shader_slot;
	uint32 slot;
};

 
struct vmw_ctx_bindinfo_so_target {
	struct vmw_ctx_bindinfo bi;
	uint32 offset;
	uint32 size;
	uint32 slot;
};

 
struct vmw_ctx_bindinfo_vb {
	struct vmw_ctx_bindinfo bi;
	uint32 offset;
	uint32 stride;
	uint32 slot;
};

 
struct vmw_ctx_bindinfo_ib {
	struct vmw_ctx_bindinfo bi;
	uint32 offset;
	uint32 format;
};

 
struct vmw_dx_shader_bindings {
	struct vmw_ctx_bindinfo_shader shader;
	struct vmw_ctx_bindinfo_cb const_buffers[SVGA3D_DX_MAX_CONSTBUFFERS];
	struct vmw_ctx_bindinfo_view shader_res[SVGA3D_DX_MAX_SRVIEWS];
	DECLARE_BITMAP(dirty_sr, SVGA3D_DX_MAX_SRVIEWS);
	unsigned long dirty;
};

 
struct vmw_ctx_bindinfo_uav {
	struct vmw_ctx_bindinfo_view views[SVGA3D_DX11_1_MAX_UAVIEWS];
	uint32 index;
};

 
struct vmw_ctx_bindinfo_so {
	struct vmw_ctx_bindinfo bi;
	uint32 slot;
};

extern void vmw_binding_add(struct vmw_ctx_binding_state *cbs,
			    const struct vmw_ctx_bindinfo *ci,
			    u32 shader_slot, u32 slot);
extern void vmw_binding_cb_offset_update(struct vmw_ctx_binding_state *cbs,
					 u32 shader_slot, u32 slot, u32 offsetInBytes);
extern void vmw_binding_add_uav_index(struct vmw_ctx_binding_state *cbs,
				      uint32 slot, uint32 splice_index);
extern void
vmw_binding_state_commit(struct vmw_ctx_binding_state *to,
			 struct vmw_ctx_binding_state *from);
extern void vmw_binding_res_list_kill(struct list_head *head);
extern void vmw_binding_res_list_scrub(struct list_head *head);
extern int vmw_binding_rebind_all(struct vmw_ctx_binding_state *cbs);
extern void vmw_binding_state_kill(struct vmw_ctx_binding_state *cbs);
extern void vmw_binding_state_scrub(struct vmw_ctx_binding_state *cbs);
extern struct vmw_ctx_binding_state *
vmw_binding_state_alloc(struct vmw_private *dev_priv);
extern void vmw_binding_state_free(struct vmw_ctx_binding_state *cbs);
extern struct list_head *
vmw_binding_state_list(struct vmw_ctx_binding_state *cbs);
extern void vmw_binding_state_reset(struct vmw_ctx_binding_state *cbs);
extern u32 vmw_binding_dirtying(enum vmw_ctx_binding_type binding_type);


#endif
