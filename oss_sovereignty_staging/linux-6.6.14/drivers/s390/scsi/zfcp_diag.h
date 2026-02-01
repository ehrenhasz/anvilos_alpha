 
 

#ifndef ZFCP_DIAG_H
#define ZFCP_DIAG_H

#include <linux/spinlock.h>

#include "zfcp_fsf.h"
#include "zfcp_def.h"

 
struct zfcp_diag_header {
	spinlock_t	access_lock;

	 
	u64		updating	:1;
	u64		incomplete	:1;

	unsigned long	timestamp;

	void		*buffer;
	size_t		buffer_size;
};

 
struct zfcp_diag_adapter {
	unsigned long	max_age;

	struct zfcp_diag_adapter_port_data {
		struct zfcp_diag_header		header;
		struct fsf_qtcb_bottom_port	data;
	} port_data;
	struct zfcp_diag_adapter_config_data {
		struct zfcp_diag_header		header;
		struct fsf_qtcb_bottom_config	data;
	} config_data;
};

int zfcp_diag_adapter_setup(struct zfcp_adapter *const adapter);
void zfcp_diag_adapter_free(struct zfcp_adapter *const adapter);

void zfcp_diag_update_xdata(struct zfcp_diag_header *const hdr,
			    const void *const data, const bool incomplete);

 
typedef int (*zfcp_diag_update_buffer_func)(struct zfcp_adapter *const adapter);

int zfcp_diag_update_config_data_buffer(struct zfcp_adapter *const adapter);
int zfcp_diag_update_port_data_buffer(struct zfcp_adapter *const adapter);
int zfcp_diag_update_buffer_limited(struct zfcp_adapter *const adapter,
				    struct zfcp_diag_header *const hdr,
				    zfcp_diag_update_buffer_func buffer_update);

 
static inline bool
zfcp_diag_support_sfp(const struct zfcp_adapter *const adapter)
{
	return !!(adapter->adapter_features & FSF_FEATURE_REPORT_SFP_DATA);
}

#endif  
