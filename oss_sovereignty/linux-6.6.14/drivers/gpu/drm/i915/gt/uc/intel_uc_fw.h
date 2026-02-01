 
 

#ifndef _INTEL_UC_FW_H_
#define _INTEL_UC_FW_H_

#include <linux/sizes.h>
#include <linux/types.h>
#include "intel_uc_fw_abi.h"
#include "intel_device_info.h"
#include "i915_gem.h"
#include "i915_vma.h"

struct drm_printer;
struct drm_i915_private;
struct intel_gt;

 
#define INTEL_UC_FIRMWARE_URL "https://git.kernel.org/pub/scm/linux/kernel/git/firmware/linux-firmware.git/tree/i915"

 

enum intel_uc_fw_status {
	INTEL_UC_FIRMWARE_NOT_SUPPORTED = -1,  
	INTEL_UC_FIRMWARE_UNINITIALIZED = 0,  
	INTEL_UC_FIRMWARE_DISABLED,  
	INTEL_UC_FIRMWARE_SELECTED,  
	INTEL_UC_FIRMWARE_MISSING,  
	INTEL_UC_FIRMWARE_ERROR,  
	INTEL_UC_FIRMWARE_AVAILABLE,  
	INTEL_UC_FIRMWARE_INIT_FAIL,  
	INTEL_UC_FIRMWARE_LOADABLE,  
	INTEL_UC_FIRMWARE_LOAD_FAIL,  
	INTEL_UC_FIRMWARE_TRANSFERRED,  
	INTEL_UC_FIRMWARE_RUNNING  
};

enum intel_uc_fw_type {
	INTEL_UC_FW_TYPE_GUC = 0,
	INTEL_UC_FW_TYPE_HUC,
	INTEL_UC_FW_TYPE_GSC,
};
#define INTEL_UC_FW_NUM_TYPES 3

struct intel_uc_fw_ver {
	u32 major;
	u32 minor;
	u32 patch;
	u32 build;
};

 
struct intel_uc_fw_file {
	const char *path;
	struct intel_uc_fw_ver ver;
};

 
struct intel_uc_fw {
	enum intel_uc_fw_type type;
	union {
		const enum intel_uc_fw_status status;
		enum intel_uc_fw_status __status;  
	};
	struct intel_uc_fw_file file_wanted;
	struct intel_uc_fw_file file_selected;
	bool user_overridden;
	size_t size;
	struct drm_i915_gem_object *obj;

	 
	bool needs_ggtt_mapping;

	 
	struct i915_vma_resource vma_res;
	struct i915_vma *rsa_data;

	u32 rsa_size;
	u32 ucode_size;
	u32 private_data_size;

	u32 dma_start_offset;

	bool has_gsc_headers;
};

 
#define INTEL_UC_RSVD_GGTT_PER_FW SZ_2M

#ifdef CONFIG_DRM_I915_DEBUG_GUC
void intel_uc_fw_change_status(struct intel_uc_fw *uc_fw,
			       enum intel_uc_fw_status status);
#else
static inline void intel_uc_fw_change_status(struct intel_uc_fw *uc_fw,
					     enum intel_uc_fw_status status)
{
	uc_fw->__status = status;
}
#endif

static inline
const char *intel_uc_fw_status_repr(enum intel_uc_fw_status status)
{
	switch (status) {
	case INTEL_UC_FIRMWARE_NOT_SUPPORTED:
		return "N/A";
	case INTEL_UC_FIRMWARE_UNINITIALIZED:
		return "UNINITIALIZED";
	case INTEL_UC_FIRMWARE_DISABLED:
		return "DISABLED";
	case INTEL_UC_FIRMWARE_SELECTED:
		return "SELECTED";
	case INTEL_UC_FIRMWARE_MISSING:
		return "MISSING";
	case INTEL_UC_FIRMWARE_ERROR:
		return "ERROR";
	case INTEL_UC_FIRMWARE_AVAILABLE:
		return "AVAILABLE";
	case INTEL_UC_FIRMWARE_INIT_FAIL:
		return "INIT FAIL";
	case INTEL_UC_FIRMWARE_LOADABLE:
		return "LOADABLE";
	case INTEL_UC_FIRMWARE_LOAD_FAIL:
		return "LOAD FAIL";
	case INTEL_UC_FIRMWARE_TRANSFERRED:
		return "TRANSFERRED";
	case INTEL_UC_FIRMWARE_RUNNING:
		return "RUNNING";
	}
	return "<invalid>";
}

