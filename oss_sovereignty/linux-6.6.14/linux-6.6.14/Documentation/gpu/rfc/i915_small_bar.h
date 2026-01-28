struct __drm_i915_memory_region_info {
	struct drm_i915_gem_memory_class_instance region;
	__u32 rsvd0;
	__u64 probed_size;
	__u64 unallocated_size;
	union {
		__u64 rsvd1[8];
		struct {
			__u64 probed_cpu_visible_size;
			__u64 unallocated_cpu_visible_size;
		};
	};
};
struct __drm_i915_gem_create_ext {
	__u64 size;
	__u32 handle;
#define I915_GEM_CREATE_EXT_FLAG_NEEDS_CPU_ACCESS (1 << 0)
	__u32 flags;
#define I915_GEM_CREATE_EXT_MEMORY_REGIONS 0
#define I915_GEM_CREATE_EXT_PROTECTED_CONTENT 1
	__u64 extensions;
};
