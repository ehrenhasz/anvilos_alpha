 
 

#ifndef DPSECI_DEBUGFS_H
#define DPSECI_DEBUGFS_H

#include <linux/dcache.h>
#include "caamalg_qi2.h"

#ifdef CONFIG_DEBUG_FS
void dpaa2_dpseci_debugfs_init(struct dpaa2_caam_priv *priv);
void dpaa2_dpseci_debugfs_exit(struct dpaa2_caam_priv *priv);
#else
static inline void dpaa2_dpseci_debugfs_init(struct dpaa2_caam_priv *priv) {}
static inline void dpaa2_dpseci_debugfs_exit(struct dpaa2_caam_priv *priv) {}
#endif  

#endif  
