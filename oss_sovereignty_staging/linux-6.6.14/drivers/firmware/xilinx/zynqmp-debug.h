 
 

#ifndef __FIRMWARE_ZYNQMP_DEBUG_H__
#define __FIRMWARE_ZYNQMP_DEBUG_H__

#if IS_REACHABLE(CONFIG_ZYNQMP_FIRMWARE_DEBUG)
void zynqmp_pm_api_debugfs_init(void);
void zynqmp_pm_api_debugfs_exit(void);
#else
static inline void zynqmp_pm_api_debugfs_init(void) { }
static inline void zynqmp_pm_api_debugfs_exit(void) { }
#endif

#endif  
