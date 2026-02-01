 
 

#include <linux/compiler.h>
#include <linux/if_fddi.h>
#include <linux/spinlock.h>
#include <linux/timer.h>
#include <linux/types.h>

 
#define FZA_REG_BASE		0x100000	 
#define FZA_REG_RESET		0x100200	 
#define FZA_REG_INT_EVENT	0x100400	 
#define FZA_REG_STATUS		0x100402	 
#define FZA_REG_INT_MASK	0x100404	 
#define FZA_REG_CONTROL_A	0x100500	 
#define FZA_REG_CONTROL_B	0x100502	 

 
#define FZA_RESET_DLU	0x0002	 
#define FZA_RESET_INIT	0x0001	 
#define FZA_RESET_CLR	0x0000	 

 
#define FZA_EVENT_DLU_DONE	0x0800	 
#define FZA_EVENT_FLUSH_TX	0x0400	 
#define FZA_EVENT_PM_PARITY_ERR	0x0200	 
#define FZA_EVENT_HB_PARITY_ERR	0x0100	 
#define FZA_EVENT_NXM_ERR	0x0080	 
#define FZA_EVENT_LINK_ST_CHG	0x0040	 
#define FZA_EVENT_STATE_CHG	0x0020	 
#define FZA_EVENT_UNS_POLL	0x0010	 
#define FZA_EVENT_CMD_DONE	0x0008	 
#define FZA_EVENT_SMT_TX_POLL	0x0004	 
#define FZA_EVENT_RX_POLL	0x0002	 
#define FZA_EVENT_TX_DONE	0x0001	 

 
#define FZA_STATUS_DLU_SHIFT	0xc	 
#define FZA_STATUS_DLU_MASK	0x03
#define FZA_STATUS_LINK_SHIFT	0xb	 
#define FZA_STATUS_LINK_MASK	0x01
#define FZA_STATUS_STATE_SHIFT	0x8	 
#define FZA_STATUS_STATE_MASK	0x07
#define FZA_STATUS_HALT_SHIFT	0x0	 
#define FZA_STATUS_HALT_MASK	0xff
#define FZA_STATUS_TEST_SHIFT	0x0	 
#define FZA_STATUS_TEST_MASK	0xff

#define FZA_STATUS_GET_DLU(x)	(((x) >> FZA_STATUS_DLU_SHIFT) &	\
				 FZA_STATUS_DLU_MASK)
#define FZA_STATUS_GET_LINK(x)	(((x) >> FZA_STATUS_LINK_SHIFT) &	\
				 FZA_STATUS_LINK_MASK)
#define FZA_STATUS_GET_STATE(x)	(((x) >> FZA_STATUS_STATE_SHIFT) &	\
				 FZA_STATUS_STATE_MASK)
#define FZA_STATUS_GET_HALT(x)	(((x) >> FZA_STATUS_HALT_SHIFT) &	\
				 FZA_STATUS_HALT_MASK)
#define FZA_STATUS_GET_TEST(x)	(((x) >> FZA_STATUS_TEST_SHIFT) &	\
				 FZA_STATUS_TEST_MASK)

#define FZA_DLU_FAILURE		0x0	 
#define FZA_DLU_ERROR		0x1	 
#define FZA_DLU_SUCCESS		0x2	 

#define FZA_LINK_OFF		0x0	 
#define FZA_LINK_ON		0x1	 

#define FZA_STATE_RESET		0x0	 
#define FZA_STATE_UNINITIALIZED	0x1	 
#define FZA_STATE_INITIALIZED	0x2	 
#define FZA_STATE_RUNNING	0x3	 
#define FZA_STATE_MAINTENANCE	0x4	 
#define FZA_STATE_HALTED	0x5	 

