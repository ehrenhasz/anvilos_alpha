#ifndef __TI_CPSW_SL_H__
#define __TI_CPSW_SL_H__
#include <linux/device.h>
enum cpsw_sl_regs {
	CPSW_SL_IDVER,
	CPSW_SL_MACCONTROL,
	CPSW_SL_MACSTATUS,
	CPSW_SL_SOFT_RESET,
	CPSW_SL_RX_MAXLEN,
	CPSW_SL_BOFFTEST,
	CPSW_SL_RX_PAUSE,
	CPSW_SL_TX_PAUSE,
	CPSW_SL_EMCONTROL,
	CPSW_SL_RX_PRI_MAP,
	CPSW_SL_TX_GAP,
};
enum {
	CPSW_SL_CTL_FULLDUPLEX = BIT(0),  
	CPSW_SL_CTL_LOOPBACK = BIT(1),  
	CPSW_SL_CTL_MTEST = BIT(2),  
	CPSW_SL_CTL_RX_FLOW_EN = BIT(3),  
	CPSW_SL_CTL_TX_FLOW_EN = BIT(4),  
	CPSW_SL_CTL_GMII_EN = BIT(5),  
	CPSW_SL_CTL_TX_PACE = BIT(6),  
	CPSW_SL_CTL_GIG = BIT(7),  
	CPSW_SL_CTL_XGIG = BIT(8),  
	CPSW_SL_CTL_TX_SHORT_GAP_EN = BIT(10),  
	CPSW_SL_CTL_CMD_IDLE = BIT(11),  
	CPSW_SL_CTL_CRC_TYPE = BIT(12),  
	CPSW_SL_CTL_XGMII_EN = BIT(13),  
	CPSW_SL_CTL_IFCTL_A = BIT(15),  
	CPSW_SL_CTL_IFCTL_B = BIT(16),  
	CPSW_SL_CTL_GIG_FORCE = BIT(17),  
	CPSW_SL_CTL_EXT_EN = BIT(18),  
	CPSW_SL_CTL_EXT_EN_RX_FLO = BIT(19),  
	CPSW_SL_CTL_EXT_EN_TX_FLO = BIT(20),  
	CPSW_SL_CTL_TX_SG_LIM_EN = BIT(21),  
	CPSW_SL_CTL_RX_CEF_EN = BIT(22),  
	CPSW_SL_CTL_RX_CSF_EN = BIT(23),  
	CPSW_SL_CTL_RX_CMF_EN = BIT(24),  
	CPSW_SL_CTL_EXT_EN_XGIG = BIT(25),   
	CPSW_SL_CTL_FUNCS_COUNT
};
struct cpsw_sl;
struct cpsw_sl *cpsw_sl_get(const char *device_id, struct device *dev,
			    void __iomem *sl_base);
void cpsw_sl_reset(struct cpsw_sl *sl, unsigned long tmo);
u32 cpsw_sl_ctl_set(struct cpsw_sl *sl, u32 ctl_funcs);
u32 cpsw_sl_ctl_clr(struct cpsw_sl *sl, u32 ctl_funcs);
void cpsw_sl_ctl_reset(struct cpsw_sl *sl);
int cpsw_sl_wait_for_idle(struct cpsw_sl *sl, unsigned long tmo);
u32 cpsw_sl_reg_read(struct cpsw_sl *sl, enum cpsw_sl_regs reg);
void cpsw_sl_reg_write(struct cpsw_sl *sl, enum cpsw_sl_regs reg, u32 val);
#endif  
