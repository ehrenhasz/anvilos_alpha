
 

#include <linux/types.h>
#include <linux/crc8.h>
#include <linux/delay.h>
#include <linux/kernel.h>
#include <linux/slab.h>
#include <linux/string.h>
#include "qed_hsi.h"
#include "qed_hw.h"
#include "qed_init_ops.h"
#include "qed_iro_hsi.h"
#include "qed_reg_addr.h"

#define CDU_VALIDATION_DEFAULT_CFG CDU_CONTEXT_VALIDATION_DEFAULT_CFG

static u16 con_region_offsets[3][NUM_OF_CONNECTION_TYPES] = {
	{400, 336, 352, 368, 304, 384, 416, 352},	 
	{528, 496, 416, 512, 448, 512, 544, 480},	 
	{608, 544, 496, 576, 576, 592, 624, 560}	 
};

static u16 task_region_offsets[1][NUM_OF_CONNECTION_TYPES] = {
	{240, 240, 112, 0, 0, 0, 0, 96}	 
};

 
#define QM_PQ_MEM_4KB(pq_size)	(pq_size ? DIV_ROUND_UP((pq_size + 1) *	\
							QM_PQ_ELEMENT_SIZE, \
							0x1000) : 0)
#define QM_PQ_SIZE_256B(pq_size)	(pq_size ? DIV_ROUND_UP(pq_size, \
								0x100) - 1 : 0)
#define QM_INVALID_PQ_ID		0xffff

 
#define QM_MAX_LINK_SPEED               100000

 
#define QM_BYPASS_EN	1
#define QM_BYTE_CRD_EN	1

 
#define QM_INITIAL_VOQ_BYTE_CRD         98304
 
#define QM_OTHER_PQS_PER_PF	4

 
#define MAX_NUM_VOQS	(MAX_NUM_PORTS_K2 * NUM_TCS_4PORT_K2)
#define VOQS_BIT_MASK	(BIT(MAX_NUM_VOQS) - 1)

 

 
#define QM_PF_WFQ_INC_VAL(weight)       ((weight) * 0x9000)

 
#define QM_PF_WFQ_UPPER_BOUND           62500000

 
#define QM_PF_WFQ_MAX_INC_VAL           ((QM_PF_WFQ_UPPER_BOUND * 7) / 10)

 
#define QM_PF_WFQ_CRD_E5_NUM_VOQS       16

 
#define QM_VP_WFQ_INC_VAL(weight)       ((weight) * QM_VP_WFQ_MIN_INC_VAL)

 
#define QM_VP_WFQ_MIN_INC_VAL           10800

 
#define QM_VP_WFQ_MAX_INC_VAL           0x40000000

 
#define QM_VP_WFQ_BYPASS_THRESH         (QM_VP_WFQ_MIN_INC_VAL - 100)

 
#define QM_VP_RL_CRD_TASK_COST          9700

 
#define QM_VP_WFQ_PQ_VOQ_SHIFT          0

 
#define QM_VP_WFQ_PQ_PF_SHIFT   5

 

 
#define QM_RL_PERIOD	5

 
#define QM_RL_PERIOD_CLK_25M	(25 * QM_RL_PERIOD)

 
#define QM_RL_INC_VAL(rate)                     ({	\
						typeof(rate) __rate = (rate); \
						max_t(u32,		\
						(u32)(((__rate ? __rate : \
						100000) *		\
						QM_RL_PERIOD *		\
						101) / (8 * 100)), 1); })

 
#define QM_PF_RL_UPPER_BOUND	62500000

 
#define QM_PF_RL_MAX_INC_VAL	((QM_PF_RL_UPPER_BOUND * 7) / 10)

 
#define QM_GLOBAL_RL_UPPER_BOUND(speed)         ((u32)max_t( \
		u32,					    \
		(u32)(((speed) *			    \
		       QM_RL_PERIOD * 101) / (8 * 100)),    \
		QM_VP_RL_CRD_TASK_COST			    \
		+ 1000))

 
#define QM_OPPOR_LINE_VOQ_DEF	1
#define QM_OPPOR_FW_STOP_DEF	0
#define QM_OPPOR_PQ_EMPTY_DEF	1

 

 
#define PBF_CMDQ_PURE_LB_LINES	150

#define PBF_CMDQ_LINES_RT_OFFSET(ext_voq) \
	(PBF_REG_YCMD_QS_NUM_LINES_VOQ0_RT_OFFSET + \
	 (ext_voq) * (PBF_REG_YCMD_QS_NUM_LINES_VOQ1_RT_OFFSET - \
		PBF_REG_YCMD_QS_NUM_LINES_VOQ0_RT_OFFSET))

#define PBF_BTB_GUARANTEED_RT_OFFSET(ext_voq) \
	(PBF_REG_BTB_GUARANTEED_VOQ0_RT_OFFSET + \
	 (ext_voq) * (PBF_REG_BTB_GUARANTEED_VOQ1_RT_OFFSET - \
		PBF_REG_BTB_GUARANTEED_VOQ0_RT_OFFSET))

 
#define QM_VOQ_LINE_CRD(pbf_cmd_lines) \
	((((pbf_cmd_lines) - 4) * 2) | QM_LINE_CRD_REG_SIGN_BIT)

 

 
#define BTB_JUMBO_PKT_BLOCKS	38

 
#define BTB_HEADROOM_BLOCKS	BTB_JUMBO_PKT_BLOCKS
#define BTB_PURE_LB_FACTOR	10

 
#define BTB_PURE_LB_RATIO	7

 
#define QM_STOP_PQ_MASK_WIDTH		32
#define QM_STOP_CMD_ADDR		2
#define QM_STOP_CMD_STRUCT_SIZE		2
#define QM_STOP_CMD_PAUSE_MASK_OFFSET	0
#define QM_STOP_CMD_PAUSE_MASK_SHIFT	0
#define QM_STOP_CMD_PAUSE_MASK_MASK	-1
#define QM_STOP_CMD_GROUP_ID_OFFSET	1
#define QM_STOP_CMD_GROUP_ID_SHIFT	16
#define QM_STOP_CMD_GROUP_ID_MASK	15
#define QM_STOP_CMD_PQ_TYPE_OFFSET	1
#define QM_STOP_CMD_PQ_TYPE_SHIFT	24
#define QM_STOP_CMD_PQ_TYPE_MASK	1
#define QM_STOP_CMD_MAX_POLL_COUNT	100
#define QM_STOP_CMD_POLL_PERIOD_US	500

 
#define QM_CMD_STRUCT_SIZE(cmd)	cmd ## _STRUCT_SIZE
#define QM_CMD_SET_FIELD(var, cmd, field, value) \
	SET_FIELD(var[cmd ## _ ## field ## _OFFSET], \
		  cmd ## _ ## field, \
		  value)

#define QM_INIT_TX_PQ_MAP(p_hwfn, map, pq_id, vp_pq_id, rl_valid,	      \
			  rl_id, ext_voq, wrr)				      \
	do {								      \
		u32 __reg = 0;						      \
									      \
		BUILD_BUG_ON(sizeof((map).reg) != sizeof(__reg));	      \
		memset(&(map), 0, sizeof(map));				      \
		SET_FIELD(__reg, QM_RF_PQ_MAP_PQ_VALID, 1);	      \
		SET_FIELD(__reg, QM_RF_PQ_MAP_RL_VALID,	      \
			  !!(rl_valid));				      \
		SET_FIELD(__reg, QM_RF_PQ_MAP_VP_PQ_ID, (vp_pq_id)); \
		SET_FIELD(__reg, QM_RF_PQ_MAP_RL_ID, (rl_id));	      \
		SET_FIELD(__reg, QM_RF_PQ_MAP_VOQ, (ext_voq));	      \
		SET_FIELD(__reg, QM_RF_PQ_MAP_WRR_WEIGHT_GROUP,      \
			  (wrr));					      \
									      \
		STORE_RT_REG((p_hwfn), QM_REG_TXPQMAP_RT_OFFSET + (pq_id),    \
			     __reg);					      \
		(map).reg = cpu_to_le32(__reg);				      \
	} while (0)

#define WRITE_PQ_INFO_TO_RAM	1
#define PQ_INFO_ELEMENT(vp, pf, tc, port, rl_valid, rl) \
	(((vp) << 0) | ((pf) << 12) | ((tc) << 16) | ((port) << 20) | \
	((rl_valid ? 1 : 0) << 22) | (((rl) & 255) << 24) | \
	(((rl) >> 8) << 9))

#define PQ_INFO_RAM_GRC_ADDRESS(pq_id) \
	(XSEM_REG_FAST_MEMORY + SEM_FAST_REG_INT_RAM + \
	XSTORM_PQ_INFO_OFFSET(pq_id))

static const char * const s_protocol_types[] = {
	"PROTOCOLID_ISCSI", "PROTOCOLID_FCOE", "PROTOCOLID_ROCE",
	"PROTOCOLID_CORE", "PROTOCOLID_ETH", "PROTOCOLID_IWARP",
	"PROTOCOLID_TOE", "PROTOCOLID_PREROCE", "PROTOCOLID_COMMON",
	"PROTOCOLID_TCP", "PROTOCOLID_RDMA", "PROTOCOLID_SCSI",
};

