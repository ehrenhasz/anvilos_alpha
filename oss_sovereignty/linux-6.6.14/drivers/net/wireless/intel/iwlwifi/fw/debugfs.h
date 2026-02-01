 
 
#include "runtime.h"

#ifdef CONFIG_IWLWIFI_DEBUGFS
void iwl_fwrt_dbgfs_register(struct iwl_fw_runtime *fwrt,
			    struct dentry *dbgfs_dir);

#else
static inline void iwl_fwrt_dbgfs_register(struct iwl_fw_runtime *fwrt,
					   struct dentry *dbgfs_dir) { }

#endif  
