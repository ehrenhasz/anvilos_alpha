


#ifndef _MLXSW_PCI_HW_H
#define _MLXSW_PCI_HW_H

#include <linux/bitops.h>

#include "item.h"

#define MLXSW_PCI_BAR0_SIZE		(1024 * 1024) 
#define MLXSW_PCI_PAGE_SIZE		4096

#define MLXSW_PCI_CIR_BASE			0x71000
#define MLXSW_PCI_CIR_IN_PARAM_HI		MLXSW_PCI_CIR_BASE
#define MLXSW_PCI_CIR_IN_PARAM_LO		(MLXSW_PCI_CIR_BASE + 0x04)
#define MLXSW_PCI_CIR_IN_MODIFIER		(MLXSW_PCI_CIR_BASE + 0x08)
#define MLXSW_PCI_CIR_OUT_PARAM_HI		(MLXSW_PCI_CIR_BASE + 0x0C)
#define MLXSW_PCI_CIR_OUT_PARAM_LO		(MLXSW_PCI_CIR_BASE + 0x10)
#define MLXSW_PCI_CIR_TOKEN			(MLXSW_PCI_CIR_BASE + 0x14)
#define MLXSW_PCI_CIR_CTRL			(MLXSW_PCI_CIR_BASE + 0x18)
#define MLXSW_PCI_CIR_CTRL_GO_BIT		BIT(23)
#define MLXSW_PCI_CIR_CTRL_EVREQ_BIT		BIT(22)
#define MLXSW_PCI_CIR_CTRL_OPCODE_MOD_SHIFT	12
#define MLXSW_PCI_CIR_CTRL_STATUS_SHIFT		24
#define MLXSW_PCI_CIR_TIMEOUT_MSECS		1000

#define MLXSW_PCI_SW_RESET_TIMEOUT_MSECS	900000
#define MLXSW_PCI_SW_RESET_WAIT_MSECS		400
#define MLXSW_PCI_FW_READY			0xA1844
#define MLXSW_PCI_FW_READY_MASK			0xFFFF
#define MLXSW_PCI_FW_READY_MAGIC		0x5E

#define MLXSW_PCI_DOORBELL_SDQ_OFFSET		0x000
#define MLXSW_PCI_DOORBELL_RDQ_OFFSET		0x200
#define MLXSW_PCI_DOORBELL_CQ_OFFSET		0x400
#define MLXSW_PCI_DOORBELL_EQ_OFFSET		0x600
#define MLXSW_PCI_DOORBELL_ARM_CQ_OFFSET	0x800
#define MLXSW_PCI_DOORBELL_ARM_EQ_OFFSET	0xA00

#define MLXSW_PCI_DOORBELL(offset, type_offset, num)	\
	((offset) + (type_offset) + (num) * 4)

#define MLXSW_PCI_CQS_MAX	96
#define MLXSW_PCI_EQS_COUNT	2
#define MLXSW_PCI_EQ_ASYNC_NUM	0
#define MLXSW_PCI_EQ_COMP_NUM	1

#define MLXSW_PCI_SDQS_MIN	2 
#define MLXSW_PCI_SDQ_EMAD_INDEX	0
#define MLXSW_PCI_SDQ_EMAD_TC	0
#define MLXSW_PCI_SDQ_CTL_TC	3

#define MLXSW_PCI_AQ_PAGES	8
#define MLXSW_PCI_AQ_SIZE	(MLXSW_PCI_PAGE_SIZE * MLXSW_PCI_AQ_PAGES)
#define MLXSW_PCI_WQE_SIZE	32 
#define MLXSW_PCI_CQE01_SIZE	16 
#define MLXSW_PCI_CQE2_SIZE	32 
#define MLXSW_PCI_CQE_SIZE_MAX	MLXSW_PCI_CQE2_SIZE
#define MLXSW_PCI_EQE_SIZE	16 
#define MLXSW_PCI_WQE_COUNT	(MLXSW_PCI_AQ_SIZE / MLXSW_PCI_WQE_SIZE)
#define MLXSW_PCI_CQE01_COUNT	(MLXSW_PCI_AQ_SIZE / MLXSW_PCI_CQE01_SIZE)
#define MLXSW_PCI_CQE2_COUNT	(MLXSW_PCI_AQ_SIZE / MLXSW_PCI_CQE2_SIZE)
#define MLXSW_PCI_EQE_COUNT	(MLXSW_PCI_AQ_SIZE / MLXSW_PCI_EQE_SIZE)
#define MLXSW_PCI_EQE_UPDATE_COUNT	0x80