#define FZA_HALT_UNKNOWN	0x00	 
#define FZA_HALT_HOST		0x01	 
#define FZA_HALT_HB_PARITY	0x02	 
#define FZA_HALT_NXM		0x03	 
#define FZA_HALT_SW		0x04	 
#define FZA_HALT_HW		0x05	 
#define FZA_HALT_PC_TRACE	0x06	 
#define FZA_HALT_DLSW		0x07	 
#define FZA_HALT_DLHW		0x08	 

#define FZA_TEST_FATAL		0x00	 
#define FZA_TEST_68K		0x01	 
#define FZA_TEST_SRAM_BWADDR	0x02	 
#define FZA_TEST_SRAM_DBUS	0x03	 
#define FZA_TEST_SRAM_STUCK1	0x04	 
#define FZA_TEST_SRAM_STUCK2	0x05	 
#define FZA_TEST_SRAM_COUPL1	0x06	 
#define FZA_TEST_SRAM_COUPL2	0x07	 
#define FZA_TEST_FLASH_CRC	0x08	 
#define FZA_TEST_ROM		0x09	 
#define FZA_TEST_PHY_CSR	0x0a	 
#define FZA_TEST_MAC_BIST	0x0b	 
#define FZA_TEST_MAC_CSR	0x0c	 
#define FZA_TEST_MAC_ADDR_UNIQ	0x0d	 
#define FZA_TEST_ELM_BIST	0x0e	 
#define FZA_TEST_ELM_CSR	0x0f	 
#define FZA_TEST_ELM_ADDR_UNIQ	0x10	 
#define FZA_TEST_CAM		0x11	 
#define FZA_TEST_NIROM		0x12	 
#define FZA_TEST_SC_LOOP	0x13	 
#define FZA_TEST_LM_LOOP	0x14	 
#define FZA_TEST_EB_LOOP	0x15	 
#define FZA_TEST_SC_LOOP_BYPS	0x16	 
#define FZA_TEST_LM_LOOP_LOCAL	0x17	 
#define FZA_TEST_EB_LOOP_LOCAL	0x18	 
#define FZA_TEST_CDC_LOOP	0x19	 
#define FZA_TEST_FIBER_LOOP	0x1A	 
#define FZA_TEST_CAM_MATCH_LOOP	0x1B	 
#define FZA_TEST_68K_IRQ_STUCK	0x1C	 
#define FZA_TEST_IRQ_PRESENT	0x1D	 
#define FZA_TEST_RMC_BIST	0x1E	 
#define FZA_TEST_RMC_CSR	0x1F	 
#define FZA_TEST_RMC_ADDR_UNIQ	0x20	 
#define FZA_TEST_PM_DPATH	0x21	 
#define FZA_TEST_PM_ADDR	0x22	 
#define FZA_TEST_RES_23		0x23	 
#define FZA_TEST_PM_DESC	0x24	 
#define FZA_TEST_PM_OWN		0x25	 
#define FZA_TEST_PM_PARITY	0x26	 
#define FZA_TEST_PM_BSWAP	0x27	 
#define FZA_TEST_PM_WSWAP	0x28	 
#define FZA_TEST_PM_REF		0x29	 
#define FZA_TEST_PM_CSR		0x2A	 
#define FZA_TEST_PORT_STATUS	0x2B	 
#define FZA_TEST_HOST_IRQMASK	0x2C	 
#define FZA_TEST_TIMER_IRQ1	0x2D	 
#define FZA_TEST_FORCE_IRQ1	0x2E	 
#define FZA_TEST_TIMER_IRQ5	0x2F	 
#define FZA_TEST_FORCE_IRQ5	0x30	 
#define FZA_TEST_RES_31		0x31	 
#define FZA_TEST_IC_PRIO	0x32	 
#define FZA_TEST_PM_FULL	0x33	 
#define FZA_TEST_PMI_DMA	0x34	 

 
#define FZA_MASK_RESERVED	0xf000	 
#define FZA_MASK_DLU_DONE	0x0800	 
#define FZA_MASK_FLUSH_TX	0x0400	 
#define FZA_MASK_PM_PARITY_ERR	0x0200	 
#define FZA_MASK_HB_PARITY_ERR	0x0100	 
#define FZA_MASK_NXM_ERR	0x0080	 
#define FZA_MASK_LINK_ST_CHG	0x0040	 
#define FZA_MASK_STATE_CHG	0x0020	 
#define FZA_MASK_UNS_POLL	0x0010	 
#define FZA_MASK_CMD_DONE	0x0008	 
#define FZA_MASK_SMT_TX_POLL	0x0004	 
#define FZA_MASK_RCV_POLL	0x0002	 
#define FZA_MASK_TX_DONE	0x0001	 

 
#define FZA_MASK_NONE		0x0000
#define FZA_MASK_NORMAL							\
		((~(FZA_MASK_RESERVED | FZA_MASK_DLU_DONE |		\
		    FZA_MASK_PM_PARITY_ERR | FZA_MASK_HB_PARITY_ERR |	\
		    FZA_MASK_NXM_ERR)) & 0xffff)

 