static const char *s_ramrod_cmd_ids[][28] = {
	{
	"ISCSI_RAMROD_CMD_ID_UNUSED", "ISCSI_RAMROD_CMD_ID_INIT_FUNC",
	 "ISCSI_RAMROD_CMD_ID_DESTROY_FUNC",
	 "ISCSI_RAMROD_CMD_ID_OFFLOAD_CONN",
	 "ISCSI_RAMROD_CMD_ID_UPDATE_CONN",
	 "ISCSI_RAMROD_CMD_ID_TERMINATION_CONN",
	 "ISCSI_RAMROD_CMD_ID_CLEAR_SQ", "ISCSI_RAMROD_CMD_ID_MAC_UPDATE",
	 "ISCSI_RAMROD_CMD_ID_CONN_STATS", },
	{ "FCOE_RAMROD_CMD_ID_INIT_FUNC", "FCOE_RAMROD_CMD_ID_DESTROY_FUNC",
	 "FCOE_RAMROD_CMD_ID_STAT_FUNC",
	 "FCOE_RAMROD_CMD_ID_OFFLOAD_CONN",
	 "FCOE_RAMROD_CMD_ID_TERMINATE_CONN", },
	{ "RDMA_RAMROD_UNUSED", "RDMA_RAMROD_FUNC_INIT",
	 "RDMA_RAMROD_FUNC_CLOSE", "RDMA_RAMROD_REGISTER_MR",
	 "RDMA_RAMROD_DEREGISTER_MR", "RDMA_RAMROD_CREATE_CQ",
	 "RDMA_RAMROD_RESIZE_CQ", "RDMA_RAMROD_DESTROY_CQ",
	 "RDMA_RAMROD_CREATE_SRQ", "RDMA_RAMROD_MODIFY_SRQ",
	 "RDMA_RAMROD_DESTROY_SRQ", "RDMA_RAMROD_START_NS_TRACKING",
	 "RDMA_RAMROD_STOP_NS_TRACKING", "ROCE_RAMROD_CREATE_QP",
	 "ROCE_RAMROD_MODIFY_QP", "ROCE_RAMROD_QUERY_QP",
	 "ROCE_RAMROD_DESTROY_QP", "ROCE_RAMROD_CREATE_UD_QP",
	 "ROCE_RAMROD_DESTROY_UD_QP", "ROCE_RAMROD_FUNC_UPDATE",
	 "ROCE_RAMROD_SUSPEND_QP", "ROCE_RAMROD_QUERY_SUSPENDED_QP",
	 "ROCE_RAMROD_CREATE_SUSPENDED_QP", "ROCE_RAMROD_RESUME_QP",
	 "ROCE_RAMROD_SUSPEND_UD_QP", "ROCE_RAMROD_RESUME_UD_QP",
	 "ROCE_RAMROD_CREATE_SUSPENDED_UD_QP", "ROCE_RAMROD_FLUSH_DPT_QP", },
	{ "CORE_RAMROD_UNUSED", "CORE_RAMROD_RX_QUEUE_START",
	 "CORE_RAMROD_TX_QUEUE_START", "CORE_RAMROD_RX_QUEUE_STOP",
	 "CORE_RAMROD_TX_QUEUE_STOP",
	 "CORE_RAMROD_RX_QUEUE_FLUSH",
	 "CORE_RAMROD_TX_QUEUE_UPDATE", "CORE_RAMROD_QUEUE_STATS_QUERY", },
	{ "ETH_RAMROD_UNUSED", "ETH_RAMROD_VPORT_START",
	 "ETH_RAMROD_VPORT_UPDATE", "ETH_RAMROD_VPORT_STOP",
	 "ETH_RAMROD_RX_QUEUE_START", "ETH_RAMROD_RX_QUEUE_STOP",
	 "ETH_RAMROD_TX_QUEUE_START", "ETH_RAMROD_TX_QUEUE_STOP",
	 "ETH_RAMROD_FILTERS_UPDATE", "ETH_RAMROD_RX_QUEUE_UPDATE",
	 "ETH_RAMROD_RX_CREATE_OPENFLOW_ACTION",
	 "ETH_RAMROD_RX_ADD_OPENFLOW_FILTER",
	 "ETH_RAMROD_RX_DELETE_OPENFLOW_FILTER",
	 "ETH_RAMROD_RX_ADD_UDP_FILTER",
	 "ETH_RAMROD_RX_DELETE_UDP_FILTER",
	 "ETH_RAMROD_RX_CREATE_GFT_ACTION",
	 "ETH_RAMROD_RX_UPDATE_GFT_FILTER", "ETH_RAMROD_TX_QUEUE_UPDATE",
	 "ETH_RAMROD_RGFS_FILTER_ADD", "ETH_RAMROD_RGFS_FILTER_DEL",
	 "ETH_RAMROD_TGFS_FILTER_ADD", "ETH_RAMROD_TGFS_FILTER_DEL",
	 "ETH_RAMROD_GFS_COUNTERS_REPORT_REQUEST", },
	{ "RDMA_RAMROD_UNUSED", "RDMA_RAMROD_FUNC_INIT",
	 "RDMA_RAMROD_FUNC_CLOSE", "RDMA_RAMROD_REGISTER_MR",
	 "RDMA_RAMROD_DEREGISTER_MR", "RDMA_RAMROD_CREATE_CQ",
	 "RDMA_RAMROD_RESIZE_CQ", "RDMA_RAMROD_DESTROY_CQ",
	 "RDMA_RAMROD_CREATE_SRQ", "RDMA_RAMROD_MODIFY_SRQ",
	 "RDMA_RAMROD_DESTROY_SRQ", "RDMA_RAMROD_START_NS_TRACKING",
	 "RDMA_RAMROD_STOP_NS_TRACKING",
	 "IWARP_RAMROD_CMD_ID_TCP_OFFLOAD",
	 "IWARP_RAMROD_CMD_ID_MPA_OFFLOAD",
	 "IWARP_RAMROD_CMD_ID_MPA_OFFLOAD_SEND_RTR",
	 "IWARP_RAMROD_CMD_ID_CREATE_QP", "IWARP_RAMROD_CMD_ID_QUERY_QP",
	 "IWARP_RAMROD_CMD_ID_MODIFY_QP",
	 "IWARP_RAMROD_CMD_ID_DESTROY_QP",
	 "IWARP_RAMROD_CMD_ID_ABORT_TCP_OFFLOAD", },
	{ NULL },  
	{ NULL },  
	{ "COMMON_RAMROD_UNUSED", "COMMON_RAMROD_PF_START",
	     "COMMON_RAMROD_PF_STOP", "COMMON_RAMROD_VF_START",
	     "COMMON_RAMROD_VF_STOP", "COMMON_RAMROD_PF_UPDATE",
	     "COMMON_RAMROD_RL_UPDATE", "COMMON_RAMROD_EMPTY", }
};

 

 
static u8 qed_get_ext_voq(struct qed_hwfn *p_hwfn,
			  u8 port_id, u8 tc, u8 max_phys_tcs_per_port)
{
	if (tc == PURE_LB_TC)
		return NUM_OF_PHYS_TCS * MAX_NUM_PORTS_BB + port_id;
	else
		return port_id * max_phys_tcs_per_port + tc;
}

 
static void qed_enable_pf_rl(struct qed_hwfn *p_hwfn, bool pf_rl_en)
{
	STORE_RT_REG(p_hwfn, QM_REG_RLPFENABLE_RT_OFFSET, pf_rl_en ? 1 : 0);
	if (pf_rl_en) {
		u8 num_ext_voqs = MAX_NUM_VOQS;
		u64 voq_bit_mask = ((u64)1 << num_ext_voqs) - 1;

		 
		STORE_RT_REG(p_hwfn,
			     QM_REG_RLPFVOQENABLE_RT_OFFSET,
			     (u32)voq_bit_mask);

		 
		STORE_RT_REG(p_hwfn,
			     QM_REG_RLPFPERIOD_RT_OFFSET, QM_RL_PERIOD_CLK_25M);
		STORE_RT_REG(p_hwfn,
			     QM_REG_RLPFPERIODTIMER_RT_OFFSET,
			     QM_RL_PERIOD_CLK_25M);

		 
		if (QM_BYPASS_EN)
			STORE_RT_REG(p_hwfn,
				     QM_REG_AFULLQMBYPTHRPFRL_RT_OFFSET,
				     QM_PF_RL_UPPER_BOUND);
	}
}

 
static void qed_enable_pf_wfq(struct qed_hwfn *p_hwfn, bool pf_wfq_en)
{
	STORE_RT_REG(p_hwfn, QM_REG_WFQPFENABLE_RT_OFFSET, pf_wfq_en ? 1 : 0);

	 
	if (pf_wfq_en && QM_BYPASS_EN)
		STORE_RT_REG(p_hwfn,
			     QM_REG_AFULLQMBYPTHRPFWFQ_RT_OFFSET,
			     QM_PF_WFQ_UPPER_BOUND);
}

 
static void qed_enable_global_rl(struct qed_hwfn *p_hwfn, bool global_rl_en)
{
	STORE_RT_REG(p_hwfn, QM_REG_RLGLBLENABLE_RT_OFFSET,
		     global_rl_en ? 1 : 0);
	if (global_rl_en) {
		 
		STORE_RT_REG(p_hwfn,
			     QM_REG_RLGLBLPERIOD_0_RT_OFFSET,
			     QM_RL_PERIOD_CLK_25M);
		STORE_RT_REG(p_hwfn,
			     QM_REG_RLGLBLPERIODTIMER_0_RT_OFFSET,
			     QM_RL_PERIOD_CLK_25M);

		 
		if (QM_BYPASS_EN)
			STORE_RT_REG(p_hwfn,
				     QM_REG_AFULLQMBYPTHRGLBLRL_RT_OFFSET,
				     QM_GLOBAL_RL_UPPER_BOUND(10000) - 1);
	}
}

 
static void qed_enable_vport_wfq(struct qed_hwfn *p_hwfn, bool vport_wfq_en)
{
	STORE_RT_REG(p_hwfn, QM_REG_WFQVPENABLE_RT_OFFSET,
		     vport_wfq_en ? 1 : 0);

	 
	if (vport_wfq_en && QM_BYPASS_EN)
		STORE_RT_REG(p_hwfn,
			     QM_REG_AFULLQMBYPTHRVPWFQ_RT_OFFSET,
			     QM_VP_WFQ_BYPASS_THRESH);
}

 
static void qed_cmdq_lines_voq_rt_init(struct qed_hwfn *p_hwfn,
				       u8 ext_voq, u16 cmdq_lines)
{
	u32 qm_line_crd = QM_VOQ_LINE_CRD(cmdq_lines);

	OVERWRITE_RT_REG(p_hwfn, PBF_CMDQ_LINES_RT_OFFSET(ext_voq),
			 (u32)cmdq_lines);
	STORE_RT_REG(p_hwfn, QM_REG_VOQCRDLINE_RT_OFFSET + ext_voq,
		     qm_line_crd);
	STORE_RT_REG(p_hwfn, QM_REG_VOQINITCRDLINE_RT_OFFSET + ext_voq,
		     qm_line_crd);
}

 
static void
qed_cmdq_lines_rt_init(struct qed_hwfn *p_hwfn,
		       u8 max_ports_per_engine,
		       u8 max_phys_tcs_per_port,
		       struct init_qm_port_params port_params[MAX_NUM_PORTS])
{
	u8 tc, ext_voq, port_id, num_tcs_in_port;
	u8 num_ext_voqs = MAX_NUM_VOQS;

	 
	for (ext_voq = 0; ext_voq < num_ext_voqs; ext_voq++)
		STORE_RT_REG(p_hwfn, PBF_CMDQ_LINES_RT_OFFSET(ext_voq), 0);

