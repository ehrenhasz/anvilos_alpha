 

#ifndef CDX_MCDI_FUNCTIONS_H
#define CDX_MCDI_FUNCTIONS_H

#include "mcdi.h"
#include "../cdx.h"

 
int cdx_mcdi_get_num_buses(struct cdx_mcdi *cdx);

 
int cdx_mcdi_get_num_devs(struct cdx_mcdi *cdx, int bus_num);

 
int cdx_mcdi_get_dev_config(struct cdx_mcdi *cdx,
			    u8 bus_num, u8 dev_num,
			    struct cdx_dev_params *dev_params);

 
int cdx_mcdi_reset_device(struct cdx_mcdi *cdx,
			  u8 bus_num, u8 dev_num);

#endif  
