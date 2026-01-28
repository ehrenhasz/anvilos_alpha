#ifndef __CN23XX_PF_DEVICE_H__
#define __CN23XX_PF_DEVICE_H__
#include "cn23xx_pf_regs.h"
struct octeon_cn23xx_pf {
	u8 __iomem *intr_sum_reg64;
	u8 __iomem *intr_enb_reg64;
	u64 intr_mask64;
	struct octeon_config *conf;
};
#define CN23XX_SLI_DEF_BP			0x40
struct oct_vf_stats {
	u64 rx_packets;
	u64 tx_packets;
	u64 rx_bytes;
	u64 tx_bytes;
	u64 broadcast;
	u64 multicast;
};
int setup_cn23xx_octeon_pf_device(struct octeon_device *oct);
int validate_cn23xx_pf_config_info(struct octeon_device *oct,
				   struct octeon_config *conf23xx);
u32 cn23xx_pf_get_oq_ticks(struct octeon_device *oct, u32 time_intr_in_us);
void cn23xx_dump_pf_initialized_regs(struct octeon_device *oct);
int cn23xx_sriov_config(struct octeon_device *oct);
int cn23xx_fw_loaded(struct octeon_device *oct);
void cn23xx_tell_vf_its_macaddr_changed(struct octeon_device *oct, int vfidx,
					u8 *mac);
int cn23xx_get_vf_stats(struct octeon_device *oct, int ifidx,
			struct oct_vf_stats *stats);
#endif
