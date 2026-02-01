 
 

#ifndef __SPARX5_VCAP_DEBUGFS_H__
#define __SPARX5_VCAP_DEBUGFS_H__

#include <linux/netdevice.h>

#include <vcap_api.h>
#include <vcap_api_client.h>

#if defined(CONFIG_DEBUG_FS)

 
int sparx5_port_info(struct net_device *ndev,
		     struct vcap_admin *admin,
		     struct vcap_output_print *out);

#else

static inline int sparx5_port_info(struct net_device *ndev,
				   struct vcap_admin *admin,
				   struct vcap_output_print *out)
{
	return 0;
}

#endif

#endif  