	for (port_id = 0; port_id < max_ports_per_engine; port_id++) {
		u16 phys_lines, phys_lines_per_tc;

		if (!port_params[port_id].active)
			continue;

		 
		phys_lines = port_params[port_id].num_pbf_cmd_lines;
		phys_lines -= PBF_CMDQ_PURE_LB_LINES;

		 
		num_tcs_in_port = 0;
		for (tc = 0; tc < max_phys_tcs_per_port; tc++)
			if (((port_params[port_id].active_phys_tcs >>
			      tc) & 0x1) == 1)
				num_tcs_in_port++;
		phys_lines_per_tc = phys_lines / num_tcs_in_port;

		 
		for (tc = 0; tc < max_phys_tcs_per_port; tc++) {
			ext_voq = qed_get_ext_voq(p_hwfn,
						  port_id,
						  tc, max_phys_tcs_per_port);
			if (((port_params[port_id].active_phys_tcs >>
			      tc) & 0x1) == 1)
				qed_cmdq_lines_voq_rt_init(p_hwfn,
							   ext_voq,
							   phys_lines_per_tc);
		}

		 
		ext_voq = qed_get_ext_voq(p_hwfn,
					  port_id,
					  PURE_LB_TC, max_phys_tcs_per_port);
		qed_cmdq_lines_voq_rt_init(p_hwfn, ext_voq,
					   PBF_CMDQ_PURE_LB_LINES);
	}
}

 
static void
qed_btb_blocks_rt_init(struct qed_hwfn *p_hwfn,
		       u8 max_ports_per_engine,
		       u8 max_phys_tcs_per_port,
		       struct init_qm_port_params port_params[MAX_NUM_PORTS])
{
	u32 usable_blocks, pure_lb_blocks, phys_blocks;
	u8 tc, ext_voq, port_id, num_tcs_in_port;

	for (port_id = 0; port_id < max_ports_per_engine; port_id++) {
		if (!port_params[port_id].active)
			continue;

		 
		usable_blocks = port_params[port_id].num_btb_blocks -
				BTB_HEADROOM_BLOCKS;

		 
		num_tcs_in_port = 0;
		for (tc = 0; tc < NUM_OF_PHYS_TCS; tc++)
			if (((port_params[port_id].active_phys_tcs >>
			      tc) & 0x1) == 1)
				num_tcs_in_port++;

		pure_lb_blocks = (usable_blocks * BTB_PURE_LB_FACTOR) /
				 (num_tcs_in_port * BTB_PURE_LB_FACTOR +
				  BTB_PURE_LB_RATIO);
		pure_lb_blocks = max_t(u32, BTB_JUMBO_PKT_BLOCKS,
				       pure_lb_blocks / BTB_PURE_LB_FACTOR);
		phys_blocks = (usable_blocks - pure_lb_blocks) /
			      num_tcs_in_port;

		 
		for (tc = 0; tc < NUM_OF_PHYS_TCS; tc++) {
			if (((port_params[port_id].active_phys_tcs >>
			      tc) & 0x1) == 1) {
				ext_voq =
					qed_get_ext_voq(p_hwfn,
							port_id,
							tc,
							max_phys_tcs_per_port);
				STORE_RT_REG(p_hwfn,
					     PBF_BTB_GUARANTEED_RT_OFFSET
					     (ext_voq), phys_blocks);
			}
		}

		 
		ext_voq = qed_get_ext_voq(p_hwfn,
					  port_id,
					  PURE_LB_TC, max_phys_tcs_per_port);
		STORE_RT_REG(p_hwfn, PBF_BTB_GUARANTEED_RT_OFFSET(ext_voq),
			     pure_lb_blocks);
	}
}

 
static int qed_global_rl_rt_init(struct qed_hwfn *p_hwfn)
{
	u32 upper_bound = QM_GLOBAL_RL_UPPER_BOUND(QM_MAX_LINK_SPEED) |
			  (u32)QM_RL_CRD_REG_SIGN_BIT;
	u32 inc_val;
	u16 rl_id;

	 
	for (rl_id = 0; rl_id < MAX_QM_GLOBAL_RLS; rl_id++) {
		inc_val = QM_RL_INC_VAL(QM_MAX_LINK_SPEED);

		STORE_RT_REG(p_hwfn,
			     QM_REG_RLGLBLCRD_RT_OFFSET + rl_id,
			     (u32)QM_RL_CRD_REG_SIGN_BIT);
		STORE_RT_REG(p_hwfn,
			     QM_REG_RLGLBLUPPERBOUND_RT_OFFSET + rl_id,
			     upper_bound);
		STORE_RT_REG(p_hwfn,
			     QM_REG_RLGLBLINCVAL_RT_OFFSET + rl_id, inc_val);
	}

	return 0;
}

 
static u32 qed_get_vport_rl_upper_bound(enum init_qm_rl_type vport_rl_type,
					u32 link_speed)
{
	switch (vport_rl_type) {
	case QM_RL_TYPE_NORMAL:
		return QM_INITIAL_VOQ_BYTE_CRD;
	case QM_RL_TYPE_QCN:
		return QM_GLOBAL_RL_UPPER_BOUND(link_speed);
	default:
		return 0;
	}
}

 
static int qed_vport_rl_rt_init(struct qed_hwfn *p_hwfn,
				u16 start_rl,
				u16 num_rls,
				u32 link_speed,
				struct init_qm_rl_params *rl_params)
{
	u16 i, rl_id;

	if (num_rls && start_rl + num_rls >= MAX_QM_GLOBAL_RLS) {
		DP_NOTICE(p_hwfn, "Invalid rate limiter configuration\n");
		return -1;
	}

	 
	for (i = 0, rl_id = start_rl; i < num_rls; i++, rl_id++) {
		u32 upper_bound, inc_val;

		upper_bound =
		    qed_get_vport_rl_upper_bound((enum init_qm_rl_type)
						 rl_params[i].vport_rl_type,
						 link_speed);

		inc_val =
		    QM_RL_INC_VAL(rl_params[i].vport_rl ?
				  rl_params[i].vport_rl : link_speed);
		if (inc_val > upper_bound) {
			DP_NOTICE(p_hwfn,
				  "Invalid RL rate - limit configuration\n");
			return -1;
		}

		STORE_RT_REG(p_hwfn, QM_REG_RLGLBLCRD_RT_OFFSET + rl_id,
			     (u32)QM_RL_CRD_REG_SIGN_BIT);
		STORE_RT_REG(p_hwfn, QM_REG_RLGLBLUPPERBOUND_RT_OFFSET + rl_id,
			     upper_bound | (u32)QM_RL_CRD_REG_SIGN_BIT);
		STORE_RT_REG(p_hwfn, QM_REG_RLGLBLINCVAL_RT_OFFSET + rl_id,
			     inc_val);
	}