#define MLXSW_PCI_WQE_SG_ENTRIES	3
#define MLXSW_PCI_WQE_TYPE_ETHERNET	0xA


MLXSW_ITEM32(pci, wqe, c, 0x00, 31, 1);


MLXSW_ITEM32(pci, wqe, lp, 0x00, 30, 1);


MLXSW_ITEM32(pci, wqe, type, 0x00, 23, 4);


MLXSW_ITEM16_INDEXED(pci, wqe, byte_count, 0x02, 0, 14, 0x02, 0x00, false);


MLXSW_ITEM64_INDEXED(pci, wqe, address, 0x08, 0, 64, 0x8, 0x0, false);

enum mlxsw_pci_cqe_v {
	MLXSW_PCI_CQE_V0,
	MLXSW_PCI_CQE_V1,
	MLXSW_PCI_CQE_V2,
};

#define mlxsw_pci_cqe_item_helpers(name, v0, v1, v2)				\
static inline u32 mlxsw_pci_cqe_##name##_get(enum mlxsw_pci_cqe_v v, char *cqe)	\
{										\
	switch (v) {								\
	default:								\
	case MLXSW_PCI_CQE_V0:							\
		return mlxsw_pci_cqe##v0##_##name##_get(cqe);			\
	case MLXSW_PCI_CQE_V1:							\
		return mlxsw_pci_cqe##v1##_##name##_get(cqe);			\
	case MLXSW_PCI_CQE_V2:							\
		return mlxsw_pci_cqe##v2##_##name##_get(cqe);			\
	}									\
}										\
static inline void mlxsw_pci_cqe_##name##_set(enum mlxsw_pci_cqe_v v,		\
					      char *cqe, u32 val)		\
{										\
	switch (v) {								\
	default:								\
	case MLXSW_PCI_CQE_V0:							\
		mlxsw_pci_cqe##v0##_##name##_set(cqe, val);			\
		break;								\
	case MLXSW_PCI_CQE_V1:							\
		mlxsw_pci_cqe##v1##_##name##_set(cqe, val);			\
		break;								\
	case MLXSW_PCI_CQE_V2:							\
		mlxsw_pci_cqe##v2##_##name##_set(cqe, val);			\
		break;								\
	}									\
}


MLXSW_ITEM32(pci, cqe0, lag, 0x00, 23, 1);
MLXSW_ITEM32(pci, cqe12, lag, 0x00, 24, 1);
mlxsw_pci_cqe_item_helpers(lag, 0, 12, 12);


MLXSW_ITEM32(pci, cqe, system_port, 0x00, 0, 16);
MLXSW_ITEM32(pci, cqe0, lag_id, 0x00, 4, 12);
MLXSW_ITEM32(pci, cqe12, lag_id, 0x00, 0, 16);
mlxsw_pci_cqe_item_helpers(lag_id, 0, 12, 12);
MLXSW_ITEM32(pci, cqe0, lag_subport, 0x00, 0, 4);
MLXSW_ITEM32(pci, cqe12, lag_subport, 0x00, 16, 8);
mlxsw_pci_cqe_item_helpers(lag_subport, 0, 12, 12);


MLXSW_ITEM32(pci, cqe, wqe_counter, 0x04, 16, 16);


MLXSW_ITEM32(pci, cqe, byte_count, 0x04, 0, 14);

#define MLXSW_PCI_CQE2_MIRROR_CONG_INVALID	0xFFFF


MLXSW_ITEM32(pci, cqe2, mirror_cong_high, 0x08, 16, 4);


MLXSW_ITEM32(pci, cqe, trap_id, 0x08, 0, 10);


MLXSW_ITEM32(pci, cqe0, crc, 0x0C, 8, 1);
MLXSW_ITEM32(pci, cqe12, crc, 0x0C, 9, 1);
mlxsw_pci_cqe_item_helpers(crc, 0, 12, 12);


MLXSW_ITEM32(pci, cqe0, e, 0x0C, 7, 1);
MLXSW_ITEM32(pci, cqe12, e, 0x00, 27, 1);
mlxsw_pci_cqe_item_helpers(e, 0, 12, 12);


MLXSW_ITEM32(pci, cqe0, sr, 0x0C, 6, 1);
MLXSW_ITEM32(pci, cqe12, sr, 0x00, 26, 1);
mlxsw_pci_cqe_item_helpers(sr, 0, 12, 12);


MLXSW_ITEM32(pci, cqe0, dqn, 0x0C, 1, 5);
MLXSW_ITEM32(pci, cqe12, dqn, 0x0C, 1, 6);
mlxsw_pci_cqe_item_helpers(dqn, 0, 12, 12);


