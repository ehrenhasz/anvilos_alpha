#ifndef _MVS94XX_REG_H_
#define _MVS94XX_REG_H_
#include <linux/types.h>
#define MAX_LINK_RATE		SAS_LINK_RATE_6_0_GBPS
enum VANIR_REVISION_ID {
	VANIR_A0_REV		= 0xA0,
	VANIR_B0_REV		= 0x01,
	VANIR_C0_REV		= 0x02,
	VANIR_C1_REV		= 0x03,
	VANIR_C2_REV		= 0xC2,
};
enum host_registers {
	MVS_HST_CHIP_CONFIG	= 0x10104,	 
};
enum hw_registers {
	MVS_GBL_CTL		= 0x04,   
	MVS_GBL_INT_STAT	= 0x00,   
	MVS_GBL_PI		= 0x0C,   
	MVS_PHY_CTL		= 0x40,   
	MVS_PORTS_IMP		= 0x9C,   
	MVS_GBL_PORT_TYPE	= 0xa0,   
	MVS_CTL			= 0x100,  
	MVS_PCS			= 0x104,  
	MVS_CMD_LIST_LO		= 0x108,  
	MVS_CMD_LIST_HI		= 0x10C,
	MVS_RX_FIS_LO		= 0x110,  
	MVS_RX_FIS_HI		= 0x114,
	MVS_STP_REG_SET_0	= 0x118,  
	MVS_STP_REG_SET_1	= 0x11C,
	MVS_TX_CFG		= 0x120,  
	MVS_TX_LO		= 0x124,  
	MVS_TX_HI		= 0x128,
	MVS_TX_PROD_IDX		= 0x12C,  
	MVS_TX_CONS_IDX		= 0x130,  
	MVS_RX_CFG		= 0x134,  
	MVS_RX_LO		= 0x138,  
	MVS_RX_HI		= 0x13C,
	MVS_RX_CONS_IDX		= 0x140,  
	MVS_INT_COAL		= 0x148,  
	MVS_INT_COAL_TMOUT	= 0x14C,  
	MVS_INT_STAT		= 0x150,  
	MVS_INT_MASK		= 0x154,  
	MVS_INT_STAT_SRS_0	= 0x158,  
	MVS_INT_MASK_SRS_0	= 0x15C,
	MVS_INT_STAT_SRS_1	= 0x160,
	MVS_INT_MASK_SRS_1	= 0x164,
	MVS_NON_NCQ_ERR_0	= 0x168,  
	MVS_NON_NCQ_ERR_1	= 0x16C,
	MVS_CMD_ADDR		= 0x170,  
	MVS_CMD_DATA		= 0x174,  
	MVS_MEM_PARITY_ERR	= 0x178,  
	MVS_P0_INT_STAT		= 0x180,  
	MVS_P0_INT_MASK		= 0x184,  
	MVS_P4_INT_STAT		= 0x1A0,  
	MVS_P4_INT_MASK		= 0x1A4,  
	MVS_P0_SER_CTLSTAT	= 0x1D0,  
	MVS_P4_SER_CTLSTAT	= 0x1E0,  
	MVS_P0_CFG_ADDR		= 0x200,  
	MVS_P0_CFG_DATA		= 0x204,  
	MVS_P4_CFG_ADDR		= 0x220,  
	MVS_P4_CFG_DATA		= 0x224,  
	MVS_P0_VSR_ADDR		= 0x250,  
	MVS_P0_VSR_DATA		= 0x254,  
	MVS_P4_VSR_ADDR 	= 0x250,  
	MVS_P4_VSR_DATA 	= 0x254,  
	MVS_PA_VSR_ADDR		= 0x290,  
	MVS_PA_VSR_PORT		= 0x294,  
	MVS_COMMAND_ACTIVE	= 0x300,
};
enum pci_cfg_registers {
	PCR_PHY_CTL		= 0x40,
	PCR_PHY_CTL2		= 0x90,
	PCR_DEV_CTRL		= 0x78,
	PCR_LINK_STAT		= 0x82,
};
enum sas_sata_vsp_regs {
	VSR_PHY_STAT		= 0x00 * 4,  
	VSR_PHY_MODE1		= 0x01 * 4,  
	VSR_PHY_MODE2		= 0x02 * 4,  
	VSR_PHY_MODE3		= 0x03 * 4,  
	VSR_PHY_MODE4		= 0x04 * 4,  
	VSR_PHY_MODE5		= 0x05 * 4,  
	VSR_PHY_MODE6		= 0x06 * 4,  
	VSR_PHY_MODE7		= 0x07 * 4,  
	VSR_PHY_MODE8		= 0x08 * 4,  
	VSR_PHY_MODE9		= 0x09 * 4,  
	VSR_PHY_MODE10		= 0x0A * 4,  
	VSR_PHY_MODE11		= 0x0B * 4,  
	VSR_PHY_ACT_LED		= 0x0C * 4,  
	VSR_PHY_FFE_CONTROL	= 0x10C,
	VSR_PHY_DFE_UPDATE_CRTL	= 0x110,
	VSR_REF_CLOCK_CRTL	= 0x1A0,
};
enum chip_register_bits {
	PHY_MIN_SPP_PHYS_LINK_RATE_MASK = (0x7 << 8),
	PHY_MAX_SPP_PHYS_LINK_RATE_MASK = (0x7 << 12),
	PHY_NEG_SPP_PHYS_LINK_RATE_MASK_OFFSET = (16),
	PHY_NEG_SPP_PHYS_LINK_RATE_MASK =
			(0x3 << PHY_NEG_SPP_PHYS_LINK_RATE_MASK_OFFSET),
};
enum pci_interrupt_cause {
	MVS_IRQ_COM_IN_I2O_IOP0        = (1 << 0),
	MVS_IRQ_COM_IN_I2O_IOP1        = (1 << 1),
	MVS_IRQ_COM_IN_I2O_IOP2        = (1 << 2),
	MVS_IRQ_COM_IN_I2O_IOP3        = (1 << 3),
	MVS_IRQ_COM_OUT_I2O_HOS0       = (1 << 4),
	MVS_IRQ_COM_OUT_I2O_HOS1       = (1 << 5),
	MVS_IRQ_COM_OUT_I2O_HOS2       = (1 << 6),
	MVS_IRQ_COM_OUT_I2O_HOS3       = (1 << 7),
	MVS_IRQ_PCIF_TO_CPU_DRBL0      = (1 << 8),
	MVS_IRQ_PCIF_TO_CPU_DRBL1      = (1 << 9),
	MVS_IRQ_PCIF_TO_CPU_DRBL2      = (1 << 10),
	MVS_IRQ_PCIF_TO_CPU_DRBL3      = (1 << 11),
	MVS_IRQ_PCIF_DRBL0             = (1 << 12),
	MVS_IRQ_PCIF_DRBL1             = (1 << 13),
	MVS_IRQ_PCIF_DRBL2             = (1 << 14),
	MVS_IRQ_PCIF_DRBL3             = (1 << 15),
	MVS_IRQ_XOR_A                  = (1 << 16),
	MVS_IRQ_XOR_B                  = (1 << 17),
	MVS_IRQ_SAS_A                  = (1 << 18),
	MVS_IRQ_SAS_B                  = (1 << 19),
	MVS_IRQ_CPU_CNTRL              = (1 << 20),
	MVS_IRQ_GPIO                   = (1 << 21),
	MVS_IRQ_UART                   = (1 << 22),
	MVS_IRQ_SPI                    = (1 << 23),
	MVS_IRQ_I2C                    = (1 << 24),
	MVS_IRQ_SGPIO                  = (1 << 25),
	MVS_IRQ_COM_ERR                = (1 << 29),
	MVS_IRQ_I2O_ERR                = (1 << 30),
	MVS_IRQ_PCIE_ERR               = (1 << 31),
};
union reg_phy_cfg {
	u32 v;
	struct {
		u32 phy_reset:1;
		u32 sas_support:1;
		u32 sata_support:1;
		u32 sata_host_mode:1;
		u32 speed_support:3;
		u32 snw_3_support:1;
		u32 tx_lnk_parity:1;
		u32 tx_spt_phs_lnk_rate:6;
		u32 tx_lgcl_lnk_rate:4;
		u32 tx_ssc_type:1;
		u32 sata_spin_up_spt:1;
		u32 sata_spin_up_en:1;
		u32 bypass_oob:1;
		u32 disable_phy:1;
		u32 rsvd:8;
	} u;
};
#define MAX_SG_ENTRY		255
struct mvs_prd_imt {
#ifndef __BIG_ENDIAN
	__le32			len:22;
	u8			_r_a:2;
	u8			misc_ctl:4;
	u8			inter_sel:4;
#else
	u32			inter_sel:4;
	u32			misc_ctl:4;
	u32			_r_a:2;
	u32			len:22;
#endif
};
struct mvs_prd {
	__le64			addr;
	__le32			im_len;
} __attribute__ ((packed));
enum sgpio_registers {
	MVS_SGPIO_HOST_OFFSET	= 0x100,	 
	MVS_SGPIO_CFG0	= 0xc200,
	MVS_SGPIO_CFG0_ENABLE	= (1 << 0),	 
	MVS_SGPIO_CFG0_BLINKB	= (1 << 1),	 
	MVS_SGPIO_CFG0_BLINKA	= (1 << 2),
	MVS_SGPIO_CFG0_INVSCLK	= (1 << 3),	 
	MVS_SGPIO_CFG0_INVSLOAD	= (1 << 4),
	MVS_SGPIO_CFG0_INVSDOUT	= (1 << 5),
	MVS_SGPIO_CFG0_SLOAD_FALLEDGE = (1 << 6),	 
	MVS_SGPIO_CFG0_SDOUT_FALLEDGE = (1 << 7),
	MVS_SGPIO_CFG0_SDIN_RISEEDGE = (1 << 8),
	MVS_SGPIO_CFG0_MAN_BITLEN_SHIFT = 18,	 
	MVS_SGPIO_CFG0_AUT_BITLEN_SHIFT = 24,	 
	MVS_SGPIO_CFG1	= 0xc204,	 
	MVS_SGPIO_CFG1_LOWA_SHIFT	= 0,	 
	MVS_SGPIO_CFG1_HIA_SHIFT	= 4,	 
	MVS_SGPIO_CFG1_LOWB_SHIFT	= 8,	 
	MVS_SGPIO_CFG1_HIB_SHIFT	= 12,	 
	MVS_SGPIO_CFG1_MAXACTON_SHIFT	= 16,	 
	MVS_SGPIO_CFG1_FORCEACTOFF_SHIFT	= 20,
	MVS_SGPIO_CFG1_STRCHACTON_SHIFT	= 24,
	MVS_SGPIO_CFG1_STRCHACTOFF_SHIFT	= 28,
	MVS_SGPIO_CFG2	= 0xc208,	 
	MVS_SGPIO_CFG2_CLK_SHIFT	= 0,
	MVS_SGPIO_CFG2_BLINK_SHIFT	= 20,
	MVS_SGPIO_CTRL	= 0xc20c,	 
	MVS_SGPIO_CTRL_SDOUT_AUTO	= 2,
	MVS_SGPIO_CTRL_SDOUT_SHIFT	= 2,
	MVS_SGPIO_DSRC	= 0xc220,	 
	MVS_SGPIO_DCTRL	= 0xc238,
	MVS_SGPIO_DCTRL_ERR_SHIFT	= 0,
	MVS_SGPIO_DCTRL_LOC_SHIFT	= 3,
	MVS_SGPIO_DCTRL_ACT_SHIFT	= 5,
};
enum sgpio_led_status {
	LED_OFF	= 0,
	LED_ON	= 1,
	LED_BLINKA	= 2,
	LED_BLINKA_INV	= 3,
	LED_BLINKA_SOF	= 4,
	LED_BLINKA_EOF	= 5,
	LED_BLINKB	= 6,
	LED_BLINKB_INV	= 7,
};
#define DEFAULT_SGPIO_BITS ((LED_BLINKA_SOF << \
				MVS_SGPIO_DCTRL_ACT_SHIFT) << (8 * 3) | \
			(LED_BLINKA_SOF << \
				MVS_SGPIO_DCTRL_ACT_SHIFT) << (8 * 2) | \
			(LED_BLINKA_SOF << \
				MVS_SGPIO_DCTRL_ACT_SHIFT) << (8 * 1) | \
			(LED_BLINKA_SOF << \
				MVS_SGPIO_DCTRL_ACT_SHIFT) << (8 * 0))