	return 0;
}

 
static int qed_tx_pq_map_rt_init(struct qed_hwfn *p_hwfn,
				 struct qed_ptt *p_ptt,
				 struct qed_qm_pf_rt_init_params *p_params,
				 u32 base_mem_addr_4kb)
{
	u32 tx_pq_vf_mask[MAX_QM_TX_QUEUES / QM_PF_QUEUE_GROUP_SIZE] = { 0 };
	struct init_qm_vport_params *vport_params = p_params->vport_params;
	u32 num_tx_pq_vf_masks = MAX_QM_TX_QUEUES / QM_PF_QUEUE_GROUP_SIZE;
	u16 num_pqs, first_pq_group, last_pq_group, i, j, pq_id, pq_group;
	struct init_qm_pq_params *pq_params = p_params->pq_params;
	u32 pq_mem_4kb, vport_pq_mem_4kb, mem_addr_4kb;

	num_pqs = p_params->num_pf_pqs + p_params->num_vf_pqs;

	first_pq_group = p_params->start_pq / QM_PF_QUEUE_GROUP_SIZE;
	last_pq_group = (p_params->start_pq + num_pqs - 1) /
			QM_PF_QUEUE_GROUP_SIZE;

	pq_mem_4kb = QM_PQ_MEM_4KB(p_params->num_pf_cids);
	vport_pq_mem_4kb = QM_PQ_MEM_4KB(p_params->num_vf_cids);
	mem_addr_4kb = base_mem_addr_4kb;

	 
	for (pq_group = first_pq_group; pq_group <= last_pq_group; pq_group++)
		STORE_RT_REG(p_hwfn, QM_REG_PQTX2PF_0_RT_OFFSET + pq_group,
			     (u32)(p_params->pf_id));

	 
	STORE_RT_REG(p_hwfn, QM_REG_MAXPQSIZE_0_RT_OFFSET,
		     QM_PQ_SIZE_256B(p_params->num_pf_cids));
	STORE_RT_REG(p_hwfn, QM_REG_MAXPQSIZE_1_RT_OFFSET,
		     QM_PQ_SIZE_256B(p_params->num_vf_cids));

	 
	for (i = 0, pq_id = p_params->start_pq; i < num_pqs; i++, pq_id++) {
		u16 *p_first_tx_pq_id, vport_id_in_pf;
		struct qm_rf_pq_map tx_pq_map;
		u8 tc_id = pq_params[i].tc_id;
		bool is_vf_pq;
		u8 ext_voq;

		ext_voq = qed_get_ext_voq(p_hwfn,
					  pq_params[i].port_id,
					  tc_id,
					  p_params->max_phys_tcs_per_port);
		is_vf_pq = (i >= p_params->num_pf_pqs);

		 
		vport_id_in_pf = pq_params[i].vport_id - p_params->start_vport;
		p_first_tx_pq_id =
		    &vport_params[vport_id_in_pf].first_tx_pq_id[tc_id];
		if (*p_first_tx_pq_id == QM_INVALID_PQ_ID) {
			u32 map_val =
				(ext_voq << QM_VP_WFQ_PQ_VOQ_SHIFT) |
				(p_params->pf_id << QM_VP_WFQ_PQ_PF_SHIFT);

			 
			*p_first_tx_pq_id = pq_id;

			 
			STORE_RT_REG(p_hwfn,
				     QM_REG_WFQVPMAP_RT_OFFSET +
				     *p_first_tx_pq_id,
				     map_val);
		}

		 
		QM_INIT_TX_PQ_MAP(p_hwfn,
				  tx_pq_map,
				  pq_id,
				  *p_first_tx_pq_id,
				  pq_params[i].rl_valid,
				  pq_params[i].rl_id,
				  ext_voq, pq_params[i].wrr_group);

		 
		STORE_RT_REG(p_hwfn,
			     QM_REG_BASEADDRTXPQ_RT_OFFSET + pq_id,
			     mem_addr_4kb);

		 
		if (p_params->is_pf_loading)
			for (j = 0; j < 2; j++)
				STORE_RT_REG(p_hwfn,
					     QM_REG_PTRTBLTX_RT_OFFSET +
					     (pq_id * 2) + j, 0);

		 
		if (WRITE_PQ_INFO_TO_RAM != 0) {
			u32 pq_info = 0;

			pq_info = PQ_INFO_ELEMENT(*p_first_tx_pq_id,
						  p_params->pf_id,
						  tc_id,
						  pq_params[i].port_id,
						  pq_params[i].rl_valid,
						  pq_params[i].rl_id);
			qed_wr(p_hwfn, p_ptt, PQ_INFO_RAM_GRC_ADDRESS(pq_id),
			       pq_info);
		}

		 
		if (is_vf_pq) {
			tx_pq_vf_mask[pq_id /
				      QM_PF_QUEUE_GROUP_SIZE] |=
			    BIT((pq_id % QM_PF_QUEUE_GROUP_SIZE));
			mem_addr_4kb += vport_pq_mem_4kb;
		} else {
			mem_addr_4kb += pq_mem_4kb;
		}
	}

	 
	for (i = 0; i < num_tx_pq_vf_masks; i++)
		if (tx_pq_vf_mask[i])
			STORE_RT_REG(p_hwfn,
				     QM_REG_MAXPQSIZETXSEL_0_RT_OFFSET + i,
				     tx_pq_vf_mask[i]);

	return 0;
}

 
static void qed_other_pq_map_rt_init(struct qed_hwfn *p_hwfn,
				     u8 pf_id,
				     bool is_pf_loading,
				     u32 num_pf_cids,
				     u32 num_tids, u32 base_mem_addr_4kb)
{
	u32 pq_size, pq_mem_4kb, mem_addr_4kb;
	u16 i, j, pq_id, pq_group;

	 
	pq_group = pf_id;
	pq_size = num_pf_cids + num_tids;
	pq_mem_4kb = QM_PQ_MEM_4KB(pq_size);
	mem_addr_4kb = base_mem_addr_4kb;

	 
	STORE_RT_REG(p_hwfn, QM_REG_PQOTHER2PF_0_RT_OFFSET + pq_group,
		     (u32)(pf_id));

	 
	STORE_RT_REG(p_hwfn, QM_REG_MAXPQSIZE_2_RT_OFFSET,
		     QM_PQ_SIZE_256B(pq_size));

	for (i = 0, pq_id = pf_id * QM_PF_QUEUE_GROUP_SIZE;
	     i < QM_OTHER_PQS_PER_PF; i++, pq_id++) {
		 
		STORE_RT_REG(p_hwfn,
			     QM_REG_BASEADDROTHERPQ_RT_OFFSET + pq_id,
			     mem_addr_4kb);

		 
		if (is_pf_loading)
			for (j = 0; j < 2; j++)
				STORE_RT_REG(p_hwfn,
					     QM_REG_PTRTBLOTHER_RT_OFFSET +
					     (pq_id * 2) + j, 0);

		mem_addr_4kb += pq_mem_4kb;
	}
}

 
static int qed_pf_wfq_rt_init(struct qed_hwfn *p_hwfn,
			      struct qed_qm_pf_rt_init_params *p_params)
{
	u16 num_tx_pqs = p_params->num_pf_pqs + p_params->num_vf_pqs;
	struct init_qm_pq_params *pq_params = p_params->pq_params;
	u32 inc_val, crd_reg_offset;
	u8 ext_voq;
	u16 i;

	inc_val = QM_PF_WFQ_INC_VAL(p_params->pf_wfq);
	if (!inc_val || inc_val > QM_PF_WFQ_MAX_INC_VAL) {
		DP_NOTICE(p_hwfn, "Invalid PF WFQ weight configuration\n");
		return -1;
	}

	for (i = 0; i < num_tx_pqs; i++) {
		ext_voq = qed_get_ext_voq(p_hwfn,
					  pq_params[i].port_id,
					  pq_params[i].tc_id,
					  p_params->max_phys_tcs_per_port);
		crd_reg_offset =
			(p_params->pf_id < MAX_NUM_PFS_BB ?
			 QM_REG_WFQPFCRD_RT_OFFSET :
			 QM_REG_WFQPFCRD_MSB_RT_OFFSET) +
			ext_voq * MAX_NUM_PFS_BB +
			(p_params->pf_id % MAX_NUM_PFS_BB);
		OVERWRITE_RT_REG(p_hwfn,
				 crd_reg_offset, (u32)QM_WFQ_CRD_REG_SIGN_BIT);
	}

	STORE_RT_REG(p_hwfn,
		     QM_REG_WFQPFUPPERBOUND_RT_OFFSET + p_params->pf_id,
		     QM_PF_WFQ_UPPER_BOUND | (u32)QM_WFQ_CRD_REG_SIGN_BIT);
	STORE_RT_REG(p_hwfn, QM_REG_WFQPFWEIGHT_RT_OFFSET + p_params->pf_id,
		     inc_val);

	return 0;
}

 
static int qed_pf_rl_rt_init(struct qed_hwfn *p_hwfn, u8 pf_id, u32 pf_rl)
{
	u32 inc_val = QM_RL_INC_VAL(pf_rl);

	if (inc_val > QM_PF_RL_MAX_INC_VAL) {
		DP_NOTICE(p_hwfn, "Invalid PF rate limit configuration\n");
		return -1;
	}

	STORE_RT_REG(p_hwfn,
		     QM_REG_RLPFCRD_RT_OFFSET + pf_id,
		     (u32)QM_RL_CRD_REG_SIGN_BIT);
	STORE_RT_REG(p_hwfn,
		     QM_REG_RLPFUPPERBOUND_RT_OFFSET + pf_id,
		     QM_PF_RL_UPPER_BOUND | (u32)QM_RL_CRD_REG_SIGN_BIT);
	STORE_RT_REG(p_hwfn, QM_REG_RLPFINCVAL_RT_OFFSET + pf_id, inc_val);

	return 0;
}

 
static int qed_vp_wfq_rt_init(struct qed_hwfn *p_hwfn,
			      u16 num_vports,
			      struct init_qm_vport_params *vport_params)
{
	u16 vport_pq_id, wfq, i;
	u32 inc_val;
	u8 tc;

	 
	for (i = 0; i < num_vports; i++) {
		 
		for (tc = 0; tc < NUM_OF_TCS; tc++) {
			 
			vport_pq_id = vport_params[i].first_tx_pq_id[tc];
			if (vport_pq_id == QM_INVALID_PQ_ID)
				continue;

			 
			wfq = vport_params[i].wfq;
			wfq = wfq ? wfq : vport_params[i].tc_wfq[tc];
			inc_val = QM_VP_WFQ_INC_VAL(wfq);
			if (inc_val > QM_VP_WFQ_MAX_INC_VAL) {
				DP_NOTICE(p_hwfn,
					  "Invalid VPORT WFQ weight configuration\n");
				return -1;
			}

			 
			STORE_RT_REG(p_hwfn, QM_REG_WFQVPCRD_RT_OFFSET +
				     vport_pq_id,
				     (u32)QM_WFQ_CRD_REG_SIGN_BIT);
			STORE_RT_REG(p_hwfn, QM_REG_WFQVPUPPERBOUND_RT_OFFSET +
				     vport_pq_id,
				     inc_val | QM_WFQ_CRD_REG_SIGN_BIT);
			STORE_RT_REG(p_hwfn, QM_REG_WFQVPWEIGHT_RT_OFFSET +
				     vport_pq_id, inc_val);
		}
	}

	return 0;
}

static bool qed_poll_on_qm_cmd_ready(struct qed_hwfn *p_hwfn,
				     struct qed_ptt *p_ptt)
{
	u32 reg_val, i;

	for (i = 0, reg_val = 0; i < QM_STOP_CMD_MAX_POLL_COUNT && !reg_val;
	     i++) {
		udelay(QM_STOP_CMD_POLL_PERIOD_US);
		reg_val = qed_rd(p_hwfn, p_ptt, QM_REG_SDMCMDREADY);
	}

	 
	if (i == QM_STOP_CMD_MAX_POLL_COUNT) {
		DP_VERBOSE(p_hwfn, NETIF_MSG_HW,
			   "Timeout when waiting for QM SDM command ready signal\n");
		return false;
	}

	return true;
}

static bool qed_send_qm_cmd(struct qed_hwfn *p_hwfn,
			    struct qed_ptt *p_ptt,
			    u32 cmd_addr, u32 cmd_data_lsb, u32 cmd_data_msb)
{
	if (!qed_poll_on_qm_cmd_ready(p_hwfn, p_ptt))
		return false;

	qed_wr(p_hwfn, p_ptt, QM_REG_SDMCMDADDR, cmd_addr);
	qed_wr(p_hwfn, p_ptt, QM_REG_SDMCMDDATALSB, cmd_data_lsb);
	qed_wr(p_hwfn, p_ptt, QM_REG_SDMCMDDATAMSB, cmd_data_msb);
	qed_wr(p_hwfn, p_ptt, QM_REG_SDMCMDGO, 1);
	qed_wr(p_hwfn, p_ptt, QM_REG_SDMCMDGO, 0);

	return qed_poll_on_qm_cmd_ready(p_hwfn, p_ptt);
}

 

u32 qed_qm_pf_mem_size(u32 num_pf_cids,
		       u32 num_vf_cids,
		       u32 num_tids, u16 num_pf_pqs, u16 num_vf_pqs)
{
	return QM_PQ_MEM_4KB(num_pf_cids) * num_pf_pqs +
	       QM_PQ_MEM_4KB(num_vf_cids) * num_vf_pqs +
	       QM_PQ_MEM_4KB(num_pf_cids + num_tids) * QM_OTHER_PQS_PER_PF;
}