MLXSW_ITEM32(pci, cqe2, time_stamp_low, 0x0C, 16, 16);

#define MLXSW_PCI_CQE2_MIRROR_TCLASS_INVALID	0x1F


MLXSW_ITEM32(pci, cqe2, mirror_tclass, 0x10, 27, 5);


MLXSW_ITEM32(pci, cqe2, tx_lag, 0x10, 24, 1);


MLXSW_ITEM32(pci, cqe2, tx_lag_subport, 0x10, 16, 8);

#define MLXSW_PCI_CQE2_TX_PORT_MULTI_PORT	0xFFFE
#define MLXSW_PCI_CQE2_TX_PORT_INVALID		0xFFFF


MLXSW_ITEM32(pci, cqe2, tx_lag_id, 0x10, 0, 16);


MLXSW_ITEM32(pci, cqe2, tx_system_port, 0x10, 0, 16);


MLXSW_ITEM32(pci, cqe2, mirror_cong_low, 0x14, 20, 12);

#define MLXSW_PCI_CQE2_MIRROR_CONG_SHIFT	13	

static inline u16 mlxsw_pci_cqe2_mirror_cong_get(const char *cqe)
{
	u16 cong_high = mlxsw_pci_cqe2_mirror_cong_high_get(cqe);
	u16 cong_low = mlxsw_pci_cqe2_mirror_cong_low_get(cqe);

	return cong_high << 12 | cong_low;
}


MLXSW_ITEM32(pci, cqe2, user_def_val_orig_pkt_len, 0x14, 0, 20);


MLXSW_ITEM32(pci, cqe2, mirror_reason, 0x18, 24, 8);

enum mlxsw_pci_cqe_time_stamp_type {
	MLXSW_PCI_CQE_TIME_STAMP_TYPE_USEC,
	MLXSW_PCI_CQE_TIME_STAMP_TYPE_FRC,
	MLXSW_PCI_CQE_TIME_STAMP_TYPE_UTC,
	MLXSW_PCI_CQE_TIME_STAMP_TYPE_MIRROR_UTC,
};


MLXSW_ITEM32(pci, cqe2, time_stamp_type, 0x18, 22, 2);

#define MLXSW_PCI_CQE2_MIRROR_LATENCY_INVALID	0xFFFFFF


MLXSW_ITEM32(pci, cqe2, time_stamp_high, 0x18, 0, 22);

static inline u64 mlxsw_pci_cqe2_time_stamp_get(const char *cqe)
{
	u64 ts_high = mlxsw_pci_cqe2_time_stamp_high_get(cqe);
	u64 ts_low = mlxsw_pci_cqe2_time_stamp_low_get(cqe);

	return ts_high << 16 | ts_low;
}

static inline u8 mlxsw_pci_cqe2_time_stamp_sec_get(const char *cqe)
{
	u64 full_ts = mlxsw_pci_cqe2_time_stamp_get(cqe);

	return full_ts >> 30 & 0xFF;
}

static inline u32 mlxsw_pci_cqe2_time_stamp_nsec_get(const char *cqe)
{
	u64 full_ts = mlxsw_pci_cqe2_time_stamp_get(cqe);

	return full_ts & 0x3FFFFFFF;
}


MLXSW_ITEM32(pci, cqe2, mirror_latency, 0x1C, 8, 24);


MLXSW_ITEM32(pci, cqe01, owner, 0x0C, 0, 1);
MLXSW_ITEM32(pci, cqe2, owner, 0x1C, 0, 1);
mlxsw_pci_cqe_item_helpers(owner, 01, 01, 2);


MLXSW_ITEM32(pci, eqe, event_type, 0x0C, 24, 8);
#define MLXSW_PCI_EQE_EVENT_TYPE_COMP	0x00
#define MLXSW_PCI_EQE_EVENT_TYPE_CMD	0x0A


MLXSW_ITEM32(pci, eqe, event_sub_type, 0x0C, 16, 8);


MLXSW_ITEM32(pci, eqe, cqn, 0x0C, 8, 7);


MLXSW_ITEM32(pci, eqe, owner, 0x0C, 0, 1);


MLXSW_ITEM32(pci, eqe, cmd_token, 0x00, 16, 16);


MLXSW_ITEM32(pci, eqe, cmd_status, 0x00, 0, 8);


MLXSW_ITEM32(pci, eqe, cmd_out_param_h, 0x04, 0, 32);


MLXSW_ITEM32(pci, eqe, cmd_out_param_l, 0x08, 0, 32);

#endif
