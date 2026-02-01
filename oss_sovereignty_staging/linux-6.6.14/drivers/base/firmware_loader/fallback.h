 
#ifndef __FIRMWARE_FALLBACK_H
#define __FIRMWARE_FALLBACK_H

#include <linux/firmware.h>
#include <linux/device.h>

#include "firmware.h"
#include "sysfs.h"

#ifdef CONFIG_FW_LOADER_USER_HELPER
int firmware_fallback_sysfs(struct firmware *fw, const char *name,
			    struct device *device,
			    u32 opt_flags,
			    int ret);
void kill_pending_fw_fallback_reqs(bool only_kill_custom);

void fw_fallback_set_cache_timeout(void);
void fw_fallback_set_default_timeout(void);

#else  
static inline int firmware_fallback_sysfs(struct firmware *fw, const char *name,
					  struct device *device,
					  u32 opt_flags,
					  int ret)
{
	 
	return ret;
}

static inline void kill_pending_fw_fallback_reqs(bool only_kill_custom) { }
static inline void fw_fallback_set_cache_timeout(void) { }
static inline void fw_fallback_set_default_timeout(void) { }
#endif  

#ifdef CONFIG_EFI_EMBEDDED_FIRMWARE
int firmware_fallback_platform(struct fw_priv *fw_priv);
#else
static inline int firmware_fallback_platform(struct fw_priv *fw_priv)
{
	return -ENOENT;
}
#endif

#endif  