int qed_qm_common_rt_init(struct qed_hwfn *p_hwfn,
			  struct qed_qm_common_rt_init_params *p_params)
{
	u32 mask = 0;

	 
	SET_FIELD(mask, QM_RF_OPPORTUNISTIC_MASK_LINEVOQ,
		  QM_OPPOR_LINE_VOQ_DEF);
	SET_FIELD(mask, QM_RF_OPPORTUNISTIC_MASK_BYTEVOQ, QM_BYTE_CRD_EN);
	SET_FIELD(mask, QM_RF_OPPORTUNISTIC_MASK_PFWFQ,
		  p_params->pf_wfq_en ? 1 : 0);
	SET_FIELD(mask, QM_RF_OPPORTUNISTIC_MASK_VPWFQ,
		  p_params->vport_wfq_en ? 1 : 0);
	SET_FIELD(mask, QM_RF_OPPORTUNISTIC_MASK_PFRL,
		  p_params->pf_rl_en ? 1 : 0);
	SET_FIELD(mask, QM_RF_OPPORTUNISTIC_MASK_VPQCNRL,
		  p_params->global_rl_en ? 1 : 0);
	SET_FIELD(mask, QM_RF_OPPORTUNISTIC_MASK_FWPAUSE, QM_OPPOR_FW_STOP_DEF);
	SET_FIELD(mask,
		  QM_RF_OPPORTUNISTIC_MASK_QUEUEEMPTY, QM_OPPOR_PQ_EMPTY_DEF);
	STORE_RT_REG(p_hwfn, QM_REG_AFULLOPRTNSTCCRDMASK_RT_OFFSET, mask);

	 
	qed_enable_pf_rl(p_hwfn, p_params->pf_rl_en);

	 
	qed_enable_pf_wfq(p_hwfn, p_params->pf_wfq_en);

	 
	qed_enable_global_rl(p_hwfn, p_params->global_rl_en);

	 
	qed_enable_vport_wfq(p_hwfn, p_params->vport_wfq_en);

	 
	qed_cmdq_lines_rt_init(p_hwfn,
			       p_params->max_ports_per_engine,
			       p_params->max_phys_tcs_per_port,
			       p_params->port_params);

	 
	qed_btb_blocks_rt_init(p_hwfn,
			       p_params->max_ports_per_engine,
			       p_params->max_phys_tcs_per_port,
			       p_params->port_params);

	qed_global_rl_rt_init(p_hwfn);

	return 0;
}

int qed_qm_pf_rt_init(struct qed_hwfn *p_hwfn,
		      struct qed_ptt *p_ptt,
		      struct qed_qm_pf_rt_init_params *p_params)
{
	struct init_qm_vport_params *vport_params = p_params->vport_params;
	u32 other_mem_size_4kb = QM_PQ_MEM_4KB(p_params->num_pf_cids +
					       p_params->num_tids) *
				 QM_OTHER_PQS_PER_PF;
	u16 i;
	u8 tc;

	 
	for (i = 0; i < p_params->num_vports; i++)
		for (tc = 0; tc < NUM_OF_TCS; tc++)
			vport_params[i].first_tx_pq_id[tc] = QM_INVALID_PQ_ID;

	 
	qed_other_pq_map_rt_init(p_hwfn,
				 p_params->pf_id,
				 p_params->is_pf_loading, p_params->num_pf_cids,
				 p_params->num_tids, 0);

	 
	if (qed_tx_pq_map_rt_init(p_hwfn, p_ptt, p_params, other_mem_size_4kb))
		return -1;

	 
	if (p_params->pf_wfq)
		if (qed_pf_wfq_rt_init(p_hwfn, p_params))
			return -1;

	 
	if (qed_pf_rl_rt_init(p_hwfn, p_params->pf_id, p_params->pf_rl))
		return -1;

	 
	if (qed_vp_wfq_rt_init(p_hwfn, p_params->num_vports, vport_params))
		return -1;

	 
	if (qed_vport_rl_rt_init(p_hwfn, p_params->start_rl,
				 p_params->num_rls, p_params->link_speed,
				 p_params->rl_params))
		return -1;

	return 0;
}

int qed_init_pf_wfq(struct qed_hwfn *p_hwfn,
		    struct qed_ptt *p_ptt, u8 pf_id, u16 pf_wfq)
{
	u32 inc_val = QM_PF_WFQ_INC_VAL(pf_wfq);

	if (!inc_val || inc_val > QM_PF_WFQ_MAX_INC_VAL) {
		DP_NOTICE(p_hwfn, "Invalid PF WFQ weight configuration\n");
		return -1;
	}

	qed_wr(p_hwfn, p_ptt, QM_REG_WFQPFWEIGHT + pf_id * 4, inc_val);

	return 0;
}

int qed_init_pf_rl(struct qed_hwfn *p_hwfn,
		   struct qed_ptt *p_ptt, u8 pf_id, u32 pf_rl)
{
	u32 inc_val = QM_RL_INC_VAL(pf_rl);

	if (inc_val > QM_PF_RL_MAX_INC_VAL) {
		DP_NOTICE(p_hwfn, "Invalid PF rate limit configuration\n");
		return -1;
	}

	qed_wr(p_hwfn,
	       p_ptt, QM_REG_RLPFCRD + pf_id * 4, (u32)QM_RL_CRD_REG_SIGN_BIT);
	qed_wr(p_hwfn, p_ptt, QM_REG_RLPFINCVAL + pf_id * 4, inc_val);

	return 0;
}

int qed_init_vport_wfq(struct qed_hwfn *p_hwfn,
		       struct qed_ptt *p_ptt,
		       u16 first_tx_pq_id[NUM_OF_TCS], u16 wfq)
{
	int result = 0;
	u16 vport_pq_id;
	u8 tc;

	for (tc = 0; tc < NUM_OF_TCS && !result; tc++) {
		vport_pq_id = first_tx_pq_id[tc];
		if (vport_pq_id != QM_INVALID_PQ_ID)
			result = qed_init_vport_tc_wfq(p_hwfn, p_ptt,
						       vport_pq_id, wfq);
	}

	return result;
}

int qed_init_vport_tc_wfq(struct qed_hwfn *p_hwfn, struct qed_ptt *p_ptt,
			  u16 first_tx_pq_id, u16 wfq)
{
	u32 inc_val;

	if (first_tx_pq_id == QM_INVALID_PQ_ID)
		return -1;

	inc_val = QM_VP_WFQ_INC_VAL(wfq);
	if (!inc_val || inc_val > QM_VP_WFQ_MAX_INC_VAL) {
		DP_NOTICE(p_hwfn, "Invalid VPORT WFQ configuration.\n");
		return -1;
	}

	qed_wr(p_hwfn, p_ptt, QM_REG_WFQVPCRD + first_tx_pq_id * 4,
	       (u32)QM_WFQ_CRD_REG_SIGN_BIT);
	qed_wr(p_hwfn, p_ptt, QM_REG_WFQVPUPPERBOUND + first_tx_pq_id * 4,
	       inc_val | QM_WFQ_CRD_REG_SIGN_BIT);
	qed_wr(p_hwfn, p_ptt, QM_REG_WFQVPWEIGHT + first_tx_pq_id * 4,
	       inc_val);

	return 0;
}

int qed_init_global_rl(struct qed_hwfn *p_hwfn,
		       struct qed_ptt *p_ptt, u16 rl_id, u32 rate_limit,
		       enum init_qm_rl_type vport_rl_type)
{
	u32 inc_val, upper_bound;

	upper_bound =
	    (vport_rl_type ==
	     QM_RL_TYPE_QCN) ? QM_GLOBAL_RL_UPPER_BOUND(QM_MAX_LINK_SPEED) :
	    QM_INITIAL_VOQ_BYTE_CRD;
	inc_val = QM_RL_INC_VAL(rate_limit);
	if (inc_val > upper_bound) {
		DP_NOTICE(p_hwfn, "Invalid VPORT rate limit configuration.\n");
		return -1;
	}

	qed_wr(p_hwfn, p_ptt,
	       QM_REG_RLGLBLCRD + rl_id * 4, (u32)QM_RL_CRD_REG_SIGN_BIT);
	qed_wr(p_hwfn,
	       p_ptt,
	       QM_REG_RLGLBLUPPERBOUND + rl_id * 4,
	       upper_bound | (u32)QM_RL_CRD_REG_SIGN_BIT);
	qed_wr(p_hwfn, p_ptt, QM_REG_RLGLBLINCVAL + rl_id * 4, inc_val);

	return 0;
}

bool qed_send_qm_stop_cmd(struct qed_hwfn *p_hwfn,
			  struct qed_ptt *p_ptt,
			  bool is_release_cmd,
			  bool is_tx_pq, u16 start_pq, u16 num_pqs)
{
	u32 cmd_arr[QM_CMD_STRUCT_SIZE(QM_STOP_CMD)] = { 0 };
	u32 pq_mask = 0, last_pq, pq_id;

	last_pq = start_pq + num_pqs - 1;

	 
	QM_CMD_SET_FIELD(cmd_arr, QM_STOP_CMD, PQ_TYPE, is_tx_pq ? 0 : 1);

	 
	for (pq_id = start_pq; pq_id <= last_pq; pq_id++) {
		 
		if (!is_release_cmd)
			pq_mask |= BIT((pq_id % QM_STOP_PQ_MASK_WIDTH));

		 
		if ((pq_id == last_pq) ||
		    (pq_id % QM_STOP_PQ_MASK_WIDTH ==
		     (QM_STOP_PQ_MASK_WIDTH - 1))) {
			QM_CMD_SET_FIELD(cmd_arr,
					 QM_STOP_CMD, PAUSE_MASK, pq_mask);
			QM_CMD_SET_FIELD(cmd_arr,
					 QM_STOP_CMD,
					 GROUP_ID,
					 pq_id / QM_STOP_PQ_MASK_WIDTH);
			if (!qed_send_qm_cmd(p_hwfn, p_ptt, QM_STOP_CMD_ADDR,
					     cmd_arr[0], cmd_arr[1]))
				return false;
			pq_mask = 0;
		}
	}

	return true;
}

#define SET_TUNNEL_TYPE_ENABLE_BIT(var, offset, enable) \
	do { \
		typeof(var) *__p_var = &(var); \
		typeof(offset) __offset = offset; \
		*__p_var = (*__p_var & ~BIT(__offset)) | \
			   ((enable) ? BIT(__offset) : 0); \
	} while (0)

#define PRS_ETH_TUNN_OUTPUT_FORMAT     0xF4DAB910
#define PRS_ETH_OUTPUT_FORMAT          0xFFFF4910

