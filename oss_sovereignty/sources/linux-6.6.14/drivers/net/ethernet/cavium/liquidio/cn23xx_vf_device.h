


#ifndef __CN23XX_VF_DEVICE_H__
#define __CN23XX_VF_DEVICE_H__

#include "cn23xx_vf_regs.h"


struct octeon_cn23xx_vf {
	struct octeon_config *conf;
};

#define BUSY_READING_REG_VF_LOOP_COUNT		10000

#define CN23XX_MAILBOX_MSGPARAM_SIZE		6

void cn23xx_vf_ask_pf_to_do_flr(struct octeon_device *oct);

int cn23xx_octeon_pfvf_handshake(struct octeon_device *oct);

int cn23xx_setup_octeon_vf_device(struct octeon_device *oct);

u32 cn23xx_vf_get_oq_ticks(struct octeon_device *oct, u32 time_intr_in_us);

void cn23xx_dump_vf_initialized_regs(struct octeon_device *oct);
#endif