#define FZA_CONTROL_A_HB_PARITY_ERR	0x8000	 
#define FZA_CONTROL_A_NXM_ERR		0x4000	 
#define FZA_CONTROL_A_SMT_RX_OVFL	0x0040	 
#define FZA_CONTROL_A_FLUSH_DONE	0x0020	 
#define FZA_CONTROL_A_SHUT		0x0010	 
#define FZA_CONTROL_A_HALT		0x0008	 
#define FZA_CONTROL_A_CMD_POLL		0x0004	 
#define FZA_CONTROL_A_SMT_RX_POLL	0x0002	 
#define FZA_CONTROL_A_TX_POLL		0x0001	 

 
#define FZA_CONTROL_B_CONSOLE	0x0002	 
#define FZA_CONTROL_B_DRIVER	0x0001	 
#define FZA_CONTROL_B_IDLE	0x0000	 

#define FZA_RESET_PAD							\
		(FZA_REG_RESET - FZA_REG_BASE)
#define FZA_INT_EVENT_PAD						\
		(FZA_REG_INT_EVENT - FZA_REG_RESET - sizeof(u16))
#define FZA_CONTROL_A_PAD						\
		(FZA_REG_CONTROL_A - FZA_REG_INT_MASK - sizeof(u16))

 
struct fza_regs {
	u8  pad0[FZA_RESET_PAD];
	u16 reset;				 
	u8  pad1[FZA_INT_EVENT_PAD];
	u16 int_event;				 
	u16 status;				 
	u16 int_mask;				 
	u8  pad2[FZA_CONTROL_A_PAD];
	u16 control_a;				 
	u16 control_b;				 
};

 
struct fza_ring_cmd {
	u32 cmd_own;		 
	u32 stat;		 
	u32 buffer;		 
	u32 pad0;
};

#define FZA_RING_CMD		0x200400	 
#define FZA_RING_CMD_SIZE	0x40		 
 
#define FZA_RING_CMD_MASK	0x7fffffff
#define FZA_RING_CMD_NOP	0x00000000	 
#define FZA_RING_CMD_INIT	0x00000001	 
#define FZA_RING_CMD_MODCAM	0x00000002	 
#define FZA_RING_CMD_PARAM	0x00000003	 
#define FZA_RING_CMD_MODPROM	0x00000004	 
#define FZA_RING_CMD_SETCHAR	0x00000005	 
#define FZA_RING_CMD_RDCNTR	0x00000006	 
#define FZA_RING_CMD_STATUS	0x00000007	 
#define FZA_RING_CMD_RDCAM	0x00000008	 

 
#define FZA_RING_STAT_SUCCESS	0x00000000

 
struct fza_ring_uns {
	u32 own;		 
	u32 id;			 
	u32 buffer;		 
	u32 pad0;		 
};

#define FZA_RING_UNS		0x200800	 
#define FZA_RING_UNS_SIZE	0x40		 
 
