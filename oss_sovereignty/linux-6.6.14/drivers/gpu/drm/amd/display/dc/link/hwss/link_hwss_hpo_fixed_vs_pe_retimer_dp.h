 
#ifndef __LINK_HWSS_HPO_FIXED_VS_PE_RETIMER_DP_H__
#define __LINK_HWSS_HPO_FIXED_VS_PE_RETIMER_DP_H__

#include "link.h"

bool requires_fixed_vs_pe_retimer_hpo_link_hwss(const struct dc_link *link);
const struct link_hwss *get_hpo_fixed_vs_pe_retimer_dp_link_hwss(void);

#endif  
