#ifndef _FLEXCAN_H
#define _FLEXCAN_H
#include <linux/can/rx-offload.h>
#define FLEXCAN_QUIRK_BROKEN_WERR_STATE BIT(1)
#define FLEXCAN_QUIRK_DISABLE_RXFG BIT(2)
#define FLEXCAN_QUIRK_ENABLE_EACEN_RRS  BIT(3)
#define FLEXCAN_QUIRK_DISABLE_MECR BIT(4)
#define FLEXCAN_QUIRK_USE_RX_MAILBOX BIT(5)
#define FLEXCAN_QUIRK_BROKEN_PERR_STATE BIT(6)
#define FLEXCAN_QUIRK_DEFAULT_BIG_ENDIAN BIT(7)
#define FLEXCAN_QUIRK_SETUP_STOP_MODE_GPR BIT(8)
#define FLEXCAN_QUIRK_SUPPORT_FD BIT(9)
#define FLEXCAN_QUIRK_SUPPORT_ECC BIT(10)
#define FLEXCAN_QUIRK_SETUP_STOP_MODE_SCFW BIT(11)
#define FLEXCAN_QUIRK_NR_IRQ_3 BIT(12)
#define FLEXCAN_QUIRK_NR_MB_16 BIT(13)
#define FLEXCAN_QUIRK_SUPPORT_RX_MAILBOX BIT(14)
#define FLEXCAN_QUIRK_SUPPORT_RX_MAILBOX_RTR BIT(15)
#define FLEXCAN_QUIRK_SUPPORT_RX_FIFO BIT(16)
struct flexcan_devtype_data {
	u32 quirks;		 
};
struct flexcan_stop_mode {
	struct regmap *gpr;
	u8 req_gpr;
	u8 req_bit;
};
struct flexcan_priv {
	struct can_priv can;
	struct can_rx_offload offload;
	struct device *dev;
	struct flexcan_regs __iomem *regs;
	struct flexcan_mb __iomem *tx_mb;
	struct flexcan_mb __iomem *tx_mb_reserved;
	u8 tx_mb_idx;
	u8 mb_count;
	u8 mb_size;
	u8 clk_src;	 
	u8 scu_idx;
	u64 rx_mask;
	u64 tx_mask;
	u32 reg_ctrl_default;
	struct clk *clk_ipg;
	struct clk *clk_per;
	struct flexcan_devtype_data devtype_data;
	struct regulator *reg_xceiver;
	struct flexcan_stop_mode stm;
	int irq_boff;
	int irq_err;
	struct imx_sc_ipc *sc_ipc_handle;
	u32 (*read)(void __iomem *addr);
	void (*write)(u32 val, void __iomem *addr);
};
extern const struct ethtool_ops flexcan_ethtool_ops;
static inline bool
flexcan_supports_rx_mailbox(const struct flexcan_priv *priv)
{
	const u32 quirks = priv->devtype_data.quirks;
	return quirks & FLEXCAN_QUIRK_SUPPORT_RX_MAILBOX;
}
static inline bool
flexcan_supports_rx_mailbox_rtr(const struct flexcan_priv *priv)
{
	const u32 quirks = priv->devtype_data.quirks;
	return (quirks & (FLEXCAN_QUIRK_SUPPORT_RX_MAILBOX |
			  FLEXCAN_QUIRK_SUPPORT_RX_MAILBOX_RTR)) ==
		(FLEXCAN_QUIRK_SUPPORT_RX_MAILBOX |
		 FLEXCAN_QUIRK_SUPPORT_RX_MAILBOX_RTR);
}
static inline bool
flexcan_supports_rx_fifo(const struct flexcan_priv *priv)
{
	const u32 quirks = priv->devtype_data.quirks;
	return quirks & FLEXCAN_QUIRK_SUPPORT_RX_FIFO;
}
static inline bool
flexcan_active_rx_rtr(const struct flexcan_priv *priv)
{
	const u32 quirks = priv->devtype_data.quirks;
	if (quirks & FLEXCAN_QUIRK_USE_RX_MAILBOX) {
		if (quirks & FLEXCAN_QUIRK_SUPPORT_RX_MAILBOX_RTR)
			return true;
	} else {
		return true;
	}
	return false;
}
#endif  