#define FZA_RING_UNS_UND	0x00000000	 
#define FZA_RING_UNS_INIT_IN	0x00000001	 
#define FZA_RING_UNS_INIT_RX	0x00000002	 
#define FZA_RING_UNS_BEAC_IN	0x00000003	 
#define FZA_RING_UNS_DUP_ADDR	0x00000004	 
#define FZA_RING_UNS_DUP_TOK	0x00000005	 
#define FZA_RING_UNS_PURG_ERR	0x00000006	 
#define FZA_RING_UNS_STRIP_ERR	0x00000007	 
#define FZA_RING_UNS_OP_OSC	0x00000008	 
#define FZA_RING_UNS_BEAC_RX	0x00000009	 
#define FZA_RING_UNS_PCT_IN	0x0000000a	 
#define FZA_RING_UNS_PCT_RX	0x0000000b	 
#define FZA_RING_UNS_TX_UNDER	0x0000000c	 
#define FZA_RING_UNS_TX_FAIL	0x0000000d	 
#define FZA_RING_UNS_RX_OVER	0x0000000e	 

 
struct fza_ring_rmc_tx {
	u32 rmc;		 
	u32 avl;		 
	u32 own;		 
	u32 pad0;		 
};

#define FZA_TX_BUFFER_ADDR(x)	(0x200000 | (((x) & 0xffff) << 5))
#define FZA_TX_BUFFER_SIZE	512
struct fza_buffer_tx {
	u32 data[FZA_TX_BUFFER_SIZE / sizeof(u32)];
};

 
#define FZA_RING_TX_SOP		0x80000000	 
#define FZA_RING_TX_EOP		0x40000000	 
#define FZA_RING_TX_DTP		0x20000000	 
#define FZA_RING_TX_VBC		0x10000000	 
#define FZA_RING_TX_DCC_MASK	0x0f000000	 
#define FZA_RING_TX_DCC_SUCCESS	0x01000000	 
#define FZA_RING_TX_DCC_DTP_SOP	0x02000000	 
#define FZA_RING_TX_DCC_DTP	0x04000000	 
#define FZA_RING_TX_DCC_ABORT	0x05000000	 
#define FZA_RING_TX_DCC_PARITY	0x06000000	 
#define FZA_RING_TX_DCC_UNDRRUN	0x07000000	 
#define FZA_RING_TX_XPO_MASK	0x003fe000	 

 
struct fza_ring_hst_rx {
	u32 buf0_own;		 
	u32 buffer1;		 
	u32 rmc;		 
	u32 pad0;
};

#define FZA_RX_BUFFER_SIZE	(4096 + 512)	 

 
#define FZA_RING_RX_SOP		0x80000000	 
#define FZA_RING_RX_EOP		0x40000000	 
#define FZA_RING_RX_FSC_MASK	0x38000000	 
#define FZA_RING_RX_FSB_MASK	0x07c00000	 
#define FZA_RING_RX_FSB_ERR	0x04000000	 
#define FZA_RING_RX_FSB_ADDR	0x02000000	 
#define FZA_RING_RX_FSB_COP	0x01000000	 
#define FZA_RING_RX_FSB_F0	0x00800000	 
#define FZA_RING_RX_FSB_F1	0x00400000	 
#define FZA_RING_RX_BAD		0x00200000	 
#define FZA_RING_RX_CRC		0x00100000	 
#define FZA_RING_RX_RRR_MASK	0x000e0000	 
#define FZA_RING_RX_RRR_OK	0x00000000	 
#define FZA_RING_RX_RRR_SADDR	0x00020000	 
#define FZA_RING_RX_RRR_DADDR	0x00040000	 
#define FZA_RING_RX_RRR_ABORT	0x00060000	 
#define FZA_RING_RX_RRR_LENGTH	0x00080000	 
#define FZA_RING_RX_RRR_FRAG	0x000a0000	 
#define FZA_RING_RX_RRR_FORMAT	0x000c0000	 
#define FZA_RING_RX_RRR_RESET	0x000e0000	 
#define FZA_RING_RX_DA_MASK	0x00018000	 
#define FZA_RING_RX_DA_NONE	0x00000000	 
#define FZA_RING_RX_DA_PROM	0x00008000	 
#define FZA_RING_RX_DA_CAM	0x00010000	 
#define FZA_RING_RX_DA_LOCAL	0x00018000	 
#define FZA_RING_RX_SA_MASK	0x00006000	 
#define FZA_RING_RX_SA_NONE	0x00000000	 
#define FZA_RING_RX_SA_ALIAS	0x00002000	 
#define FZA_RING_RX_SA_CAM	0x00004000	 
#define FZA_RING_RX_SA_LOCAL	0x00006000	 

 
struct fza_ring_smt {
	u32 own;		 
	u32 rmc;		 
	u32 buffer;		 
	u32 pad0;		 
};

 
#define FZA_RING_OWN_MASK	0x80000000
#define FZA_RING_OWN_FZA	0x00000000	 
#define FZA_RING_OWN_HOST	0x80000000	 
#define FZA_RING_TX_OWN_RMC	0x80000000	 
#define FZA_RING_TX_OWN_HOST	0x00000000	 

 
#define FZA_RING_PBC_MASK	0x00001fff	 

 