static inline int intel_uc_fw_status_to_error(enum intel_uc_fw_status status)
{
	switch (status) {
	case INTEL_UC_FIRMWARE_NOT_SUPPORTED:
		return -ENODEV;
	case INTEL_UC_FIRMWARE_UNINITIALIZED:
		return -EACCES;
	case INTEL_UC_FIRMWARE_DISABLED:
		return -EPERM;
	case INTEL_UC_FIRMWARE_MISSING:
		return -ENOENT;
	case INTEL_UC_FIRMWARE_ERROR:
		return -ENOEXEC;
	case INTEL_UC_FIRMWARE_INIT_FAIL:
	case INTEL_UC_FIRMWARE_LOAD_FAIL:
		return -EIO;
	case INTEL_UC_FIRMWARE_SELECTED:
		return -ESTALE;
	case INTEL_UC_FIRMWARE_AVAILABLE:
	case INTEL_UC_FIRMWARE_LOADABLE:
	case INTEL_UC_FIRMWARE_TRANSFERRED:
	case INTEL_UC_FIRMWARE_RUNNING:
		return 0;
	}
	return -EINVAL;
}

static inline const char *intel_uc_fw_type_repr(enum intel_uc_fw_type type)
{
	switch (type) {
	case INTEL_UC_FW_TYPE_GUC:
		return "GuC";
	case INTEL_UC_FW_TYPE_HUC:
		return "HuC";
	case INTEL_UC_FW_TYPE_GSC:
		return "GSC";
	}
	return "uC";
}

static inline enum intel_uc_fw_status
__intel_uc_fw_status(struct intel_uc_fw *uc_fw)
{
	 
	GEM_BUG_ON(uc_fw->status == INTEL_UC_FIRMWARE_UNINITIALIZED);
	return uc_fw->status;
}

static inline bool intel_uc_fw_is_supported(struct intel_uc_fw *uc_fw)
{
	return __intel_uc_fw_status(uc_fw) != INTEL_UC_FIRMWARE_NOT_SUPPORTED;
}

static inline bool intel_uc_fw_is_enabled(struct intel_uc_fw *uc_fw)
{
	return __intel_uc_fw_status(uc_fw) > INTEL_UC_FIRMWARE_DISABLED;
}

static inline bool intel_uc_fw_is_available(struct intel_uc_fw *uc_fw)
{
	return __intel_uc_fw_status(uc_fw) >= INTEL_UC_FIRMWARE_AVAILABLE;
}

static inline bool intel_uc_fw_is_loadable(struct intel_uc_fw *uc_fw)
{
	return __intel_uc_fw_status(uc_fw) >= INTEL_UC_FIRMWARE_LOADABLE;
}

static inline bool intel_uc_fw_is_loaded(struct intel_uc_fw *uc_fw)
{
	return __intel_uc_fw_status(uc_fw) >= INTEL_UC_FIRMWARE_TRANSFERRED;
}

static inline bool intel_uc_fw_is_running(struct intel_uc_fw *uc_fw)
{
	return __intel_uc_fw_status(uc_fw) == INTEL_UC_FIRMWARE_RUNNING;
}

static inline bool intel_uc_fw_is_overridden(const struct intel_uc_fw *uc_fw)
{
	return uc_fw->user_overridden;
}

static inline void intel_uc_fw_sanitize(struct intel_uc_fw *uc_fw)
{
	if (intel_uc_fw_is_loaded(uc_fw))
		intel_uc_fw_change_status(uc_fw, INTEL_UC_FIRMWARE_LOADABLE);
}

static inline u32 __intel_uc_fw_get_upload_size(struct intel_uc_fw *uc_fw)
{
	return sizeof(struct uc_css_header) + uc_fw->ucode_size;
}

 
static inline u32 intel_uc_fw_get_upload_size(struct intel_uc_fw *uc_fw)
{
	if (!intel_uc_fw_is_available(uc_fw))
		return 0;

	return __intel_uc_fw_get_upload_size(uc_fw);
}

void intel_uc_fw_version_from_gsc_manifest(struct intel_uc_fw_ver *ver,
					   const void *data);
int intel_uc_check_file_version(struct intel_uc_fw *uc_fw, bool *old_ver);
void intel_uc_fw_init_early(struct intel_uc_fw *uc_fw,
			    enum intel_uc_fw_type type,
			    bool needs_ggtt_mapping);
int intel_uc_fw_fetch(struct intel_uc_fw *uc_fw);
void intel_uc_fw_cleanup_fetch(struct intel_uc_fw *uc_fw);
int intel_uc_fw_upload(struct intel_uc_fw *uc_fw, u32 offset, u32 dma_flags);
int intel_uc_fw_init(struct intel_uc_fw *uc_fw);
void intel_uc_fw_fini(struct intel_uc_fw *uc_fw);
void intel_uc_fw_resume_mapping(struct intel_uc_fw *uc_fw);
size_t intel_uc_fw_copy_rsa(struct intel_uc_fw *uc_fw, void *dst, u32 max_len);
int intel_uc_fw_mark_load_failed(struct intel_uc_fw *uc_fw, int err);
void intel_uc_fw_dump(const struct intel_uc_fw *uc_fw, struct drm_printer *p);

#endif
