#ifndef __LINK_HWSS_DIO_FIXED_VS_PE_RETIMER_H__
#define __LINK_HWSS_DIO_FIXED_VS_PE_RETIMER_H__
#include "link.h"
uint32_t dp_dio_fixed_vs_pe_retimer_get_lttpr_write_address(struct dc_link *link);
uint8_t dp_dio_fixed_vs_pe_retimer_lane_cfg_to_hw_cfg(struct dc_link *link);
void dp_dio_fixed_vs_pe_retimer_exit_manual_automation(struct dc_link *link);
void enable_dio_fixed_vs_pe_retimer_program_4lane_output(struct dc_link *link);
bool requires_fixed_vs_pe_retimer_dio_link_hwss(const struct dc_link *link);
const struct link_hwss *get_dio_fixed_vs_pe_retimer_link_hwss(void);
#endif  
