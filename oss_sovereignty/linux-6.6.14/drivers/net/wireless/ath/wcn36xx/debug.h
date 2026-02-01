 

#ifndef _WCN36XX_DEBUG_H_
#define _WCN36XX_DEBUG_H_

#include <linux/kernel.h>

#define WCN36xx_MAX_DUMP_ARGS	5

#ifdef CONFIG_WCN36XX_DEBUGFS
struct wcn36xx_dfs_file {
	struct dentry *dentry;
	u32 value;
};

struct wcn36xx_dfs_entry {
	struct dentry *rootdir;
	struct wcn36xx_dfs_file file_bmps_switcher;
	struct wcn36xx_dfs_file file_dump;
	struct wcn36xx_dfs_file file_firmware_feat_caps;
};

void wcn36xx_debugfs_init(struct wcn36xx *wcn);
void wcn36xx_debugfs_exit(struct wcn36xx *wcn);

#else
static inline void wcn36xx_debugfs_init(struct wcn36xx *wcn)
{
}
static inline void wcn36xx_debugfs_exit(struct wcn36xx *wcn)
{
}

#endif  

#endif	 
