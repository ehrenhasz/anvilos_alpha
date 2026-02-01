 

#ifndef _DMUB_HW_LOCK_MGR_H_
#define _DMUB_HW_LOCK_MGR_H_

#include "dc_dmub_srv.h"
#include "core_types.h"

void dmub_hw_lock_mgr_cmd(struct dc_dmub_srv *dmub_srv,
				bool lock,
				union dmub_hw_lock_flags *hw_locks,
				struct dmub_hw_lock_inst_flags *inst_flags);

void dmub_hw_lock_mgr_inbox0_cmd(struct dc_dmub_srv *dmub_srv,
		union dmub_inbox0_cmd_lock_hw hw_lock_cmd);

bool should_use_dmub_lock(struct dc_link *link);

#endif  