struct fza_counter {
	u32 msw;
	u32 lsw;
};

struct fza_counters {
	struct fza_counter sys_buf;	 
	struct fza_counter tx_under;	 
	struct fza_counter tx_fail;	 
	struct fza_counter rx_over;	 
	struct fza_counter frame_cnt;	 
	struct fza_counter error_cnt;	 
	struct fza_counter lost_cnt;	 
	struct fza_counter rinit_in;	 
	struct fza_counter rinit_rx;	 
	struct fza_counter beac_in;	 
	struct fza_counter dup_addr;	 
	struct fza_counter dup_tok;	 
	struct fza_counter purg_err;	 
	struct fza_counter strip_err;	 
	struct fza_counter pct_in;	 
	struct fza_counter pct_rx;	 
	struct fza_counter lem_rej;	 
	struct fza_counter tne_rej;	 
	struct fza_counter lem_event;	 
	struct fza_counter lct_rej;	 
	struct fza_counter conn_cmpl;	 
	struct fza_counter el_buf;	 
};

 

 
struct fza_cmd_init {
	u32 tx_mode;			 
	u32 hst_rx_size;		 

	struct fza_counters counters;	 

	u8 rmc_rev[4];			 
	u8 rom_rev[4];			 
	u8 fw_rev[4];			 

	u32 mop_type;			 

	u32 hst_rx;			 
	u32 rmc_tx;			 
	u32 rmc_tx_size;		 
	u32 smt_tx;			 
	u32 smt_tx_size;		 
	u32 smt_rx;			 
	u32 smt_rx_size;		 

	u32 hw_addr[2];			 

	u32 def_t_req;			 
	u32 def_tvx;			 
	u32 def_t_max;			 
	u32 lem_threshold;		 
	u32 def_station_id[2];		 

	u32 pmd_type_alt;		 

	u32 smt_ver;			 

	u32 rtoken_timeout;		 
	u32 ring_purger;		 