#define ARR_REG_WR(dev, ptt, addr, arr,	arr_size) \
	do { \
		u32 i; \
		\
		for (i = 0; i < (arr_size); i++) \
			qed_wr(dev, ptt, \
			       ((addr) + (4 * i)), \
			       ((u32 *)&(arr))[i]); \
	} while (0)

 
static int qed_dmae_to_grc(struct qed_hwfn *p_hwfn, struct qed_ptt *p_ptt,
			   __le32 *p_data, u32 addr, u32 len_in_dwords)
{
	struct qed_dmae_params params = { 0 };
	u32 *data_cpu;
	int rc;

	if (!p_data)
		return -1;

	 
	SET_FIELD(params.flags, QED_DMAE_PARAMS_COMPLETION_DST, 1);

	 
	rc = qed_dmae_host2grc(p_hwfn, p_ptt,
			       (u64)(uintptr_t)(p_data),
			       addr, len_in_dwords, &params);

	 
	if (rc) {
		DP_VERBOSE(p_hwfn,
			   QED_MSG_DEBUG,
			   "Failed writing to chip using DMAE, using GRC instead\n");

		 
		data_cpu = (__force u32 *)p_data;
		le32_to_cpu_array(data_cpu, len_in_dwords);

		ARR_REG_WR(p_hwfn, p_ptt, addr, data_cpu, len_in_dwords);
		cpu_to_le32_array(data_cpu, len_in_dwords);
	}

	return len_in_dwords;
}

void qed_set_vxlan_dest_port(struct qed_hwfn *p_hwfn,
			     struct qed_ptt *p_ptt, u16 dest_port)
{
	 
	qed_wr(p_hwfn, p_ptt, PRS_REG_VXLAN_PORT, dest_port);

	 
	qed_wr(p_hwfn, p_ptt, NIG_REG_VXLAN_CTRL, dest_port);

	 
	qed_wr(p_hwfn, p_ptt, PBF_REG_VXLAN_PORT, dest_port);
}

void qed_set_vxlan_enable(struct qed_hwfn *p_hwfn,
			  struct qed_ptt *p_ptt, bool vxlan_enable)
{
	u32 reg_val;
	u8 shift;

	 
	reg_val = qed_rd(p_hwfn, p_ptt, PRS_REG_ENCAPSULATION_TYPE_EN);
	SET_FIELD(reg_val,
		  PRS_REG_ENCAPSULATION_TYPE_EN_VXLAN_ENABLE, vxlan_enable);
	qed_wr(p_hwfn, p_ptt, PRS_REG_ENCAPSULATION_TYPE_EN, reg_val);
	if (reg_val) {
		reg_val =
		    qed_rd(p_hwfn, p_ptt, PRS_REG_OUTPUT_FORMAT_4_0);

		 
		if (reg_val == (u32)PRS_ETH_OUTPUT_FORMAT)
			qed_wr(p_hwfn, p_ptt, PRS_REG_OUTPUT_FORMAT_4_0,
			       (u32)PRS_ETH_TUNN_OUTPUT_FORMAT);
	}

	 
	reg_val = qed_rd(p_hwfn, p_ptt, NIG_REG_ENC_TYPE_ENABLE);
	shift = NIG_REG_ENC_TYPE_ENABLE_VXLAN_ENABLE_SHIFT;
	SET_TUNNEL_TYPE_ENABLE_BIT(reg_val, shift, vxlan_enable);
	qed_wr(p_hwfn, p_ptt, NIG_REG_ENC_TYPE_ENABLE, reg_val);

	 
	qed_wr(p_hwfn,
	       p_ptt, DORQ_REG_L2_EDPM_TUNNEL_VXLAN_EN, vxlan_enable ? 1 : 0);
}

void qed_set_gre_enable(struct qed_hwfn *p_hwfn,
			struct qed_ptt *p_ptt,
			bool eth_gre_enable, bool ip_gre_enable)
{
	u32 reg_val;
	u8 shift;

	 
	reg_val = qed_rd(p_hwfn, p_ptt, PRS_REG_ENCAPSULATION_TYPE_EN);
	SET_FIELD(reg_val,
		  PRS_REG_ENCAPSULATION_TYPE_EN_ETH_OVER_GRE_ENABLE,
		  eth_gre_enable);
	SET_FIELD(reg_val,
		  PRS_REG_ENCAPSULATION_TYPE_EN_IP_OVER_GRE_ENABLE,
		  ip_gre_enable);
	qed_wr(p_hwfn, p_ptt, PRS_REG_ENCAPSULATION_TYPE_EN, reg_val);
	if (reg_val) {
		reg_val =
		    qed_rd(p_hwfn, p_ptt, PRS_REG_OUTPUT_FORMAT_4_0);

		 
		if (reg_val == (u32)PRS_ETH_OUTPUT_FORMAT)
			qed_wr(p_hwfn, p_ptt, PRS_REG_OUTPUT_FORMAT_4_0,
			       (u32)PRS_ETH_TUNN_OUTPUT_FORMAT);
	}

	 
	reg_val = qed_rd(p_hwfn, p_ptt, NIG_REG_ENC_TYPE_ENABLE);
	shift = NIG_REG_ENC_TYPE_ENABLE_ETH_OVER_GRE_ENABLE_SHIFT;
	SET_TUNNEL_TYPE_ENABLE_BIT(reg_val, shift, eth_gre_enable);
	shift = NIG_REG_ENC_TYPE_ENABLE_IP_OVER_GRE_ENABLE_SHIFT;
	SET_TUNNEL_TYPE_ENABLE_BIT(reg_val, shift, ip_gre_enable);
	qed_wr(p_hwfn, p_ptt, NIG_REG_ENC_TYPE_ENABLE, reg_val);

	 
	qed_wr(p_hwfn,
	       p_ptt,
	       DORQ_REG_L2_EDPM_TUNNEL_GRE_ETH_EN, eth_gre_enable ? 1 : 0);
	qed_wr(p_hwfn,
	       p_ptt, DORQ_REG_L2_EDPM_TUNNEL_GRE_IP_EN, ip_gre_enable ? 1 : 0);
}

void qed_set_geneve_dest_port(struct qed_hwfn *p_hwfn,
			      struct qed_ptt *p_ptt, u16 dest_port)
{
	 
	qed_wr(p_hwfn, p_ptt, PRS_REG_NGE_PORT, dest_port);

	 
	qed_wr(p_hwfn, p_ptt, NIG_REG_NGE_PORT, dest_port);

	 
	qed_wr(p_hwfn, p_ptt, PBF_REG_NGE_PORT, dest_port);
}

void qed_set_geneve_enable(struct qed_hwfn *p_hwfn,
			   struct qed_ptt *p_ptt,
			   bool eth_geneve_enable, bool ip_geneve_enable)
{
	u32 reg_val;

	 
	reg_val = qed_rd(p_hwfn, p_ptt, PRS_REG_ENCAPSULATION_TYPE_EN);
	SET_FIELD(reg_val,
		  PRS_REG_ENCAPSULATION_TYPE_EN_ETH_OVER_GENEVE_ENABLE,
		  eth_geneve_enable);
	SET_FIELD(reg_val,
		  PRS_REG_ENCAPSULATION_TYPE_EN_IP_OVER_GENEVE_ENABLE,
		  ip_geneve_enable);
	qed_wr(p_hwfn, p_ptt, PRS_REG_ENCAPSULATION_TYPE_EN, reg_val);
	if (reg_val) {
		reg_val =
		    qed_rd(p_hwfn, p_ptt, PRS_REG_OUTPUT_FORMAT_4_0);

		 
		if (reg_val == (u32)PRS_ETH_OUTPUT_FORMAT)
			qed_wr(p_hwfn, p_ptt, PRS_REG_OUTPUT_FORMAT_4_0,
			       (u32)PRS_ETH_TUNN_OUTPUT_FORMAT);
	}

	 
	qed_wr(p_hwfn, p_ptt, NIG_REG_NGE_ETH_ENABLE,
	       eth_geneve_enable ? 1 : 0);
	qed_wr(p_hwfn, p_ptt, NIG_REG_NGE_IP_ENABLE, ip_geneve_enable ? 1 : 0);

	 
	if (QED_IS_BB_B0(p_hwfn->cdev))
		return;

	 
	qed_wr(p_hwfn,
	       p_ptt,
	       DORQ_REG_L2_EDPM_TUNNEL_NGE_ETH_EN_K2,
	       eth_geneve_enable ? 1 : 0);
	qed_wr(p_hwfn,
	       p_ptt,
	       DORQ_REG_L2_EDPM_TUNNEL_NGE_IP_EN_K2,
	       ip_geneve_enable ? 1 : 0);
}

#define PRS_ETH_VXLAN_NO_L2_ENABLE_OFFSET      3
#define PRS_ETH_VXLAN_NO_L2_OUTPUT_FORMAT   0xC8DAB910

void qed_set_vxlan_no_l2_enable(struct qed_hwfn *p_hwfn,
				struct qed_ptt *p_ptt, bool enable)
{
	u32 reg_val, cfg_mask;

	 
	reg_val = qed_rd(p_hwfn, p_ptt, PRS_REG_MSG_INFO);

	 
	cfg_mask = BIT(PRS_ETH_VXLAN_NO_L2_ENABLE_OFFSET);

	if (enable) {
		 
		reg_val |= cfg_mask;

		 
		qed_wr(p_hwfn,
		       p_ptt,
		       PRS_REG_OUTPUT_FORMAT_4_0,
		       (u32)PRS_ETH_VXLAN_NO_L2_OUTPUT_FORMAT);
	} else {
		 
		reg_val &= ~cfg_mask;
	}

	 
	qed_wr(p_hwfn, p_ptt, PRS_REG_MSG_INFO, reg_val);
}

#define T_ETH_PACKET_ACTION_GFT_EVENTID  23
#define PARSER_ETH_CONN_GFT_ACTION_CM_HDR  272
#define T_ETH_PACKET_MATCH_RFS_EVENTID 25
#define PARSER_ETH_CONN_CM_HDR 0
#define CAM_LINE_SIZE sizeof(u32)
#define RAM_LINE_SIZE sizeof(u64)
#define REG_SIZE sizeof(u32)

