#ifndef __NITROX_CSR_H
#define __NITROX_CSR_H
#include <asm/byteorder.h>
#include <linux/types.h>
#define NR_CLUSTERS		4
#define AE_CORES_PER_CLUSTER	20
#define SE_CORES_PER_CLUSTER	16
#define AE_MAX_CORES	(AE_CORES_PER_CLUSTER * NR_CLUSTERS)
#define SE_MAX_CORES	(SE_CORES_PER_CLUSTER * NR_CLUSTERS)
#define ZIP_MAX_CORES	5
#define EMU_BIST_STATUSX(_i)	(0x1402700 + ((_i) * 0x40000))
#define UCD_BIST_STATUS		0x12C0070
#define NPS_CORE_BIST_REG	0x10000E8
#define NPS_CORE_NPC_BIST_REG	0x1000128
#define NPS_PKT_SLC_BIST_REG	0x1040088
#define NPS_PKT_IN_BIST_REG	0x1040100
#define POM_BIST_REG		0x11C0100
#define BMI_BIST_REG		0x1140080
#define EFL_CORE_BIST_REGX(_i)	(0x1240100 + ((_i) * 0x400))
#define EFL_TOP_BIST_STAT	0x1241090
#define BMO_BIST_REG		0x1180080
#define LBC_BIST_STATUS		0x1200020
#define PEM_BIST_STATUSX(_i)	(0x1080468 | ((_i) << 18))
#define EMU_SE_ENABLEX(_i)	(0x1400000 + ((_i) * 0x40000))
#define EMU_AE_ENABLEX(_i)	(0x1400008 + ((_i) * 0x40000))
#define EMU_WD_INT_ENA_W1SX(_i)	(0x1402318 + ((_i) * 0x40000))
#define EMU_GE_INT_ENA_W1SX(_i)	(0x1402518 + ((_i) * 0x40000))
#define EMU_FUSE_MAPX(_i)	(0x1402708 + ((_i) * 0x40000))
#define UCD_SE_EID_UCODE_BLOCK_NUMX(_i)	(0x12C0000 + ((_i) * 0x1000))
#define UCD_AE_EID_UCODE_BLOCK_NUMX(_i)	(0x12C0008 + ((_i) * 0x800))
#define UCD_UCODE_LOAD_BLOCK_NUM	0x12C0010
#define UCD_UCODE_LOAD_IDX_DATAX(_i)	(0x12C0018 + ((_i) * 0x20))
#define UCD_SE_CNTX(_i)			(0x12C0040 + ((_i) * 0x1000))
#define UCD_AE_CNTX(_i)			(0x12C0048 + ((_i) * 0x800))
#define AQM_CTL                         0x1300000
#define AQM_INT                         0x1300008
#define AQM_DBELL_OVF_LO                0x1300010
#define AQM_DBELL_OVF_HI                0x1300018
#define AQM_DBELL_OVF_LO_W1S            0x1300020
#define AQM_DBELL_OVF_LO_ENA_W1C        0x1300028
#define AQM_DBELL_OVF_LO_ENA_W1S        0x1300030
#define AQM_DBELL_OVF_HI_W1S            0x1300038
#define AQM_DBELL_OVF_HI_ENA_W1C        0x1300040
#define AQM_DBELL_OVF_HI_ENA_W1S        0x1300048
#define AQM_DMA_RD_ERR_LO               0x1300050
#define AQM_DMA_RD_ERR_HI               0x1300058
#define AQM_DMA_RD_ERR_LO_W1S           0x1300060
#define AQM_DMA_RD_ERR_LO_ENA_W1C       0x1300068
#define AQM_DMA_RD_ERR_LO_ENA_W1S       0x1300070
#define AQM_DMA_RD_ERR_HI_W1S           0x1300078
#define AQM_DMA_RD_ERR_HI_ENA_W1C       0x1300080
#define AQM_DMA_RD_ERR_HI_ENA_W1S       0x1300088
#define AQM_EXEC_NA_LO                  0x1300090
#define AQM_EXEC_NA_HI                  0x1300098
#define AQM_EXEC_NA_LO_W1S              0x13000A0
#define AQM_EXEC_NA_LO_ENA_W1C          0x13000A8
#define AQM_EXEC_NA_LO_ENA_W1S          0x13000B0
#define AQM_EXEC_NA_HI_W1S              0x13000B8
#define AQM_EXEC_NA_HI_ENA_W1C          0x13000C0
#define AQM_EXEC_NA_HI_ENA_W1S          0x13000C8
#define AQM_EXEC_ERR_LO                 0x13000D0
#define AQM_EXEC_ERR_HI                 0x13000D8
#define AQM_EXEC_ERR_LO_W1S             0x13000E0
#define AQM_EXEC_ERR_LO_ENA_W1C         0x13000E8
#define AQM_EXEC_ERR_LO_ENA_W1S         0x13000F0
#define AQM_EXEC_ERR_HI_W1S             0x13000F8
#define AQM_EXEC_ERR_HI_ENA_W1C         0x1300100
#define AQM_EXEC_ERR_HI_ENA_W1S         0x1300108
#define AQM_ECC_INT                     0x1300110
#define AQM_ECC_INT_W1S                 0x1300118
#define AQM_ECC_INT_ENA_W1C             0x1300120
#define AQM_ECC_INT_ENA_W1S             0x1300128
#define AQM_ECC_CTL                     0x1300130
#define AQM_BIST_STATUS                 0x1300138
#define AQM_CMD_INF_THRX(x)             (0x1300400 + ((x) * 0x8))
#define AQM_CMD_INFX(x)                 (0x1300800 + ((x) * 0x8))
#define AQM_GRP_EXECMSK_LOX(x)          (0x1300C00 + ((x) * 0x10))
#define AQM_GRP_EXECMSK_HIX(x)          (0x1300C08 + ((x) * 0x10))
#define AQM_ACTIVITY_STAT_LO            0x1300C80
#define AQM_ACTIVITY_STAT_HI            0x1300C88
#define AQM_Q_CMD_PROCX(x)              (0x1301000 + ((x) * 0x8))
#define AQM_PERF_CTL_LO                 0x1301400
#define AQM_PERF_CTL_HI                 0x1301408
#define AQM_PERF_CNT                    0x1301410
#define AQMQ_DRBLX(x)                   (0x20000 + ((x) * 0x40000))
#define AQMQ_QSZX(x)                    (0x20008 + ((x) * 0x40000))
#define AQMQ_BADRX(x)                   (0x20010 + ((x) * 0x40000))
#define AQMQ_NXT_CMDX(x)                (0x20018 + ((x) * 0x40000))
#define AQMQ_CMD_CNTX(x)                (0x20020 + ((x) * 0x40000))
#define AQMQ_CMP_THRX(x)                (0x20028 + ((x) * 0x40000))
#define AQMQ_CMP_CNTX(x)                (0x20030 + ((x) * 0x40000))
#define AQMQ_TIM_LDX(x)                 (0x20038 + ((x) * 0x40000))
#define AQMQ_TIMERX(x)                  (0x20040 + ((x) * 0x40000))
#define AQMQ_ENX(x)                     (0x20048 + ((x) * 0x40000))
#define AQMQ_ACTIVITY_STATX(x)          (0x20050 + ((x) * 0x40000))
#define AQM_VF_CMP_STATX(x)             (0x28000 + ((x) * 0x40000))
#define NPS_CORE_GBL_VFCFG	0x1000000
#define NPS_CORE_CONTROL	0x1000008
#define NPS_CORE_INT_ACTIVE	0x1000080
#define NPS_CORE_INT		0x10000A0
#define NPS_CORE_INT_ENA_W1S	0x10000B8
#define NPS_STATS_PKT_DMA_RD_CNT	0x1000180
#define NPS_STATS_PKT_DMA_WR_CNT	0x1000190
#define NPS_PKT_INT			0x1040018
#define NPS_PKT_MBOX_INT_LO		0x1040020
#define NPS_PKT_MBOX_INT_LO_ENA_W1C	0x1040030
#define NPS_PKT_MBOX_INT_LO_ENA_W1S	0x1040038
#define NPS_PKT_MBOX_INT_HI		0x1040040
#define NPS_PKT_MBOX_INT_HI_ENA_W1C	0x1040050
#define NPS_PKT_MBOX_INT_HI_ENA_W1S	0x1040058
#define NPS_PKT_IN_RERR_HI		0x1040108
#define NPS_PKT_IN_RERR_HI_ENA_W1S	0x1040120
#define NPS_PKT_IN_RERR_LO		0x1040128
#define NPS_PKT_IN_RERR_LO_ENA_W1S	0x1040140
#define NPS_PKT_IN_ERR_TYPE		0x1040148
#define NPS_PKT_IN_ERR_TYPE_ENA_W1S	0x1040160
#define NPS_PKT_IN_INSTR_CTLX(_i)	(0x10060 + ((_i) * 0x40000))
#define NPS_PKT_IN_INSTR_BADDRX(_i)	(0x10068 + ((_i) * 0x40000))
#define NPS_PKT_IN_INSTR_RSIZEX(_i)	(0x10070 + ((_i) * 0x40000))
#define NPS_PKT_IN_DONE_CNTSX(_i)	(0x10080 + ((_i) * 0x40000))
#define NPS_PKT_IN_INSTR_BAOFF_DBELLX(_i)	(0x10078 + ((_i) * 0x40000))
#define NPS_PKT_IN_INT_LEVELSX(_i)		(0x10088 + ((_i) * 0x40000))
#define NPS_PKT_SLC_RERR_HI		0x1040208
#define NPS_PKT_SLC_RERR_HI_ENA_W1S	0x1040220
#define NPS_PKT_SLC_RERR_LO		0x1040228
#define NPS_PKT_SLC_RERR_LO_ENA_W1S	0x1040240
#define NPS_PKT_SLC_ERR_TYPE		0x1040248
#define NPS_PKT_SLC_ERR_TYPE_ENA_W1S	0x1040260
#define NPS_PKT_MBOX_PF_VF_PFDATAX(_i)	(0x1040800 + ((_i) * 0x8))
#define NPS_PKT_MBOX_VF_PF_PFDATAX(_i)	(0x1040C00 + ((_i) * 0x8))
#define NPS_PKT_SLC_CTLX(_i)		(0x10000 + ((_i) * 0x40000))
#define NPS_PKT_SLC_CNTSX(_i)		(0x10008 + ((_i) * 0x40000))
#define NPS_PKT_SLC_INT_LEVELSX(_i)	(0x10010 + ((_i) * 0x40000))
#define POM_INT_ENA_W1S		0x11C0018
#define POM_GRP_EXECMASKX(_i)	(0x11C1100 | ((_i) * 8))
#define POM_INT		0x11C0000
#define POM_PERF_CTL	0x11CC400
#define BMI_INT		0x1140000
#define BMI_CTL		0x1140020
#define BMI_INT_ENA_W1S	0x1140018
#define BMI_NPS_PKT_CNT	0x1140070
#define EFL_CORE_INT_ENA_W1SX(_i)		(0x1240018 + ((_i) * 0x400))
#define EFL_CORE_VF_ERR_INT0X(_i)		(0x1240050 + ((_i) * 0x400))
#define EFL_CORE_VF_ERR_INT0_ENA_W1SX(_i)	(0x1240068 + ((_i) * 0x400))
#define EFL_CORE_VF_ERR_INT1X(_i)		(0x1240070 + ((_i) * 0x400))
#define EFL_CORE_VF_ERR_INT1_ENA_W1SX(_i)	(0x1240088 + ((_i) * 0x400))
#define EFL_CORE_SE_ERR_INTX(_i)		(0x12400A0 + ((_i) * 0x400))
#define EFL_RNM_CTL_STATUS			0x1241800
#define EFL_CORE_INTX(_i)			(0x1240000 + ((_i) * 0x400))
#define BMO_CTL2		0x1180028
#define BMO_NPS_SLC_PKT_CNT	0x1180078
#define LBC_INT			0x1200000
#define LBC_INVAL_CTL		0x1201010
#define LBC_PLM_VF1_64_INT	0x1202008
#define LBC_INVAL_STATUS	0x1202010
#define LBC_INT_ENA_W1S		0x1203000
#define LBC_PLM_VF1_64_INT_ENA_W1S	0x1205008
#define LBC_PLM_VF65_128_INT		0x1206008
#define LBC_ELM_VF1_64_INT		0x1208000
#define LBC_PLM_VF65_128_INT_ENA_W1S	0x1209008
#define LBC_ELM_VF1_64_INT_ENA_W1S	0x120B000
#define LBC_ELM_VF65_128_INT		0x120C000
#define LBC_ELM_VF65_128_INT_ENA_W1S	0x120F000
#define RST_BOOT	0x10C1600
#define FUS_DAT1	0x10C1408
#define PEM0_INT 0x1080428
union ucd_core_eid_ucode_block_num {
	u64 value;
	struct {
#if (defined(__BIG_ENDIAN_BITFIELD))
		u64 raz_4_63 : 60;
		u64 ucode_len : 1;
		u64 ucode_blk : 3;
#else
		u64 ucode_blk : 3;
		u64 ucode_len : 1;
		u64 raz_4_63 : 60;
#endif
	};
};
union aqm_grp_execmsk_lo {
	u64 value;
	struct {
#if (defined(__BIG_ENDIAN_BITFIELD))
		u64 raz_40_63 : 24;
		u64 exec_0_to_39 : 40;
#else
		u64 exec_0_to_39 : 40;
		u64 raz_40_63 : 24;
#endif
	};
};
union aqm_grp_execmsk_hi {
	u64 value;
	struct {
#if (defined(__BIG_ENDIAN_BITFIELD))
		u64 raz_40_63 : 24;
		u64 exec_40_to_79 : 40;
#else
		u64 exec_40_to_79 : 40;
		u64 raz_40_63 : 24;
#endif
	};
};
union aqmq_drbl {
	u64 value;
	struct {
#if (defined(__BIG_ENDIAN_BITFIELD))
		u64 raz_32_63 : 32;
		u64 dbell_count : 32;
#else
		u64 dbell_count : 32;
		u64 raz_32_63 : 32;
#endif
	};
};
union aqmq_qsz {
	u64 value;
	struct {
#if (defined(__BIG_ENDIAN_BITFIELD))
		u64 raz_32_63 : 32;
		u64 host_queue_size : 32;
#else
		u64 host_queue_size : 32;
		u64 raz_32_63 : 32;
#endif
	};
};
union aqmq_cmp_thr {
	u64 value;
	struct {
#if (defined(__BIG_ENDIAN_BITFIELD))
		u64 raz_32_63 : 32;
		u64 commands_completed_threshold : 32;
#else
		u64 commands_completed_threshold : 32;
		u64 raz_32_63 : 32;
#endif
	};
};
union aqmq_cmp_cnt {
	u64 value;
	struct {
#if (defined(__BIG_ENDIAN_BITFIELD))
		u64 raz_34_63 : 30;
		u64 resend : 1;
		u64 completion_status : 1;
		u64 commands_completed_count : 32;
#else
		u64 commands_completed_count : 32;
		u64 completion_status : 1;
		u64 resend : 1;
		u64 raz_34_63 : 30;
#endif
	};
};
union aqmq_en {
	u64 value;
	struct {
#if (defined(__BIG_ENDIAN_BITFIELD))
		u64 raz_1_63 : 63;
		u64 queue_enable : 1;
#else
		u64 queue_enable : 1;
		u64 raz_1_63 : 63;
#endif
	};
};
union aqmq_activity_stat {
	u64 value;
	struct {
#if (defined(__BIG_ENDIAN_BITFIELD))
		u64 raz_1_63 : 63;
		u64 queue_active : 1;
#else
		u64 queue_active : 1;
		u64 raz_1_63 : 63;
#endif
	};
};
union emu_fuse_map {
	u64 value;
	struct {
#if (defined(__BIG_ENDIAN_BITFIELD))
		u64 valid : 1;
		u64 raz_52_62 : 11;
		u64 ae_fuse : 20;
		u64 raz_16_31 : 16;
		u64 se_fuse : 16;
#else
		u64 se_fuse : 16;
		u64 raz_16_31 : 16;
		u64 ae_fuse : 20;
		u64 raz_52_62 : 11;
		u64 valid : 1;
#endif
	} s;
};
union emu_se_enable {
	u64 value;
	struct {
#if (defined(__BIG_ENDIAN_BITFIELD))
		u64 raz	: 48;
		u64 enable : 16;
#else
		u64 enable : 16;
		u64 raz	: 48;
#endif
	} s;
};
union emu_ae_enable {
	u64 value;
	struct {
#if (defined(__BIG_ENDIAN_BITFIELD))
		u64 raz	: 44;
		u64 enable : 20;
#else
		u64 enable : 20;
		u64 raz	: 44;
#endif
	} s;
};
union emu_wd_int_ena_w1s {
	u64 value;
	struct {
#if (defined(__BIG_ENDIAN_BITFIELD))
		u64 raz2 : 12;
		u64 ae_wd : 20;
		u64 raz1 : 16;
		u64 se_wd : 16;
#else
		u64 se_wd : 16;
		u64 raz1 : 16;
		u64 ae_wd : 20;
		u64 raz2 : 12;
#endif
	} s;
};
union emu_ge_int_ena_w1s {
	u64 value;
	struct {
#if (defined(__BIG_ENDIAN_BITFIELD))
		u64 raz_52_63 : 12;
		u64 ae_ge : 20;
		u64 raz_16_31: 16;
		u64 se_ge : 16;
#else
		u64 se_ge : 16;
		u64 raz_16_31: 16;
		u64 ae_ge : 20;
		u64 raz_52_63 : 12;
#endif
	} s;
};
union nps_pkt_slc_ctl {
	u64 value;
	struct {
#if defined(__BIG_ENDIAN_BITFIELD)
		u64 raz : 61;
		u64 rh : 1;
		u64 z : 1;
		u64 enb : 1;
#else
		u64 enb : 1;
		u64 z : 1;
		u64 rh : 1;
		u64 raz : 61;
#endif
	} s;
};
union nps_pkt_slc_cnts {
	u64 value;
	struct {
#if defined(__BIG_ENDIAN_BITFIELD)
		u64 slc_int : 1;
		u64 uns_int : 1;
		u64 in_int : 1;
		u64 mbox_int : 1;
		u64 resend : 1;
		u64 raz : 5;
		u64 timer : 22;
		u64 cnt : 32;
#else
		u64 cnt	: 32;
		u64 timer : 22;
		u64 raz	: 5;
		u64 resend : 1;
		u64 mbox_int : 1;
		u64 in_int : 1;
		u64 uns_int : 1;
		u64 slc_int : 1;
#endif
	} s;
};
union nps_pkt_slc_int_levels {
	u64 value;
	struct {
#if defined(__BIG_ENDIAN_BITFIELD)
		u64 bmode : 1;
		u64 raz	: 9;
		u64 timet : 22;
		u64 cnt	: 32;
#else
		u64 cnt : 32;
		u64 timet : 22;
		u64 raz : 9;
		u64 bmode : 1;
#endif
	} s;
};
union nps_pkt_int {
	u64 value;
	struct {
#if defined(__BIG_ENDIAN_BITFIELD)
		u64 raz	: 54;
		u64 uns_wto : 1;
		u64 in_err : 1;
		u64 uns_err : 1;
		u64 slc_err : 1;
		u64 in_dbe : 1;
		u64 in_sbe : 1;
		u64 uns_dbe : 1;
		u64 uns_sbe : 1;
		u64 slc_dbe : 1;
		u64 slc_sbe : 1;
#else
		u64 slc_sbe : 1;
		u64 slc_dbe : 1;
		u64 uns_sbe : 1;
		u64 uns_dbe : 1;
		u64 in_sbe : 1;
		u64 in_dbe : 1;
		u64 slc_err : 1;
		u64 uns_err : 1;
		u64 in_err : 1;
		u64 uns_wto : 1;
		u64 raz	: 54;
#endif
	} s;
};
union nps_pkt_in_done_cnts {
	u64 value;
	struct {
#if defined(__BIG_ENDIAN_BITFIELD)
		u64 slc_int : 1;
		u64 uns_int : 1;
		u64 in_int : 1;
		u64 mbox_int : 1;
		u64 resend : 1;
		u64 raz : 27;
		u64 cnt	: 32;
#else
		u64 cnt	: 32;
		u64 raz	: 27;
		u64 resend : 1;
		u64 mbox_int : 1;
		u64 in_int : 1;
		u64 uns_int : 1;
		u64 slc_int : 1;
#endif
	} s;
};
union nps_pkt_in_instr_ctl {
	u64 value;
	struct {
#if (defined(__BIG_ENDIAN_BITFIELD))
		u64 raz	: 62;
		u64 is64b : 1;
		u64 enb	: 1;
#else
		u64 enb	: 1;
		u64 is64b : 1;
		u64 raz : 62;
#endif
	} s;
};
union nps_pkt_in_instr_rsize {
	u64 value;
	struct {
#if (defined(__BIG_ENDIAN_BITFIELD))
		u64 raz	: 32;
		u64 rsize : 32;
#else
		u64 rsize : 32;
		u64 raz	: 32;
#endif
	} s;
};
union nps_pkt_in_instr_baoff_dbell {
	u64 value;
	struct {
#if (defined(__BIG_ENDIAN_BITFIELD))
		u64 aoff : 32;
		u64 dbell : 32;
#else
		u64 dbell : 32;
		u64 aoff : 32;
#endif
	} s;
};
union nps_core_int_ena_w1s {
	u64 value;
	struct {
#if (defined(__BIG_ENDIAN_BITFIELD))
		u64 raz4 : 55;
		u64 host_nps_wr_err : 1;
		u64 npco_dma_malform : 1;
		u64 exec_wr_timeout : 1;
		u64 host_wr_timeout : 1;
		u64 host_wr_err : 1;
		u64 raz3 : 1;
		u64 raz2 : 1;
		u64 raz1 : 1;
		u64 raz0 : 1;
#else
		u64 raz0 : 1;
		u64 raz1 : 1;
		u64 raz2 : 1;
		u64 raz3 : 1;
		u64 host_wr_err	: 1;
		u64 host_wr_timeout : 1;
		u64 exec_wr_timeout : 1;
		u64 npco_dma_malform : 1;
		u64 host_nps_wr_err : 1;
		u64 raz4 : 55;
#endif
	} s;
};
union nps_core_gbl_vfcfg {
	u64 value;
	struct {
#if (defined(__BIG_ENDIAN_BITFIELD))
		u64  raz :55;
		u64  ilk_disable :1;
		u64  obaf :1;
		u64  ibaf :1;
		u64  zaf :1;
		u64  aeaf :1;
		u64  seaf :1;
		u64  cfg :3;
#else
		u64  cfg :3;
		u64  seaf :1;
		u64  aeaf :1;
		u64  zaf :1;
		u64  ibaf :1;
		u64  obaf :1;
		u64  ilk_disable :1;
		u64  raz :55;
#endif
	} s;
};
union nps_core_int_active {
	u64 value;
	struct {
#if (defined(__BIG_ENDIAN_BITFIELD))
		u64 resend : 1;
		u64 raz	: 43;
		u64 ocla : 1;
		u64 mbox : 1;
		u64 emu	: 4;
		u64 bmo	: 1;
		u64 bmi	: 1;
		u64 aqm	: 1;
		u64 zqm	: 1;
		u64 efl	: 1;
		u64 ilk	: 1;
		u64 lbc	: 1;
		u64 pem	: 1;
		u64 pom	: 1;
		u64 ucd	: 1;
		u64 zctl : 1;
		u64 lbm	: 1;
		u64 nps_pkt : 1;
		u64 nps_core : 1;
#else
		u64 nps_core : 1;
		u64 nps_pkt : 1;
		u64 lbm	: 1;
		u64 zctl: 1;
		u64 ucd	: 1;
		u64 pom	: 1;
		u64 pem	: 1;
		u64 lbc	: 1;
		u64 ilk	: 1;
		u64 efl	: 1;
		u64 zqm	: 1;
		u64 aqm	: 1;
		u64 bmi	: 1;
		u64 bmo	: 1;
		u64 emu	: 4;
		u64 mbox : 1;
		u64 ocla : 1;
		u64 raz	: 43;
		u64 resend : 1;
#endif
	} s;
};
union efl_core_int {
	u64 value;
	struct {
#if (defined(__BIG_ENDIAN_BITFIELD))
		u64 raz	: 57;
		u64 epci_decode_err : 1;
		u64 ae_err : 1;
		u64 se_err : 1;
		u64 dbe	: 1;
		u64 sbe	: 1;
		u64 d_left : 1;
		u64 len_ovr : 1;
#else
		u64 len_ovr : 1;
		u64 d_left : 1;
		u64 sbe	: 1;
		u64 dbe	: 1;
		u64 se_err : 1;
		u64 ae_err : 1;
		u64 epci_decode_err  : 1;
		u64 raz	: 57;
#endif
	} s;
};
union efl_core_int_ena_w1s {
	u64 value;
	struct {
#if (defined(__BIG_ENDIAN_BITFIELD))
		u64 raz_7_63 : 57;
		u64 epci_decode_err : 1;
		u64 raz_2_5 : 4;
		u64 d_left : 1;
		u64 len_ovr : 1;
#else
		u64 len_ovr : 1;
		u64 d_left : 1;
		u64 raz_2_5 : 4;
		u64 epci_decode_err : 1;
		u64 raz_7_63 : 57;
#endif
	} s;
};
union efl_rnm_ctl_status {
	u64 value;
	struct {
#if (defined(__BIG_ENDIAN_BITFIELD))
		u64 raz_9_63 : 55;
		u64 ent_sel : 4;
		u64 exp_ent : 1;
		u64 rng_rst : 1;
		u64 rnm_rst : 1;
		u64 rng_en : 1;
		u64 ent_en : 1;
#else
		u64 ent_en : 1;
		u64 rng_en : 1;
		u64 rnm_rst : 1;
		u64 rng_rst : 1;
		u64 exp_ent : 1;
		u64 ent_sel : 4;
		u64 raz_9_63 : 55;
#endif
	} s;
};
union bmi_ctl {
	u64 value;
	struct {
#if (defined(__BIG_ENDIAN_BITFIELD))
		u64 raz_56_63 : 8;
		u64 ilk_hdrq_thrsh : 8;
		u64 nps_hdrq_thrsh : 8;
		u64 totl_hdrq_thrsh : 8;
		u64 ilk_free_thrsh : 8;
		u64 nps_free_thrsh : 8;
		u64 totl_free_thrsh : 8;
		u64 max_pkt_len : 8;
#else
		u64 max_pkt_len : 8;
		u64 totl_free_thrsh : 8;
		u64 nps_free_thrsh : 8;
		u64 ilk_free_thrsh : 8;
		u64 totl_hdrq_thrsh : 8;
		u64 nps_hdrq_thrsh : 8;
		u64 ilk_hdrq_thrsh : 8;
		u64 raz_56_63 : 8;
#endif
	} s;
};
union bmi_int_ena_w1s {
	u64 value;
	struct {
#if (defined(__BIG_ENDIAN_BITFIELD))
		u64 raz_13_63	: 51;
		u64 ilk_req_oflw : 1;
		u64 nps_req_oflw : 1;
		u64 raz_10 : 1;
		u64 raz_9 : 1;
		u64 fpf_undrrn	: 1;
		u64 eop_err_ilk	: 1;
		u64 eop_err_nps	: 1;
		u64 sop_err_ilk	: 1;
		u64 sop_err_nps	: 1;
		u64 pkt_rcv_err_ilk : 1;
		u64 pkt_rcv_err_nps : 1;
		u64 max_len_err_ilk : 1;
		u64 max_len_err_nps : 1;
#else
		u64 max_len_err_nps : 1;
		u64 max_len_err_ilk : 1;
		u64 pkt_rcv_err_nps : 1;
		u64 pkt_rcv_err_ilk : 1;
		u64 sop_err_nps	: 1;
		u64 sop_err_ilk	: 1;
		u64 eop_err_nps	: 1;
		u64 eop_err_ilk	: 1;
		u64 fpf_undrrn	: 1;
		u64 raz_9 : 1;
		u64 raz_10 : 1;
		u64 nps_req_oflw : 1;
		u64 ilk_req_oflw : 1;
		u64 raz_13_63 : 51;
#endif
	} s;
};
union bmo_ctl2 {
	u64 value;
	struct {
#if (defined(__BIG_ENDIAN_BITFIELD))
		u64 arb_sel : 1;
		u64 raz_32_62 : 31;
		u64 ilk_buf_thrsh : 8;
		u64 nps_slc_buf_thrsh : 8;
		u64 nps_uns_buf_thrsh : 8;
		u64 totl_buf_thrsh : 8;
#else
		u64 totl_buf_thrsh : 8;
		u64 nps_uns_buf_thrsh : 8;
		u64 nps_slc_buf_thrsh : 8;
		u64 ilk_buf_thrsh : 8;
		u64 raz_32_62 : 31;
		u64 arb_sel : 1;
#endif
	} s;
};
union pom_int_ena_w1s {
	u64 value;
	struct {
#if (defined(__BIG_ENDIAN_BITFIELD))
		u64 raz2 : 60;
		u64 illegal_intf : 1;
		u64 illegal_dport : 1;
		u64 raz1 : 1;
		u64 raz0 : 1;
#else
		u64 raz0 : 1;
		u64 raz1 : 1;
		u64 illegal_dport : 1;
		u64 illegal_intf : 1;
		u64 raz2 : 60;
#endif
	} s;
};
union lbc_inval_ctl {
	u64 value;
	struct {
#if (defined(__BIG_ENDIAN_BITFIELD))
		u64 raz2 : 48;
		u64 wait_timer : 8;
		u64 raz1 : 6;
		u64 cam_inval_start : 1;
		u64 raz0 : 1;
#else
		u64 raz0 : 1;
		u64 cam_inval_start : 1;
		u64 raz1 : 6;
		u64 wait_timer : 8;
		u64 raz2 : 48;
#endif
	} s;
};
union lbc_int_ena_w1s {
	u64 value;
	struct {
#if (defined(__BIG_ENDIAN_BITFIELD))
		u64 raz_10_63 : 54;
		u64 cam_hard_err : 1;
		u64 cam_inval_abort : 1;
		u64 over_fetch_err : 1;
		u64 cache_line_to_err : 1;
		u64 raz_2_5 : 4;
		u64 cam_soft_err : 1;
		u64 dma_rd_err : 1;
#else
		u64 dma_rd_err : 1;
		u64 cam_soft_err : 1;
		u64 raz_2_5 : 4;
		u64 cache_line_to_err : 1;
		u64 over_fetch_err : 1;
		u64 cam_inval_abort : 1;
		u64 cam_hard_err : 1;
		u64 raz_10_63 : 54;
#endif
	} s;
};
union lbc_int {
	u64 value;
	struct {
#if (defined(__BIG_ENDIAN_BITFIELD))
		u64 raz_10_63 : 54;
		u64 cam_hard_err : 1;
		u64 cam_inval_abort : 1;
		u64 over_fetch_err : 1;
		u64 cache_line_to_err : 1;
		u64 sbe : 1;
		u64 dbe	: 1;
		u64 pref_dat_len_mismatch_err : 1;
		u64 rd_dat_len_mismatch_err : 1;
		u64 cam_soft_err : 1;
		u64 dma_rd_err : 1;
#else
		u64 dma_rd_err : 1;
		u64 cam_soft_err : 1;
		u64 rd_dat_len_mismatch_err : 1;
		u64 pref_dat_len_mismatch_err : 1;
		u64 dbe	: 1;
		u64 sbe	: 1;
		u64 cache_line_to_err : 1;
		u64 over_fetch_err : 1;
		u64 cam_inval_abort : 1;
		u64 cam_hard_err : 1;
		u64 raz_10_63 : 54;
#endif
	} s;
};
union lbc_inval_status {
	u64 value;
	struct {
#if (defined(__BIG_ENDIAN_BITFIELD))
		u64 raz3 : 23;
		u64 cam_clean_entry_complete_cnt : 9;
		u64 raz2 : 7;
		u64 cam_clean_entry_cnt : 9;
		u64 raz1 : 5;
		u64 cam_inval_state : 3;
		u64 raz0 : 5;
		u64 cam_inval_abort : 1;
		u64 cam_rst_rdy	: 1;
		u64 done : 1;
#else
		u64 done : 1;
		u64 cam_rst_rdy : 1;
		u64 cam_inval_abort : 1;
		u64 raz0 : 5;
		u64 cam_inval_state : 3;
		u64 raz1 : 5;
		u64 cam_clean_entry_cnt : 9;
		u64 raz2 : 7;
		u64 cam_clean_entry_complete_cnt : 9;
		u64 raz3 : 23;
#endif
	} s;
};
union rst_boot {
	u64 value;
	struct {
#if (defined(__BIG_ENDIAN_BITFIELD))
		u64 raz_63 : 1;
		u64 jtcsrdis : 1;
		u64 raz_59_61 : 3;
		u64 jt_tst_mode : 1;
		u64 raz_40_57 : 18;
		u64 io_supply : 3;
		u64 raz_30_36 : 7;
		u64 pnr_mul : 6;
		u64 raz_12_23 : 12;
		u64 lboot : 10;
		u64 rboot : 1;
		u64 rboot_pin : 1;
#else
		u64 rboot_pin : 1;
		u64 rboot : 1;
		u64 lboot : 10;
		u64 raz_12_23 : 12;
		u64 pnr_mul : 6;
		u64 raz_30_36 : 7;
		u64 io_supply : 3;
		u64 raz_40_57 : 18;
		u64 jt_tst_mode : 1;
		u64 raz_59_61 : 3;
		u64 jtcsrdis : 1;
		u64 raz_63 : 1;
#endif
	};
};
union fus_dat1 {
	u64 value;
	struct {
#if (defined(__BIG_ENDIAN_BITFIELD))
		u64 raz_57_63 : 7;
		u64 pll_mul : 3;
		u64 pll_half_dis : 1;
		u64 raz_43_52 : 10;
		u64 efus_lck : 3;
		u64 raz_26_39 : 14;
		u64 zip_info : 5;
		u64 bar2_sz_conf : 1;
		u64 efus_ign : 1;
		u64 nozip : 1;
		u64 raz_11_17 : 7;
		u64 pll_alt_matrix : 1;
		u64 pll_bwadj_denom : 2;
		u64 chip_id : 8;
#else
		u64 chip_id : 8;
		u64 pll_bwadj_denom : 2;
		u64 pll_alt_matrix : 1;
		u64 raz_11_17 : 7;
		u64 nozip : 1;
		u64 efus_ign : 1;
		u64 bar2_sz_conf : 1;
		u64 zip_info : 5;
		u64 raz_26_39 : 14;
		u64 efus_lck : 3;
		u64 raz_43_52 : 10;
		u64 pll_half_dis : 1;
		u64 pll_mul : 3;
		u64 raz_57_63 : 7;
#endif
	};
};
#endif  