	u32 smt_ver_max;		 
	u32 smt_ver_min;		 
	u32 pmd_type;			 
};

 
#define FZA_PMD_TYPE_MMF	  0	 
#define FZA_PMD_TYPE_TW		101	 
#define FZA_PMD_TYPE_STP	102	 

 
#define FZA_CMD_CAM_SIZE	64		 
struct fza_cmd_cam {
	u32 hw_addr[FZA_CMD_CAM_SIZE][2];	 
};

 
struct fza_cmd_param {
	u32 loop_mode;			 
	u32 t_max;			 
	u32 t_req;			 
	u32 tvx;			 
	u32 lem_threshold;		 
	u32 station_id[2];		 
	u32 rtoken_timeout;		 
	u32 ring_purger;		 
};

 
#define FZA_LOOP_NORMAL		0
#define FZA_LOOP_INTERN		1
#define FZA_LOOP_EXTERN		2

 
struct fza_cmd_modprom {
	u32 llc_prom;			 
	u32 smt_prom;			 
	u32 llc_multi;			 
	u32 llc_bcast;			 
};

 
struct fza_cmd_setchar {
	u32 t_max;			 
	u32 t_req;			 
	u32 tvx;			 
	u32 lem_threshold;		 
	u32 rtoken_timeout;		 
	u32 ring_purger;		 
};

 
struct fza_cmd_rdcntr {
	struct fza_counters counters;	 
};

 
struct fza_cmd_status {
	u32 led_state;			 
	u32 rmt_state;			 
	u32 link_state;			 
	u32 dup_addr;			 
	u32 ring_purger;		 
	u32 t_neg;			 
	u32 una[2];			 
	u32 una_timeout;		 
	u32 strip_mode;			 
	u32 yield_mode;			 
	u32 phy_state;			 
	u32 neigh_phy;			 
	u32 reject;			 
	u32 phy_lee;			 
	u32 una_old[2];			 
	u32 rmt_mac;			 
	u32 ring_err;			 
	u32 beac_rx[2];			 
	u32 un_dup_addr;		 
	u32 dna[2];			 
	u32 dna_old[2];			 
};

 
union fza_cmd_buf {
	struct fza_cmd_init init;
	struct fza_cmd_cam cam;
	struct fza_cmd_param param;
	struct fza_cmd_modprom modprom;
	struct fza_cmd_setchar setchar;
	struct fza_cmd_rdcntr rdcntr;
	struct fza_cmd_status status;
};

 

 
#define FZA_PRH0_FMT_TYPE_MASK	0xc0	 
#define FZA_PRH0_TOK_TYPE_MASK	0x30	 
#define FZA_PRH0_TKN_TYPE_ANY	0x30	 
#define FZA_PRH0_TKN_TYPE_UNR	0x20	 
#define FZA_PRH0_TKN_TYPE_RST	0x10	 
#define FZA_PRH0_TKN_TYPE_IMM	0x00	 
#define FZA_PRH0_FRAME_MASK	0x08	 
#define FZA_PRH0_FRAME_SYNC	0x08	 
#define FZA_PRH0_FRAME_ASYNC	0x00	 
#define FZA_PRH0_MODE_MASK	0x04	 
#define FZA_PRH0_MODE_IMMED	0x04	 
#define FZA_PRH0_MODE_NORMAL	0x00	 
#define FZA_PRH0_SF_MASK	0x02	 
#define FZA_PRH0_SF_FIRST	0x02	 
#define FZA_PRH0_SF_NORMAL	0x00	 
#define FZA_PRH0_BCN_MASK	0x01	 
#define FZA_PRH0_BCN_BEACON	0x01	 
#define FZA_PRH0_BCN_DATA	0x01	 
 
					 
#define FZA_PRH1_SL_MASK	0x40	 
#define FZA_PRH1_SL_LAST	0x40	 
#define FZA_PRH1_SL_NORMAL	0x00	 
#define FZA_PRH1_CRC_MASK	0x20	 
#define FZA_PRH1_CRC_NORMAL	0x20	 
#define FZA_PRH1_CRC_SKIP	0x00	 
#define FZA_PRH1_TKN_SEND_MASK	0x18	 
#define FZA_PRH1_TKN_SEND_ORIG	0x18	 
#define FZA_PRH1_TKN_SEND_RST	0x10	 
#define FZA_PRH1_TKN_SEND_UNR	0x08	 
#define FZA_PRH1_TKN_SEND_NONE	0x00	 
#define FZA_PRH1_EXTRA_FS_MASK	0x07	 
#define FZA_PRH1_EXTRA_FS_ST	0x07	 
#define FZA_PRH1_EXTRA_FS_SS	0x06	 
#define FZA_PRH1_EXTRA_FS_SR	0x05	 
#define FZA_PRH1_EXTRA_FS_NONE1	0x04	 
#define FZA_PRH1_EXTRA_FS_RT	0x03	 
#define FZA_PRH1_EXTRA_FS_RS	0x02	 
#define FZA_PRH1_EXTRA_FS_RR	0x01	 
#define FZA_PRH1_EXTRA_FS_NONE	0x00	 
 
