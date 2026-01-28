#ifndef _DMUB_DCN30_H_
#define _DMUB_DCN30_H_
#include "dmub_dcn20.h"
extern const struct dmub_srv_common_regs dmub_srv_dcn30_regs;
void dmub_dcn30_backdoor_load(struct dmub_srv *dmub,
			      const struct dmub_window *cw0,
			      const struct dmub_window *cw1);
void dmub_dcn30_setup_windows(struct dmub_srv *dmub,
			      const struct dmub_window *cw2,
			      const struct dmub_window *cw3,
			      const struct dmub_window *cw4,
			      const struct dmub_window *cw5,
			      const struct dmub_window *cw6);
#endif  
