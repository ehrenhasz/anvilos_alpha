#include "core.h"
#ifdef CONFIG_DEBUG_FS
int dwc2_debugfs_init(struct dwc2_hsotg *hsotg);
void dwc2_debugfs_exit(struct dwc2_hsotg *hsotg);
#else
static inline int dwc2_debugfs_init(struct dwc2_hsotg *hsotg)
{  return 0;  }
static inline void dwc2_debugfs_exit(struct dwc2_hsotg *hsotg)
{  }
#endif