#define FZA_PRH2_NORMAL		0x00	 

 
#define FZA_PRH0_LLC		(FZA_PRH0_TKN_TYPE_UNR)
#define FZA_PRH1_LLC		(FZA_PRH1_CRC_NORMAL | FZA_PRH1_TKN_SEND_UNR)
#define FZA_PRH2_LLC		(FZA_PRH2_NORMAL)

 
#define FZA_PRH0_SMT		(FZA_PRH0_TKN_TYPE_UNR)
#define FZA_PRH1_SMT		(FZA_PRH1_CRC_NORMAL | FZA_PRH1_TKN_SEND_UNR)
#define FZA_PRH2_SMT		(FZA_PRH2_NORMAL)

#if ((FZA_RING_RX_SIZE) < 2) || ((FZA_RING_RX_SIZE) > 256)
# error FZA_RING_RX_SIZE has to be from 2 up to 256
#endif
#if ((FZA_RING_TX_MODE) != 0) && ((FZA_RING_TX_MODE) != 1)
# error FZA_RING_TX_MODE has to be either 0 or 1
#endif

#define FZA_RING_TX_SIZE (512 << (FZA_RING_TX_MODE))

struct fza_private {
	struct device *bdev;		 
	const char *name;		 
	void __iomem *mmio;		 
	struct fza_regs __iomem *regs;	 

	struct sk_buff *rx_skbuff[FZA_RING_RX_SIZE];
					 
	dma_addr_t rx_dma[FZA_RING_RX_SIZE];
					 

	struct fza_ring_cmd __iomem *ring_cmd;
					 
	int ring_cmd_index;		 
	struct fza_ring_uns __iomem *ring_uns;
					 
	int ring_uns_index;		 

	struct fza_ring_rmc_tx __iomem *ring_rmc_tx;
					 
	int ring_rmc_tx_size;		 
	int ring_rmc_tx_index;		 
	int ring_rmc_txd_index;		 

	struct fza_ring_hst_rx __iomem *ring_hst_rx;
					 
	int ring_hst_rx_size;		 
	int ring_hst_rx_index;		 

	struct fza_ring_smt __iomem *ring_smt_tx;
					 
	int ring_smt_tx_size;		 
	int ring_smt_tx_index;		 

	struct fza_ring_smt __iomem *ring_smt_rx;
					 
	int ring_smt_rx_size;		 
	int ring_smt_rx_index;		 

	struct fza_buffer_tx __iomem *buffer_tx;
					 

	uint state;			 

	spinlock_t lock;		 
	uint int_mask;			 

	int cmd_done_flag;		 
	wait_queue_head_t cmd_done_wait;

	int state_chg_flag;		 
	wait_queue_head_t state_chg_wait;

	struct timer_list reset_timer;	 
	int timer_state;		 

	int queue_active;		 

	struct net_device_stats stats;

	uint irq_count_flush_tx;	 
	uint irq_count_uns_poll;	 
	uint irq_count_smt_tx_poll;	 
	uint irq_count_rx_poll;		 
	uint irq_count_tx_done;		 
	uint irq_count_cmd_done;	 
	uint irq_count_state_chg;	 
	uint irq_count_link_st_chg;	 

	uint t_max;			 
	uint t_req;			 
	uint tvx;			 
	uint lem_threshold;		 
	uint station_id[2];		 
	uint rtoken_timeout;		 
	uint ring_purger;		 
};

struct fza_fddihdr {
	u8 pa[2];			 
	u8 sd;				 
	struct fddihdr hdr;
} __packed;