void qed_gft_disable(struct qed_hwfn *p_hwfn, struct qed_ptt *p_ptt, u16 pf_id)
{
	struct regpair ram_line = { 0 };

	 
	qed_wr(p_hwfn, p_ptt, PRS_REG_SEARCH_GFT, 0);

	 

	 
	qed_wr(p_hwfn, p_ptt, PRS_REG_GFT_CAM + CAM_LINE_SIZE * pf_id, 0);

	 
	qed_dmae_to_grc(p_hwfn, p_ptt, &ram_line.lo,
			PRS_REG_GFT_PROFILE_MASK_RAM + RAM_LINE_SIZE * pf_id,
			sizeof(ram_line) / REG_SIZE);
}

void qed_gft_config(struct qed_hwfn *p_hwfn,
		    struct qed_ptt *p_ptt,
		    u16 pf_id,
		    bool tcp,
		    bool udp,
		    bool ipv4, bool ipv6, enum gft_profile_type profile_type)
{
	struct regpair ram_line;
	u32 search_non_ip_as_gft;
	u32 reg_val, cam_line;
	u32 lo = 0, hi = 0;

	if (!ipv6 && !ipv4)
		DP_NOTICE(p_hwfn,
			  "gft_config: must accept at least on of - ipv4 or ipv6'\n");
	if (!tcp && !udp)
		DP_NOTICE(p_hwfn,
			  "gft_config: must accept at least on of - udp or tcp\n");
	if (profile_type >= MAX_GFT_PROFILE_TYPE)
		DP_NOTICE(p_hwfn, "gft_config: unsupported gft_profile_type\n");

	 
	reg_val = T_ETH_PACKET_MATCH_RFS_EVENTID <<
		  PRS_REG_CM_HDR_GFT_EVENT_ID_SHIFT;
	reg_val |= PARSER_ETH_CONN_CM_HDR << PRS_REG_CM_HDR_GFT_CM_HDR_SHIFT;
	qed_wr(p_hwfn, p_ptt, PRS_REG_CM_HDR_GFT, reg_val);

	 
	qed_wr(p_hwfn, p_ptt, PRS_REG_LOAD_L2_FILTER, 0);

	 
	qed_wr(p_hwfn, p_ptt, PRS_REG_SEARCH_TENANT_ID, 0);

	 
	cam_line = 0;
	SET_FIELD(cam_line, GFT_CAM_LINE_MAPPED_VALID, 1);

	 
	SET_FIELD(cam_line,
		  GFT_CAM_LINE_MAPPED_PF_ID_MASK,
		  GFT_CAM_LINE_MAPPED_PF_ID_MASK_MASK);
	SET_FIELD(cam_line, GFT_CAM_LINE_MAPPED_PF_ID, pf_id);

	if (!(tcp && udp)) {
		SET_FIELD(cam_line,
			  GFT_CAM_LINE_MAPPED_UPPER_PROTOCOL_TYPE_MASK,
			  GFT_CAM_LINE_MAPPED_UPPER_PROTOCOL_TYPE_MASK_MASK);
		if (tcp)
			SET_FIELD(cam_line,
				  GFT_CAM_LINE_MAPPED_UPPER_PROTOCOL_TYPE,
				  GFT_PROFILE_TCP_PROTOCOL);
		else
			SET_FIELD(cam_line,
				  GFT_CAM_LINE_MAPPED_UPPER_PROTOCOL_TYPE,
				  GFT_PROFILE_UDP_PROTOCOL);
	}

	if (!(ipv4 && ipv6)) {
		SET_FIELD(cam_line, GFT_CAM_LINE_MAPPED_IP_VERSION_MASK, 1);
		if (ipv4)
			SET_FIELD(cam_line,
				  GFT_CAM_LINE_MAPPED_IP_VERSION,
				  GFT_PROFILE_IPV4);
		else
			SET_FIELD(cam_line,
				  GFT_CAM_LINE_MAPPED_IP_VERSION,
				  GFT_PROFILE_IPV6);
	}

	 
	qed_wr(p_hwfn, p_ptt, PRS_REG_GFT_CAM + CAM_LINE_SIZE * pf_id,
	       cam_line);
	cam_line =
	    qed_rd(p_hwfn, p_ptt, PRS_REG_GFT_CAM + CAM_LINE_SIZE * pf_id);

	 

	 
	search_non_ip_as_gft = 0;

	 
	SET_FIELD(lo, GFT_RAM_LINE_TUNNEL_DST_PORT, 1);
	SET_FIELD(lo, GFT_RAM_LINE_TUNNEL_OVER_IP_PROTOCOL, 1);

	if (profile_type == GFT_PROFILE_TYPE_4_TUPLE) {
		SET_FIELD(hi, GFT_RAM_LINE_DST_IP, 1);
		SET_FIELD(hi, GFT_RAM_LINE_SRC_IP, 1);
		SET_FIELD(hi, GFT_RAM_LINE_OVER_IP_PROTOCOL, 1);
		SET_FIELD(lo, GFT_RAM_LINE_ETHERTYPE, 1);
		SET_FIELD(lo, GFT_RAM_LINE_SRC_PORT, 1);
		SET_FIELD(lo, GFT_RAM_LINE_DST_PORT, 1);
	} else if (profile_type == GFT_PROFILE_TYPE_L4_DST_PORT) {
		SET_FIELD(hi, GFT_RAM_LINE_OVER_IP_PROTOCOL, 1);
		SET_FIELD(lo, GFT_RAM_LINE_ETHERTYPE, 1);
		SET_FIELD(lo, GFT_RAM_LINE_DST_PORT, 1);
	} else if (profile_type == GFT_PROFILE_TYPE_IP_DST_ADDR) {
		SET_FIELD(hi, GFT_RAM_LINE_DST_IP, 1);
		SET_FIELD(lo, GFT_RAM_LINE_ETHERTYPE, 1);
	} else if (profile_type == GFT_PROFILE_TYPE_IP_SRC_ADDR) {
		SET_FIELD(hi, GFT_RAM_LINE_SRC_IP, 1);
		SET_FIELD(lo, GFT_RAM_LINE_ETHERTYPE, 1);
	} else if (profile_type == GFT_PROFILE_TYPE_TUNNEL_TYPE) {
		SET_FIELD(lo, GFT_RAM_LINE_TUNNEL_ETHERTYPE, 1);

		 
		search_non_ip_as_gft = 1;
	}

	ram_line.lo = cpu_to_le32(lo);
	ram_line.hi = cpu_to_le32(hi);

	qed_wr(p_hwfn,
	       p_ptt, PRS_REG_SEARCH_NON_IP_AS_GFT, search_non_ip_as_gft);
	qed_dmae_to_grc(p_hwfn, p_ptt, &ram_line.lo,
			PRS_REG_GFT_PROFILE_MASK_RAM + RAM_LINE_SIZE * pf_id,
			sizeof(ram_line) / REG_SIZE);

	 
	ram_line.lo = cpu_to_le32(0xffffffff);
	ram_line.hi = cpu_to_le32(0x3ff);
	qed_dmae_to_grc(p_hwfn, p_ptt, &ram_line.lo,
			PRS_REG_GFT_PROFILE_MASK_RAM + RAM_LINE_SIZE *
			PRS_GFT_CAM_LINES_NO_MATCH,
			sizeof(ram_line) / REG_SIZE);

	 
	qed_wr(p_hwfn, p_ptt, PRS_REG_SEARCH_GFT, 1);
}

DECLARE_CRC8_TABLE(cdu_crc8_table);

 
static u8 qed_calc_cdu_validation_byte(u8 conn_type, u8 region, u32 cid)
{
	const u8 validation_cfg = CDU_VALIDATION_DEFAULT_CFG;
	u8 crc, validation_byte = 0;
	static u8 crc8_table_valid;  
	u32 validation_string = 0;
	__be32 data_to_crc;

	if (!crc8_table_valid) {
		crc8_populate_msb(cdu_crc8_table, 0x07);
		crc8_table_valid = 1;
	}

	 
	if ((validation_cfg >> CDU_CONTEXT_VALIDATION_CFG_USE_CID) & 1)
		validation_string |= (cid & 0xFFF00000) | ((cid & 0xFFF) << 8);

	if ((validation_cfg >> CDU_CONTEXT_VALIDATION_CFG_USE_REGION) & 1)
		validation_string |= ((region & 0xF) << 4);

	if ((validation_cfg >> CDU_CONTEXT_VALIDATION_CFG_USE_TYPE) & 1)
		validation_string |= (conn_type & 0xF);

	 
	data_to_crc = cpu_to_be32(validation_string);
	crc = crc8(cdu_crc8_table, (u8 *)&data_to_crc, sizeof(data_to_crc),
		   CRC8_INIT_VALUE);

	 
	validation_byte |=
	    ((validation_cfg >>
	      CDU_CONTEXT_VALIDATION_CFG_USE_ACTIVE) & 1) << 7;

	if ((validation_cfg >>
	     CDU_CONTEXT_VALIDATION_CFG_VALIDATION_TYPE_SHIFT) & 1)
		validation_byte |= ((conn_type & 0xF) << 3) | (crc & 0x7);
	else
		validation_byte |= crc & 0x7F;

	return validation_byte;
}

 
void qed_calc_session_ctx_validation(void *p_ctx_mem,
				     u16 ctx_size, u8 ctx_type, u32 cid)
{
	u8 *x_val_ptr, *t_val_ptr, *u_val_ptr, *p_ctx;

	p_ctx = (u8 * const)p_ctx_mem;
	x_val_ptr = &p_ctx[con_region_offsets[0][ctx_type]];
	t_val_ptr = &p_ctx[con_region_offsets[1][ctx_type]];
	u_val_ptr = &p_ctx[con_region_offsets[2][ctx_type]];

	memset(p_ctx, 0, ctx_size);

	*x_val_ptr = qed_calc_cdu_validation_byte(ctx_type, 3, cid);
	*t_val_ptr = qed_calc_cdu_validation_byte(ctx_type, 4, cid);
	*u_val_ptr = qed_calc_cdu_validation_byte(ctx_type, 5, cid);
}

 
void qed_calc_task_ctx_validation(void *p_ctx_mem,
				  u16 ctx_size, u8 ctx_type, u32 tid)
{
	u8 *p_ctx, *region1_val_ptr;

	p_ctx = (u8 * const)p_ctx_mem;
	region1_val_ptr = &p_ctx[task_region_offsets[0][ctx_type]];

	memset(p_ctx, 0, ctx_size);

	*region1_val_ptr = qed_calc_cdu_validation_byte(ctx_type, 1, tid);
}

 
void qed_memset_session_ctx(void *p_ctx_mem, u32 ctx_size, u8 ctx_type)
{
	u8 *x_val_ptr, *t_val_ptr, *u_val_ptr, *p_ctx;
	u8 x_val, t_val, u_val;

	p_ctx = (u8 * const)p_ctx_mem;
	x_val_ptr = &p_ctx[con_region_offsets[0][ctx_type]];
	t_val_ptr = &p_ctx[con_region_offsets[1][ctx_type]];
	u_val_ptr = &p_ctx[con_region_offsets[2][ctx_type]];

	x_val = *x_val_ptr;
	t_val = *t_val_ptr;
	u_val = *u_val_ptr;

	memset(p_ctx, 0, ctx_size);

	*x_val_ptr = x_val;
	*t_val_ptr = t_val;
	*u_val_ptr = u_val;
}

 
void qed_memset_task_ctx(void *p_ctx_mem, u32 ctx_size, u8 ctx_type)
{
	u8 *p_ctx, *region1_val_ptr;
	u8 region1_val;

	p_ctx = (u8 * const)p_ctx_mem;
	region1_val_ptr = &p_ctx[task_region_offsets[0][ctx_type]];

	region1_val = *region1_val_ptr;

	memset(p_ctx, 0, ctx_size);

	*region1_val_ptr = region1_val;
}

 
void qed_enable_context_validation(struct qed_hwfn *p_hwfn,
				   struct qed_ptt *p_ptt)
{
	u32 ctx_validation;

	 
	ctx_validation = CDU_VALIDATION_DEFAULT_CFG << 24;
	qed_wr(p_hwfn, p_ptt, CDU_REG_CCFC_CTX_VALID0, ctx_validation);

	 
	ctx_validation = CDU_VALIDATION_DEFAULT_CFG << 8;
	qed_wr(p_hwfn, p_ptt, CDU_REG_CCFC_CTX_VALID1, ctx_validation);

	 
	ctx_validation = CDU_VALIDATION_DEFAULT_CFG << 8;
	qed_wr(p_hwfn, p_ptt, CDU_REG_TCFC_CTX_VALID0, ctx_validation);
}