enum sas_sata_phy_regs {
	GENERATION_1_SETTING		= 0x118,
	GENERATION_1_2_SETTING		= 0x11C,
	GENERATION_2_3_SETTING		= 0x120,
	GENERATION_3_4_SETTING		= 0x124,
};
#define SPI_CTRL_REG_94XX           	0xc800
#define SPI_ADDR_REG_94XX            	0xc804
#define SPI_WR_DATA_REG_94XX         0xc808
#define SPI_RD_DATA_REG_94XX         	0xc80c
#define SPI_CTRL_READ_94XX         	(1U << 2)
#define SPI_ADDR_VLD_94XX         	(1U << 1)
#define SPI_CTRL_SpiStart_94XX     	(1U << 0)
static inline int
mv_ffc64(u64 v)
{
	u64 x = ~v;
	return x ? __ffs64(x) : -1;
}
#define r_reg_set_enable(i) \
	(((i) > 31) ? mr32(MVS_STP_REG_SET_1) : \
	mr32(MVS_STP_REG_SET_0))
#define w_reg_set_enable(i, tmp) \
	(((i) > 31) ? mw32(MVS_STP_REG_SET_1, tmp) : \
	mw32(MVS_STP_REG_SET_0, tmp))
extern const struct mvs_dispatch mvs_94xx_dispatch;
#endif
