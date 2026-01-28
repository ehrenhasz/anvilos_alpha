#ifndef __LINK_HWSS_DPIA_H__
#define __LINK_HWSS_DPIA_H__
#include "link_hwss.h"
const struct link_hwss *get_dpia_link_hwss(void);
bool can_use_dpia_link_hwss(const struct dc_link *link,
		const struct link_resource *link_res);
#endif  