const char *qed_get_protocol_type_str(u32 protocol_type)
{
	if (protocol_type >= ARRAY_SIZE(s_protocol_types))
		return "Invalid protocol type";

	return s_protocol_types[protocol_type];
}

const char *qed_get_ramrod_cmd_id_str(u32 protocol_type, u32 ramrod_cmd_id)
{
	const char *ramrod_cmd_id_str;

	if (protocol_type >= ARRAY_SIZE(s_ramrod_cmd_ids))
		return "Invalid protocol type";

	if (ramrod_cmd_id >= ARRAY_SIZE(s_ramrod_cmd_ids[0]))
		return "Invalid Ramrod command ID";

	ramrod_cmd_id_str = s_ramrod_cmd_ids[protocol_type][ramrod_cmd_id];

	if (!ramrod_cmd_id_str)
		return "Invalid Ramrod command ID";

	return ramrod_cmd_id_str;
}

static u32 qed_get_rdma_assert_ram_addr(struct qed_hwfn *p_hwfn, u8 storm_id)
{
	switch (storm_id) {
	case 0:
		return TSEM_REG_FAST_MEMORY + SEM_FAST_REG_INT_RAM +
		    TSTORM_RDMA_ASSERT_LEVEL_OFFSET(p_hwfn->rel_pf_id);
	case 1:
		return MSEM_REG_FAST_MEMORY + SEM_FAST_REG_INT_RAM +
		    MSTORM_RDMA_ASSERT_LEVEL_OFFSET(p_hwfn->rel_pf_id);
	case 2:
		return USEM_REG_FAST_MEMORY + SEM_FAST_REG_INT_RAM +
		    USTORM_RDMA_ASSERT_LEVEL_OFFSET(p_hwfn->rel_pf_id);
	case 3:
		return XSEM_REG_FAST_MEMORY + SEM_FAST_REG_INT_RAM +
		    XSTORM_RDMA_ASSERT_LEVEL_OFFSET(p_hwfn->rel_pf_id);
	case 4:
		return YSEM_REG_FAST_MEMORY + SEM_FAST_REG_INT_RAM +
		    YSTORM_RDMA_ASSERT_LEVEL_OFFSET(p_hwfn->rel_pf_id);
	case 5:
		return PSEM_REG_FAST_MEMORY + SEM_FAST_REG_INT_RAM +
		    PSTORM_RDMA_ASSERT_LEVEL_OFFSET(p_hwfn->rel_pf_id);

	default:
		return 0;
	}
}

void qed_set_rdma_error_level(struct qed_hwfn *p_hwfn,
			      struct qed_ptt *p_ptt,
			      u8 assert_level[NUM_STORMS])
{
	u8 storm_id;

	for (storm_id = 0; storm_id < NUM_STORMS; storm_id++) {
		u32 ram_addr = qed_get_rdma_assert_ram_addr(p_hwfn, storm_id);

		qed_wr(p_hwfn, p_ptt, ram_addr, assert_level[storm_id]);
	}
}

#define PHYS_ADDR_DWORDS        DIV_ROUND_UP(sizeof(dma_addr_t), 4)
#define OVERLAY_HDR_SIZE_DWORDS (sizeof(struct fw_overlay_buf_hdr) / 4)

static u32 qed_get_overlay_addr_ram_addr(struct qed_hwfn *p_hwfn, u8 storm_id)
{
	switch (storm_id) {
	case 0:
		return TSEM_REG_FAST_MEMORY + SEM_FAST_REG_INT_RAM +
		    TSTORM_OVERLAY_BUF_ADDR_OFFSET;
	case 1:
		return MSEM_REG_FAST_MEMORY + SEM_FAST_REG_INT_RAM +
		    MSTORM_OVERLAY_BUF_ADDR_OFFSET;
	case 2:
		return USEM_REG_FAST_MEMORY + SEM_FAST_REG_INT_RAM +
		    USTORM_OVERLAY_BUF_ADDR_OFFSET;
	case 3:
		return XSEM_REG_FAST_MEMORY + SEM_FAST_REG_INT_RAM +
		    XSTORM_OVERLAY_BUF_ADDR_OFFSET;
	case 4:
		return YSEM_REG_FAST_MEMORY + SEM_FAST_REG_INT_RAM +
		    YSTORM_OVERLAY_BUF_ADDR_OFFSET;
	case 5:
		return PSEM_REG_FAST_MEMORY + SEM_FAST_REG_INT_RAM +
		    PSTORM_OVERLAY_BUF_ADDR_OFFSET;

	default:
		return 0;
	}
}

struct phys_mem_desc *qed_fw_overlay_mem_alloc(struct qed_hwfn *p_hwfn,
					       const u32 * const
					       fw_overlay_in_buf,
					       u32 buf_size_in_bytes)
{
	u32 buf_size = buf_size_in_bytes / sizeof(u32), buf_offset = 0;
	struct phys_mem_desc *allocated_mem;

	if (!buf_size)
		return NULL;

	allocated_mem = kcalloc(NUM_STORMS, sizeof(struct phys_mem_desc),
				GFP_KERNEL);
	if (!allocated_mem)
		return NULL;

	 
	while (buf_offset < buf_size) {
		struct phys_mem_desc *storm_mem_desc;
		struct fw_overlay_buf_hdr *hdr;
		u32 storm_buf_size;
		u8 storm_id;

		hdr =
		    (struct fw_overlay_buf_hdr *)&fw_overlay_in_buf[buf_offset];
		storm_buf_size = GET_FIELD(hdr->data,
					   FW_OVERLAY_BUF_HDR_BUF_SIZE);
		storm_id = GET_FIELD(hdr->data, FW_OVERLAY_BUF_HDR_STORM_ID);
		if (storm_id >= NUM_STORMS)
			break;
		storm_mem_desc = allocated_mem + storm_id;
		storm_mem_desc->size = storm_buf_size * sizeof(u32);

		 
		storm_mem_desc->virt_addr =
		    dma_alloc_coherent(&p_hwfn->cdev->pdev->dev,
				       storm_mem_desc->size,
				       &storm_mem_desc->phys_addr, GFP_KERNEL);
		if (!storm_mem_desc->virt_addr)
			break;

		 
		buf_offset += OVERLAY_HDR_SIZE_DWORDS;

		 
		memcpy(storm_mem_desc->virt_addr,
		       &fw_overlay_in_buf[buf_offset], storm_mem_desc->size);

		 
		buf_offset += storm_buf_size;
	}

	 
	if (buf_offset < buf_size) {
		qed_fw_overlay_mem_free(p_hwfn, &allocated_mem);
		return NULL;
	}

	return allocated_mem;
}

void qed_fw_overlay_init_ram(struct qed_hwfn *p_hwfn,
			     struct qed_ptt *p_ptt,
			     struct phys_mem_desc *fw_overlay_mem)
{
	u8 storm_id;

	for (storm_id = 0; storm_id < NUM_STORMS; storm_id++) {
		struct phys_mem_desc *storm_mem_desc =
		    (struct phys_mem_desc *)fw_overlay_mem + storm_id;
		u32 ram_addr, i;

		 
		if (!storm_mem_desc->virt_addr)
			continue;

		 
		ram_addr = qed_get_overlay_addr_ram_addr(p_hwfn, storm_id) +
			   sizeof(dma_addr_t) * p_hwfn->rel_pf_id;

		 
		for (i = 0; i < PHYS_ADDR_DWORDS; i++, ram_addr += sizeof(u32))
			qed_wr(p_hwfn, p_ptt, ram_addr,
			       ((u32 *)&storm_mem_desc->phys_addr)[i]);
	}
}

void qed_fw_overlay_mem_free(struct qed_hwfn *p_hwfn,
			     struct phys_mem_desc **fw_overlay_mem)
{
	u8 storm_id;

	if (!fw_overlay_mem || !(*fw_overlay_mem))
		return;

	for (storm_id = 0; storm_id < NUM_STORMS; storm_id++) {
		struct phys_mem_desc *storm_mem_desc =
		    (struct phys_mem_desc *)*fw_overlay_mem + storm_id;

		 
		if (storm_mem_desc->virt_addr)
			dma_free_coherent(&p_hwfn->cdev->pdev->dev,
					  storm_mem_desc->size,
					  storm_mem_desc->virt_addr,
					  storm_mem_desc->phys_addr);
	}

	 
	kfree(*fw_overlay_mem);
	*fw_overlay_mem = NULL;
}
