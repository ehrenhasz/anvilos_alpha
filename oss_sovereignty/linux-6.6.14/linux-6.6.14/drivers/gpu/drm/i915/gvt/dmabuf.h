#ifndef _GVT_DMABUF_H_
#define _GVT_DMABUF_H_
#include <linux/vfio.h>
struct intel_vgpu_fb_info {
	__u64 start;
	__u64 start_gpa;
	__u64 drm_format_mod;
	__u32 drm_format;	 
	__u32 width;	 
	__u32 height;	 
	__u32 stride;	 
	__u32 size;	 
	__u32 x_pos;	 
	__u32 y_pos;	 
	__u32 x_hot;     
	__u32 y_hot;     
	struct intel_vgpu_dmabuf_obj *obj;
};
struct intel_vgpu_dmabuf_obj {
	struct intel_vgpu *vgpu;
	struct intel_vgpu_fb_info *info;
	__u32 dmabuf_id;
	struct kref kref;
	bool initref;
	struct list_head list;
};
int intel_vgpu_query_plane(struct intel_vgpu *vgpu, void *args);
int intel_vgpu_get_dmabuf(struct intel_vgpu *vgpu, unsigned int dmabuf_id);
void intel_vgpu_dmabuf_cleanup(struct intel_vgpu *vgpu);
#endif
