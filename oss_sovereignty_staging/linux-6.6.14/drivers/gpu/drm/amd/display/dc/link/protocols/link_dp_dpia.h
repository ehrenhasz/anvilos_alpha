 
 

#ifndef __DC_LINK_DPIA_H__
#define __DC_LINK_DPIA_H__

#include "link.h"

 
enum dc_status dpcd_get_tunneling_device_data(struct dc_link *link);

 
bool dpia_query_hpd_status(struct dc_link *link);
#endif  
