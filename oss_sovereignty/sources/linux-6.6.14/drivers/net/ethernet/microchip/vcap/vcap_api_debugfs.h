


#ifndef __VCAP_API_DEBUGFS__
#define __VCAP_API_DEBUGFS__

#include <linux/types.h>
#include <linux/debugfs.h>
#include <linux/netdevice.h>

#include "vcap_api.h"

#if defined(CONFIG_DEBUG_FS)

void vcap_port_debugfs(struct device *dev, struct dentry *parent,
		       struct vcap_control *vctrl,
		       struct net_device *ndev);


struct dentry *vcap_debugfs(struct device *dev, struct dentry *parent,
			    struct vcap_control *vctrl);

#else

static inline void vcap_port_debugfs(struct device *dev, struct dentry *parent,
				     struct vcap_control *vctrl,
				     struct net_device *ndev)
{
}

static inline struct dentry *vcap_debugfs(struct device *dev,
					  struct dentry *parent,
					  struct vcap_control *vctrl)
{
	return NULL;
}

#endif
#endif 
