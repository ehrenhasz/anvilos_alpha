#define I915_PARAM_VM_BIND_VERSION	57
#define I915_VM_CREATE_FLAGS_USE_VM_BIND	(1 << 0)
#define DRM_I915_GEM_VM_BIND		0x3d
#define DRM_I915_GEM_VM_UNBIND		0x3e
#define DRM_I915_GEM_EXECBUFFER3	0x3f
#define DRM_IOCTL_I915_GEM_VM_BIND		DRM_IOWR(DRM_COMMAND_BASE + DRM_I915_GEM_VM_BIND, struct drm_i915_gem_vm_bind)
#define DRM_IOCTL_I915_GEM_VM_UNBIND		DRM_IOWR(DRM_COMMAND_BASE + DRM_I915_GEM_VM_UNBIND, struct drm_i915_gem_vm_bind)
#define DRM_IOCTL_I915_GEM_EXECBUFFER3		DRM_IOWR(DRM_COMMAND_BASE + DRM_I915_GEM_EXECBUFFER3, struct drm_i915_gem_execbuffer3)
struct drm_i915_gem_timeline_fence {
	__u32 handle;
	__u32 flags;
#define I915_TIMELINE_FENCE_WAIT            (1 << 0)
#define I915_TIMELINE_FENCE_SIGNAL          (1 << 1)
#define __I915_TIMELINE_FENCE_UNKNOWN_FLAGS (-(I915_TIMELINE_FENCE_SIGNAL << 1))
	__u64 value;
};
struct drm_i915_gem_vm_bind {
	__u32 vm_id;
	__u32 handle;
	__u64 start;
	__u64 offset;
	__u64 length;
	__u64 flags;
#define I915_GEM_VM_BIND_CAPTURE	(1 << 0)
	struct drm_i915_gem_timeline_fence fence;
	__u64 extensions;
};
struct drm_i915_gem_vm_unbind {
	__u32 vm_id;
	__u32 rsvd;
	__u64 start;
	__u64 length;
	__u64 flags;
	struct drm_i915_gem_timeline_fence fence;
	__u64 extensions;
};
struct drm_i915_gem_execbuffer3 {
	__u32 ctx_id;
	__u32 engine_idx;
	__u64 batch_address;
	__u64 flags;
	__u32 rsvd1;
	__u32 fence_count;
	__u64 timeline_fences;
	__u64 rsvd2;
	__u64 extensions;
};
struct drm_i915_gem_create_ext_vm_private {
#define I915_GEM_CREATE_EXT_VM_PRIVATE		2
	struct i915_user_extension base;
	__u32 vm_id;
};
