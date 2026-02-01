
 

#include <linux/bitfield.h>
#include <linux/clk.h>
#include <linux/errno.h>
#include <linux/ethtool.h>
#include <linux/init.h>
#include <linux/interrupt.h>
#include <linux/io.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/netdevice.h>
#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/platform_device.h>
#include <linux/skbuff.h>
#include <linux/spinlock.h>
#include <linux/string.h>
#include <linux/types.h>
#include <linux/can/dev.h>
#include <linux/can/error.h>
#include <linux/phy/phy.h>
#include <linux/pm_runtime.h>
#include <linux/reset.h>

#define DRIVER_NAME	"xilinx_can"

 
enum xcan_reg {
	XCAN_SRR_OFFSET		= 0x00,  
	XCAN_MSR_OFFSET		= 0x04,  
	XCAN_BRPR_OFFSET	= 0x08,  
	XCAN_BTR_OFFSET		= 0x0C,  
	XCAN_ECR_OFFSET		= 0x10,  
	XCAN_ESR_OFFSET		= 0x14,  
	XCAN_SR_OFFSET		= 0x18,  
	XCAN_ISR_OFFSET		= 0x1C,  
	XCAN_IER_OFFSET		= 0x20,  
	XCAN_ICR_OFFSET		= 0x24,  

	 
	XCAN_TXFIFO_OFFSET	= 0x30,  
	XCAN_RXFIFO_OFFSET	= 0x50,  
	XCAN_AFR_OFFSET		= 0x60,  

	 
	XCAN_F_BRPR_OFFSET	= 0x088,  
	XCAN_F_BTR_OFFSET	= 0x08C,  
	XCAN_TRR_OFFSET		= 0x0090,  
	XCAN_AFR_EXT_OFFSET	= 0x00E0,  
	XCAN_FSR_OFFSET		= 0x00E8,  
	XCAN_TXMSG_BASE_OFFSET	= 0x0100,  
	XCAN_RXMSG_BASE_OFFSET	= 0x1100,  
	XCAN_RXMSG_2_BASE_OFFSET	= 0x2100,  
	XCAN_AFR_2_MASK_OFFSET	= 0x0A00,  
	XCAN_AFR_2_ID_OFFSET	= 0x0A04,  
};

#define XCAN_FRAME_ID_OFFSET(frame_base)	((frame_base) + 0x00)
#define XCAN_FRAME_DLC_OFFSET(frame_base)	((frame_base) + 0x04)
#define XCAN_FRAME_DW1_OFFSET(frame_base)	((frame_base) + 0x08)
#define XCAN_FRAME_DW2_OFFSET(frame_base)	((frame_base) + 0x0C)
#define XCANFD_FRAME_DW_OFFSET(frame_base)	((frame_base) + 0x08)

#define XCAN_CANFD_FRAME_SIZE		0x48
#define XCAN_TXMSG_FRAME_OFFSET(n)	(XCAN_TXMSG_BASE_OFFSET + \
					 XCAN_CANFD_FRAME_SIZE * (n))
#define XCAN_RXMSG_FRAME_OFFSET(n)	(XCAN_RXMSG_BASE_OFFSET + \
					 XCAN_CANFD_FRAME_SIZE * (n))
#define XCAN_RXMSG_2_FRAME_OFFSET(n)	(XCAN_RXMSG_2_BASE_OFFSET + \
					 XCAN_CANFD_FRAME_SIZE * (n))

 
#define XCAN_TX_MAILBOX_IDX		0

 
#define XCAN_SRR_CEN_MASK		0x00000002  
#define XCAN_SRR_RESET_MASK		0x00000001  
#define XCAN_MSR_LBACK_MASK		0x00000002  
#define XCAN_MSR_SLEEP_MASK		0x00000001  
#define XCAN_BRPR_BRP_MASK		0x000000FF  
#define XCAN_BRPR_TDCO_MASK		GENMASK(12, 8)   
#define XCAN_2_BRPR_TDCO_MASK		GENMASK(13, 8)   
#define XCAN_BTR_SJW_MASK		0x00000180  
#define XCAN_BTR_TS2_MASK		0x00000070  
#define XCAN_BTR_TS1_MASK		0x0000000F  
#define XCAN_BTR_SJW_MASK_CANFD		0x000F0000  
#define XCAN_BTR_TS2_MASK_CANFD		0x00000F00  
#define XCAN_BTR_TS1_MASK_CANFD		0x0000003F  
#define XCAN_ECR_REC_MASK		0x0000FF00  
#define XCAN_ECR_TEC_MASK		0x000000FF  
#define XCAN_ESR_ACKER_MASK		0x00000010  
#define XCAN_ESR_BERR_MASK		0x00000008  
#define XCAN_ESR_STER_MASK		0x00000004  
#define XCAN_ESR_FMER_MASK		0x00000002  
#define XCAN_ESR_CRCER_MASK		0x00000001  
#define XCAN_SR_TDCV_MASK		GENMASK(22, 16)  
#define XCAN_SR_TXFLL_MASK		0x00000400  
#define XCAN_SR_ESTAT_MASK		0x00000180  
#define XCAN_SR_ERRWRN_MASK		0x00000040  
#define XCAN_SR_NORMAL_MASK		0x00000008  
#define XCAN_SR_LBACK_MASK		0x00000002  
#define XCAN_SR_CONFIG_MASK		0x00000001  
#define XCAN_IXR_RXMNF_MASK		0x00020000  
#define XCAN_IXR_TXFEMP_MASK		0x00004000  
#define XCAN_IXR_WKUP_MASK		0x00000800  
#define XCAN_IXR_SLP_MASK		0x00000400  
#define XCAN_IXR_BSOFF_MASK		0x00000200  
#define XCAN_IXR_ERROR_MASK		0x00000100  
#define XCAN_IXR_RXNEMP_MASK		0x00000080  
#define XCAN_IXR_RXOFLW_MASK		0x00000040  
#define XCAN_IXR_RXOK_MASK		0x00000010  
#define XCAN_IXR_TXFLL_MASK		0x00000004  
#define XCAN_IXR_TXOK_MASK		0x00000002  
#define XCAN_IXR_ARBLST_MASK		0x00000001  
#define XCAN_IDR_ID1_MASK		0xFFE00000  
#define XCAN_IDR_SRR_MASK		0x00100000  
#define XCAN_IDR_IDE_MASK		0x00080000  
#define XCAN_IDR_ID2_MASK		0x0007FFFE  
#define XCAN_IDR_RTR_MASK		0x00000001  
#define XCAN_DLCR_DLC_MASK		0xF0000000  
#define XCAN_FSR_FL_MASK		0x00003F00  
#define XCAN_2_FSR_FL_MASK		0x00007F00  
#define XCAN_FSR_IRI_MASK		0x00000080  
#define XCAN_FSR_RI_MASK		0x0000001F  
#define XCAN_2_FSR_RI_MASK		0x0000003F  
#define XCAN_DLCR_EDL_MASK		0x08000000  
#define XCAN_DLCR_BRS_MASK		0x04000000  

 
#define XCAN_BRPR_TDC_ENABLE		BIT(16)  
#define XCAN_BTR_SJW_SHIFT		7   
#define XCAN_BTR_TS2_SHIFT		4   
#define XCAN_BTR_SJW_SHIFT_CANFD	16  
#define XCAN_BTR_TS2_SHIFT_CANFD	8   
#define XCAN_IDR_ID1_SHIFT		21  
#define XCAN_IDR_ID2_SHIFT		1   
#define XCAN_DLCR_DLC_SHIFT		28  
#define XCAN_ESR_REC_SHIFT		8   

 
#define XCAN_FRAME_MAX_DATA_LEN		8
#define XCANFD_DW_BYTES			4
#define XCAN_TIMEOUT			(1 * HZ)

 
#define XCAN_FLAG_TXFEMP	0x0001
 
#define XCAN_FLAG_RXMNF		0x0002
 
#define XCAN_FLAG_EXT_FILTERS	0x0004
 
#define XCAN_FLAG_TX_MAILBOXES	0x0008
 
#define XCAN_FLAG_RX_FIFO_MULTI	0x0010
#define XCAN_FLAG_CANFD_2	0x0020

enum xcan_ip_type {
	XAXI_CAN = 0,
	XZYNQ_CANPS,
	XAXI_CANFD,
	XAXI_CANFD_2_0,
};

struct xcan_devtype_data {
	enum xcan_ip_type cantype;
	unsigned int flags;
	const struct can_bittiming_const *bittiming_const;
	const char *bus_clk_name;
	unsigned int btr_ts2_shift;
	unsigned int btr_sjw_shift;
};

 
struct xcan_priv {
	struct can_priv can;
	spinlock_t tx_lock;  
	unsigned int tx_head;
	unsigned int tx_tail;
	unsigned int tx_max;
	struct napi_struct napi;
	u32 (*read_reg)(const struct xcan_priv *priv, enum xcan_reg reg);
	void (*write_reg)(const struct xcan_priv *priv, enum xcan_reg reg,
			  u32 val);
	struct device *dev;
	void __iomem *reg_base;
	unsigned long irq_flags;
	struct clk *bus_clk;
	struct clk *can_clk;
	struct xcan_devtype_data devtype;
	struct phy *transceiver;
	struct reset_control *rstc;
};

 
static const struct can_bittiming_const xcan_bittiming_const = {
	.name = DRIVER_NAME,
	.tseg1_min = 1,
	.tseg1_max = 16,
	.tseg2_min = 1,
	.tseg2_max = 8,
	.sjw_max = 4,
	.brp_min = 1,
	.brp_max = 256,
	.brp_inc = 1,
};

 
static const struct can_bittiming_const xcan_bittiming_const_canfd = {
	.name = DRIVER_NAME,
	.tseg1_min = 1,
	.tseg1_max = 64,
	.tseg2_min = 1,
	.tseg2_max = 16,
	.sjw_max = 16,
	.brp_min = 1,
	.brp_max = 256,
	.brp_inc = 1,
};

 
static const struct can_bittiming_const xcan_data_bittiming_const_canfd = {
	.name = DRIVER_NAME,
	.tseg1_min = 1,
	.tseg1_max = 16,
	.tseg2_min = 1,
	.tseg2_max = 8,
	.sjw_max = 8,
	.brp_min = 1,
	.brp_max = 256,
	.brp_inc = 1,
};

 
static const struct can_bittiming_const xcan_bittiming_const_canfd2 = {
	.name = DRIVER_NAME,
	.tseg1_min = 1,
	.tseg1_max = 256,
	.tseg2_min = 1,
	.tseg2_max = 128,
	.sjw_max = 128,
	.brp_min = 1,
	.brp_max = 256,
	.brp_inc = 1,
};

 
static const struct can_bittiming_const xcan_data_bittiming_const_canfd2 = {
	.name = DRIVER_NAME,
	.tseg1_min = 1,
	.tseg1_max = 32,
	.tseg2_min = 1,
	.tseg2_max = 16,
	.sjw_max = 16,
	.brp_min = 1,
	.brp_max = 256,
	.brp_inc = 1,
};

 
static const struct can_tdc_const xcan_tdc_const_canfd = {
	.tdcv_min = 0,
	.tdcv_max = 0,  
	.tdco_min = 0,
	.tdco_max = 32,
	.tdcf_min = 0,  
	.tdcf_max = 0,
};

 
static const struct can_tdc_const xcan_tdc_const_canfd2 = {
	.tdcv_min = 0,
	.tdcv_max = 0,  
	.tdco_min = 0,
	.tdco_max = 64,
	.tdcf_min = 0,  
	.tdcf_max = 0,
};

 
static void xcan_write_reg_le(const struct xcan_priv *priv, enum xcan_reg reg,
			      u32 val)
{
	iowrite32(val, priv->reg_base + reg);
}

 
static u32 xcan_read_reg_le(const struct xcan_priv *priv, enum xcan_reg reg)
{
	return ioread32(priv->reg_base + reg);
}

 
static void xcan_write_reg_be(const struct xcan_priv *priv, enum xcan_reg reg,
			      u32 val)
{
	iowrite32be(val, priv->reg_base + reg);
}

 
static u32 xcan_read_reg_be(const struct xcan_priv *priv, enum xcan_reg reg)
{
	return ioread32be(priv->reg_base + reg);
}

 
static u32 xcan_rx_int_mask(const struct xcan_priv *priv)
{
	 
	if (priv->devtype.flags & XCAN_FLAG_RX_FIFO_MULTI)
		return XCAN_IXR_RXOK_MASK;
	else
		return XCAN_IXR_RXNEMP_MASK;
}

 
static int set_reset_mode(struct net_device *ndev)
{
	struct xcan_priv *priv = netdev_priv(ndev);
	unsigned long timeout;

	priv->write_reg(priv, XCAN_SRR_OFFSET, XCAN_SRR_RESET_MASK);

	timeout = jiffies + XCAN_TIMEOUT;
	while (!(priv->read_reg(priv, XCAN_SR_OFFSET) & XCAN_SR_CONFIG_MASK)) {
		if (time_after(jiffies, timeout)) {
			netdev_warn(ndev, "timed out for config mode\n");
			return -ETIMEDOUT;
		}
		usleep_range(500, 10000);
	}

	 
	priv->tx_head = 0;
	priv->tx_tail = 0;

	return 0;
}

 
static int xcan_set_bittiming(struct net_device *ndev)
{
	struct xcan_priv *priv = netdev_priv(ndev);
	struct can_bittiming *bt = &priv->can.bittiming;
	struct can_bittiming *dbt = &priv->can.data_bittiming;
	u32 btr0, btr1;
	u32 is_config_mode;

	 
	is_config_mode = priv->read_reg(priv, XCAN_SR_OFFSET) &
				XCAN_SR_CONFIG_MASK;
	if (!is_config_mode) {
		netdev_alert(ndev,
			     "BUG! Cannot set bittiming - CAN is not in config mode\n");
		return -EPERM;
	}

	 
	btr0 = (bt->brp - 1);

	 
	btr1 = (bt->prop_seg + bt->phase_seg1 - 1);

	 
	btr1 |= (bt->phase_seg2 - 1) << priv->devtype.btr_ts2_shift;

	 
	btr1 |= (bt->sjw - 1) << priv->devtype.btr_sjw_shift;

	priv->write_reg(priv, XCAN_BRPR_OFFSET, btr0);
	priv->write_reg(priv, XCAN_BTR_OFFSET, btr1);

	if (priv->devtype.cantype == XAXI_CANFD ||
	    priv->devtype.cantype == XAXI_CANFD_2_0) {
		 
		btr0 = dbt->brp - 1;
		if (can_tdc_is_enabled(&priv->can)) {
			if (priv->devtype.cantype == XAXI_CANFD)
				btr0 |= FIELD_PREP(XCAN_BRPR_TDCO_MASK, priv->can.tdc.tdco) |
					XCAN_BRPR_TDC_ENABLE;
			else
				btr0 |= FIELD_PREP(XCAN_2_BRPR_TDCO_MASK, priv->can.tdc.tdco) |
					XCAN_BRPR_TDC_ENABLE;
		}

		 
		btr1 = dbt->prop_seg + dbt->phase_seg1 - 1;

		 
		btr1 |= (dbt->phase_seg2 - 1) << priv->devtype.btr_ts2_shift;

		 
		btr1 |= (dbt->sjw - 1) << priv->devtype.btr_sjw_shift;

		priv->write_reg(priv, XCAN_F_BRPR_OFFSET, btr0);
		priv->write_reg(priv, XCAN_F_BTR_OFFSET, btr1);
	}

	netdev_dbg(ndev, "BRPR=0x%08x, BTR=0x%08x\n",
		   priv->read_reg(priv, XCAN_BRPR_OFFSET),
		   priv->read_reg(priv, XCAN_BTR_OFFSET));

	return 0;
}

 
static int xcan_chip_start(struct net_device *ndev)
{
	struct xcan_priv *priv = netdev_priv(ndev);
	u32 reg_msr;
	int err;
	u32 ier;

	 
	err = set_reset_mode(ndev);
	if (err < 0)
		return err;

	err = xcan_set_bittiming(ndev);
	if (err < 0)
		return err;

	 
	ier = XCAN_IXR_TXOK_MASK | XCAN_IXR_BSOFF_MASK |
		XCAN_IXR_WKUP_MASK | XCAN_IXR_SLP_MASK |
		XCAN_IXR_ERROR_MASK | XCAN_IXR_RXOFLW_MASK |
		XCAN_IXR_ARBLST_MASK | xcan_rx_int_mask(priv);

	if (priv->devtype.flags & XCAN_FLAG_RXMNF)
		ier |= XCAN_IXR_RXMNF_MASK;

	priv->write_reg(priv, XCAN_IER_OFFSET, ier);

	 
	if (priv->can.ctrlmode & CAN_CTRLMODE_LOOPBACK)
		reg_msr = XCAN_MSR_LBACK_MASK;
	else
		reg_msr = 0x0;

	 
	if (priv->devtype.flags & XCAN_FLAG_EXT_FILTERS)
		priv->write_reg(priv, XCAN_AFR_EXT_OFFSET, 0x00000001);

	priv->write_reg(priv, XCAN_MSR_OFFSET, reg_msr);
	priv->write_reg(priv, XCAN_SRR_OFFSET, XCAN_SRR_CEN_MASK);

	netdev_dbg(ndev, "status:#x%08x\n",
		   priv->read_reg(priv, XCAN_SR_OFFSET));

	priv->can.state = CAN_STATE_ERROR_ACTIVE;
	return 0;
}

 
static int xcan_do_set_mode(struct net_device *ndev, enum can_mode mode)
{
	int ret;

	switch (mode) {
	case CAN_MODE_START:
		ret = xcan_chip_start(ndev);
		if (ret < 0) {
			netdev_err(ndev, "xcan_chip_start failed!\n");
			return ret;
		}
		netif_wake_queue(ndev);
		break;
	default:
		ret = -EOPNOTSUPP;
		break;
	}

	return ret;
}

 
static void xcan_write_frame(struct net_device *ndev, struct sk_buff *skb,
			     int frame_offset)
{
	u32 id, dlc, data[2] = {0, 0};
	struct canfd_frame *cf = (struct canfd_frame *)skb->data;
	u32 ramoff, dwindex = 0, i;
	struct xcan_priv *priv = netdev_priv(ndev);

	 
	if (cf->can_id & CAN_EFF_FLAG) {
		 
		id = ((cf->can_id & CAN_EFF_MASK) << XCAN_IDR_ID2_SHIFT) &
			XCAN_IDR_ID2_MASK;
		id |= (((cf->can_id & CAN_EFF_MASK) >>
			(CAN_EFF_ID_BITS - CAN_SFF_ID_BITS)) <<
			XCAN_IDR_ID1_SHIFT) & XCAN_IDR_ID1_MASK;

		 
		id |= XCAN_IDR_IDE_MASK | XCAN_IDR_SRR_MASK;

		if (cf->can_id & CAN_RTR_FLAG)
			 
			id |= XCAN_IDR_RTR_MASK;
	} else {
		 
		id = ((cf->can_id & CAN_SFF_MASK) << XCAN_IDR_ID1_SHIFT) &
			XCAN_IDR_ID1_MASK;

		if (cf->can_id & CAN_RTR_FLAG)
			 
			id |= XCAN_IDR_SRR_MASK;
	}

	dlc = can_fd_len2dlc(cf->len) << XCAN_DLCR_DLC_SHIFT;
	if (can_is_canfd_skb(skb)) {
		if (cf->flags & CANFD_BRS)
			dlc |= XCAN_DLCR_BRS_MASK;
		dlc |= XCAN_DLCR_EDL_MASK;
	}

	if (!(priv->devtype.flags & XCAN_FLAG_TX_MAILBOXES) &&
	    (priv->devtype.flags & XCAN_FLAG_TXFEMP))
		can_put_echo_skb(skb, ndev, priv->tx_head % priv->tx_max, 0);
	else
		can_put_echo_skb(skb, ndev, 0, 0);

	priv->tx_head++;

	priv->write_reg(priv, XCAN_FRAME_ID_OFFSET(frame_offset), id);
	 
	priv->write_reg(priv, XCAN_FRAME_DLC_OFFSET(frame_offset), dlc);
	if (priv->devtype.cantype == XAXI_CANFD ||
	    priv->devtype.cantype == XAXI_CANFD_2_0) {
		for (i = 0; i < cf->len; i += 4) {
			ramoff = XCANFD_FRAME_DW_OFFSET(frame_offset) +
					(dwindex * XCANFD_DW_BYTES);
			priv->write_reg(priv, ramoff,
					be32_to_cpup((__be32 *)(cf->data + i)));
			dwindex++;
		}
	} else {
		if (cf->len > 0)
			data[0] = be32_to_cpup((__be32 *)(cf->data + 0));
		if (cf->len > 4)
			data[1] = be32_to_cpup((__be32 *)(cf->data + 4));

		if (!(cf->can_id & CAN_RTR_FLAG)) {
			priv->write_reg(priv,
					XCAN_FRAME_DW1_OFFSET(frame_offset),
					data[0]);
			 
			priv->write_reg(priv,
					XCAN_FRAME_DW2_OFFSET(frame_offset),
					data[1]);
		}
	}
}

 
static int xcan_start_xmit_fifo(struct sk_buff *skb, struct net_device *ndev)
{
	struct xcan_priv *priv = netdev_priv(ndev);
	unsigned long flags;

	 
	if (unlikely(priv->read_reg(priv, XCAN_SR_OFFSET) &
			XCAN_SR_TXFLL_MASK))
		return -ENOSPC;

	spin_lock_irqsave(&priv->tx_lock, flags);

	xcan_write_frame(ndev, skb, XCAN_TXFIFO_OFFSET);

	 
	if (priv->tx_max > 1)
		priv->write_reg(priv, XCAN_ICR_OFFSET, XCAN_IXR_TXFEMP_MASK);

	 
	if ((priv->tx_head - priv->tx_tail) == priv->tx_max)
		netif_stop_queue(ndev);

	spin_unlock_irqrestore(&priv->tx_lock, flags);

	return 0;
}

 
static int xcan_start_xmit_mailbox(struct sk_buff *skb, struct net_device *ndev)
{
	struct xcan_priv *priv = netdev_priv(ndev);
	unsigned long flags;

	if (unlikely(priv->read_reg(priv, XCAN_TRR_OFFSET) &
		     BIT(XCAN_TX_MAILBOX_IDX)))
		return -ENOSPC;

	spin_lock_irqsave(&priv->tx_lock, flags);

	xcan_write_frame(ndev, skb,
			 XCAN_TXMSG_FRAME_OFFSET(XCAN_TX_MAILBOX_IDX));

	 
	priv->write_reg(priv, XCAN_TRR_OFFSET, BIT(XCAN_TX_MAILBOX_IDX));

	netif_stop_queue(ndev);

	spin_unlock_irqrestore(&priv->tx_lock, flags);

	return 0;
}

 
static netdev_tx_t xcan_start_xmit(struct sk_buff *skb, struct net_device *ndev)
{
	struct xcan_priv *priv = netdev_priv(ndev);
	int ret;

	if (can_dev_dropped_skb(ndev, skb))
		return NETDEV_TX_OK;

	if (priv->devtype.flags & XCAN_FLAG_TX_MAILBOXES)
		ret = xcan_start_xmit_mailbox(skb, ndev);
	else
		ret = xcan_start_xmit_fifo(skb, ndev);

	if (ret < 0) {
		netdev_err(ndev, "BUG!, TX full when queue awake!\n");
		netif_stop_queue(ndev);
		return NETDEV_TX_BUSY;
	}

	return NETDEV_TX_OK;
}

 
static int xcan_rx(struct net_device *ndev, int frame_base)
{
	struct xcan_priv *priv = netdev_priv(ndev);
	struct net_device_stats *stats = &ndev->stats;
	struct can_frame *cf;
	struct sk_buff *skb;
	u32 id_xcan, dlc, data[2] = {0, 0};

	skb = alloc_can_skb(ndev, &cf);
	if (unlikely(!skb)) {
		stats->rx_dropped++;
		return 0;
	}

	 
	id_xcan = priv->read_reg(priv, XCAN_FRAME_ID_OFFSET(frame_base));
	dlc = priv->read_reg(priv, XCAN_FRAME_DLC_OFFSET(frame_base)) >>
				   XCAN_DLCR_DLC_SHIFT;

	 
	cf->len = can_cc_dlc2len(dlc);

	 
	if (id_xcan & XCAN_IDR_IDE_MASK) {
		 
		cf->can_id = (id_xcan & XCAN_IDR_ID1_MASK) >> 3;
		cf->can_id |= (id_xcan & XCAN_IDR_ID2_MASK) >>
				XCAN_IDR_ID2_SHIFT;
		cf->can_id |= CAN_EFF_FLAG;
		if (id_xcan & XCAN_IDR_RTR_MASK)
			cf->can_id |= CAN_RTR_FLAG;
	} else {
		 
		cf->can_id = (id_xcan & XCAN_IDR_ID1_MASK) >>
				XCAN_IDR_ID1_SHIFT;
		if (id_xcan & XCAN_IDR_SRR_MASK)
			cf->can_id |= CAN_RTR_FLAG;
	}

	 
	data[0] = priv->read_reg(priv, XCAN_FRAME_DW1_OFFSET(frame_base));
	data[1] = priv->read_reg(priv, XCAN_FRAME_DW2_OFFSET(frame_base));

	if (!(cf->can_id & CAN_RTR_FLAG)) {
		 
		if (cf->len > 0)
			*(__be32 *)(cf->data) = cpu_to_be32(data[0]);
		if (cf->len > 4)
			*(__be32 *)(cf->data + 4) = cpu_to_be32(data[1]);

		stats->rx_bytes += cf->len;
	}
	stats->rx_packets++;

	netif_receive_skb(skb);

	return 1;
}

 
static int xcanfd_rx(struct net_device *ndev, int frame_base)
{
	struct xcan_priv *priv = netdev_priv(ndev);
	struct net_device_stats *stats = &ndev->stats;
	struct canfd_frame *cf;
	struct sk_buff *skb;
	u32 id_xcan, dlc, data[2] = {0, 0}, dwindex = 0, i, dw_offset;

	id_xcan = priv->read_reg(priv, XCAN_FRAME_ID_OFFSET(frame_base));
	dlc = priv->read_reg(priv, XCAN_FRAME_DLC_OFFSET(frame_base));
	if (dlc & XCAN_DLCR_EDL_MASK)
		skb = alloc_canfd_skb(ndev, &cf);
	else
		skb = alloc_can_skb(ndev, (struct can_frame **)&cf);

	if (unlikely(!skb)) {
		stats->rx_dropped++;
		return 0;
	}

	 
	if (dlc & XCAN_DLCR_EDL_MASK)
		cf->len = can_fd_dlc2len((dlc & XCAN_DLCR_DLC_MASK) >>
				  XCAN_DLCR_DLC_SHIFT);
	else
		cf->len = can_cc_dlc2len((dlc & XCAN_DLCR_DLC_MASK) >>
					  XCAN_DLCR_DLC_SHIFT);

	 
	if (id_xcan & XCAN_IDR_IDE_MASK) {
		 
		cf->can_id = (id_xcan & XCAN_IDR_ID1_MASK) >> 3;
		cf->can_id |= (id_xcan & XCAN_IDR_ID2_MASK) >>
				XCAN_IDR_ID2_SHIFT;
		cf->can_id |= CAN_EFF_FLAG;
		if (id_xcan & XCAN_IDR_RTR_MASK)
			cf->can_id |= CAN_RTR_FLAG;
	} else {
		 
		cf->can_id = (id_xcan & XCAN_IDR_ID1_MASK) >>
				XCAN_IDR_ID1_SHIFT;
		if (!(dlc & XCAN_DLCR_EDL_MASK) && (id_xcan &
					XCAN_IDR_SRR_MASK))
			cf->can_id |= CAN_RTR_FLAG;
	}

	 
	if (dlc & XCAN_DLCR_EDL_MASK) {
		for (i = 0; i < cf->len; i += 4) {
			dw_offset = XCANFD_FRAME_DW_OFFSET(frame_base) +
					(dwindex * XCANFD_DW_BYTES);
			data[0] = priv->read_reg(priv, dw_offset);
			*(__be32 *)(cf->data + i) = cpu_to_be32(data[0]);
			dwindex++;
		}
	} else {
		for (i = 0; i < cf->len; i += 4) {
			dw_offset = XCANFD_FRAME_DW_OFFSET(frame_base);
			data[0] = priv->read_reg(priv, dw_offset + i);
			*(__be32 *)(cf->data + i) = cpu_to_be32(data[0]);
		}
	}

	if (!(cf->can_id & CAN_RTR_FLAG))
		stats->rx_bytes += cf->len;
	stats->rx_packets++;

	netif_receive_skb(skb);

	return 1;
}

 
static enum can_state xcan_current_error_state(struct net_device *ndev)
{
	struct xcan_priv *priv = netdev_priv(ndev);
	u32 status = priv->read_reg(priv, XCAN_SR_OFFSET);

	if ((status & XCAN_SR_ESTAT_MASK) == XCAN_SR_ESTAT_MASK)
		return CAN_STATE_ERROR_PASSIVE;
	else if (status & XCAN_SR_ERRWRN_MASK)
		return CAN_STATE_ERROR_WARNING;
	else
		return CAN_STATE_ERROR_ACTIVE;
}

 
static void xcan_set_error_state(struct net_device *ndev,
				 enum can_state new_state,
				 struct can_frame *cf)
{
	struct xcan_priv *priv = netdev_priv(ndev);
	u32 ecr = priv->read_reg(priv, XCAN_ECR_OFFSET);
	u32 txerr = ecr & XCAN_ECR_TEC_MASK;
	u32 rxerr = (ecr & XCAN_ECR_REC_MASK) >> XCAN_ESR_REC_SHIFT;
	enum can_state tx_state = txerr >= rxerr ? new_state : 0;
	enum can_state rx_state = txerr <= rxerr ? new_state : 0;

	 
	if (WARN_ON(new_state > CAN_STATE_ERROR_PASSIVE))
		return;

	can_change_state(ndev, cf, tx_state, rx_state);

	if (cf) {
		cf->can_id |= CAN_ERR_CNT;
		cf->data[6] = txerr;
		cf->data[7] = rxerr;
	}
}

 
static void xcan_update_error_state_after_rxtx(struct net_device *ndev)
{
	struct xcan_priv *priv = netdev_priv(ndev);
	enum can_state old_state = priv->can.state;
	enum can_state new_state;

	 
	if (old_state != CAN_STATE_ERROR_WARNING &&
	    old_state != CAN_STATE_ERROR_PASSIVE)
		return;

	new_state = xcan_current_error_state(ndev);

	if (new_state != old_state) {
		struct sk_buff *skb;
		struct can_frame *cf;

		skb = alloc_can_err_skb(ndev, &cf);

		xcan_set_error_state(ndev, new_state, skb ? cf : NULL);

		if (skb)
			netif_rx(skb);
	}
}

 
static void xcan_err_interrupt(struct net_device *ndev, u32 isr)
{
	struct xcan_priv *priv = netdev_priv(ndev);
	struct net_device_stats *stats = &ndev->stats;
	struct can_frame cf = { };
	u32 err_status;

	err_status = priv->read_reg(priv, XCAN_ESR_OFFSET);
	priv->write_reg(priv, XCAN_ESR_OFFSET, err_status);

	if (isr & XCAN_IXR_BSOFF_MASK) {
		priv->can.state = CAN_STATE_BUS_OFF;
		priv->can.can_stats.bus_off++;
		 
		priv->write_reg(priv, XCAN_SRR_OFFSET, XCAN_SRR_RESET_MASK);
		can_bus_off(ndev);
		cf.can_id |= CAN_ERR_BUSOFF;
	} else {
		enum can_state new_state = xcan_current_error_state(ndev);

		if (new_state != priv->can.state)
			xcan_set_error_state(ndev, new_state, &cf);
	}

	 
	if (isr & XCAN_IXR_ARBLST_MASK) {
		priv->can.can_stats.arbitration_lost++;
		cf.can_id |= CAN_ERR_LOSTARB;
		cf.data[0] = CAN_ERR_LOSTARB_UNSPEC;
	}

	 
	if (isr & XCAN_IXR_RXOFLW_MASK) {
		stats->rx_over_errors++;
		stats->rx_errors++;
		cf.can_id |= CAN_ERR_CRTL;
		cf.data[1] |= CAN_ERR_CRTL_RX_OVERFLOW;
	}

	 
	if (isr & XCAN_IXR_RXMNF_MASK) {
		stats->rx_dropped++;
		stats->rx_errors++;
		netdev_err(ndev, "RX match not finished, frame discarded\n");
		cf.can_id |= CAN_ERR_CRTL;
		cf.data[1] |= CAN_ERR_CRTL_UNSPEC;
	}

	 
	if (isr & XCAN_IXR_ERROR_MASK) {
		bool berr_reporting = false;

		if (priv->can.ctrlmode & CAN_CTRLMODE_BERR_REPORTING) {
			berr_reporting = true;
			cf.can_id |= CAN_ERR_PROT | CAN_ERR_BUSERROR;
		}

		 
		if (err_status & XCAN_ESR_ACKER_MASK) {
			stats->tx_errors++;
			if (berr_reporting) {
				cf.can_id |= CAN_ERR_ACK;
				cf.data[3] = CAN_ERR_PROT_LOC_ACK;
			}
		}

		 
		if (err_status & XCAN_ESR_BERR_MASK) {
			stats->tx_errors++;
			if (berr_reporting) {
				cf.can_id |= CAN_ERR_PROT;
				cf.data[2] = CAN_ERR_PROT_BIT;
			}
		}

		 
		if (err_status & XCAN_ESR_STER_MASK) {
			stats->rx_errors++;
			if (berr_reporting) {
				cf.can_id |= CAN_ERR_PROT;
				cf.data[2] = CAN_ERR_PROT_STUFF;
			}
		}

		 
		if (err_status & XCAN_ESR_FMER_MASK) {
			stats->rx_errors++;
			if (berr_reporting) {
				cf.can_id |= CAN_ERR_PROT;
				cf.data[2] = CAN_ERR_PROT_FORM;
			}
		}

		 
		if (err_status & XCAN_ESR_CRCER_MASK) {
			stats->rx_errors++;
			if (berr_reporting) {
				cf.can_id |= CAN_ERR_PROT;
				cf.data[3] = CAN_ERR_PROT_LOC_CRC_SEQ;
			}
		}
		priv->can.can_stats.bus_error++;
	}

	if (cf.can_id) {
		struct can_frame *skb_cf;
		struct sk_buff *skb = alloc_can_err_skb(ndev, &skb_cf);

		if (skb) {
			skb_cf->can_id |= cf.can_id;
			memcpy(skb_cf->data, cf.data, CAN_ERR_DLC);
			netif_rx(skb);
		}
	}

	netdev_dbg(ndev, "%s: error status register:0x%x\n",
		   __func__, priv->read_reg(priv, XCAN_ESR_OFFSET));
}

 
static void xcan_state_interrupt(struct net_device *ndev, u32 isr)
{
	struct xcan_priv *priv = netdev_priv(ndev);

	 
	if (isr & XCAN_IXR_SLP_MASK)
		priv->can.state = CAN_STATE_SLEEPING;

	 
	if (isr & XCAN_IXR_WKUP_MASK)
		priv->can.state = CAN_STATE_ERROR_ACTIVE;
}

 
static int xcan_rx_fifo_get_next_frame(struct xcan_priv *priv)
{
	int offset;

	if (priv->devtype.flags & XCAN_FLAG_RX_FIFO_MULTI) {
		u32 fsr, mask;

		 
		priv->write_reg(priv, XCAN_ICR_OFFSET, XCAN_IXR_RXOK_MASK);

		fsr = priv->read_reg(priv, XCAN_FSR_OFFSET);

		 
		if (priv->devtype.flags & XCAN_FLAG_CANFD_2)
			mask = XCAN_2_FSR_FL_MASK;
		else
			mask = XCAN_FSR_FL_MASK;

		if (!(fsr & mask))
			return -ENOENT;

		if (priv->devtype.flags & XCAN_FLAG_CANFD_2)
			offset =
			  XCAN_RXMSG_2_FRAME_OFFSET(fsr & XCAN_2_FSR_RI_MASK);
		else
			offset =
			  XCAN_RXMSG_FRAME_OFFSET(fsr & XCAN_FSR_RI_MASK);

	} else {
		 
		if (!(priv->read_reg(priv, XCAN_ISR_OFFSET) &
		      XCAN_IXR_RXNEMP_MASK))
			return -ENOENT;

		 
		offset = XCAN_RXFIFO_OFFSET;
	}

	return offset;
}

 
static int xcan_rx_poll(struct napi_struct *napi, int quota)
{
	struct net_device *ndev = napi->dev;
	struct xcan_priv *priv = netdev_priv(ndev);
	u32 ier;
	int work_done = 0;
	int frame_offset;

	while ((frame_offset = xcan_rx_fifo_get_next_frame(priv)) >= 0 &&
	       (work_done < quota)) {
		if (xcan_rx_int_mask(priv) & XCAN_IXR_RXOK_MASK)
			work_done += xcanfd_rx(ndev, frame_offset);
		else
			work_done += xcan_rx(ndev, frame_offset);

		if (priv->devtype.flags & XCAN_FLAG_RX_FIFO_MULTI)
			 
			priv->write_reg(priv, XCAN_FSR_OFFSET,
					XCAN_FSR_IRI_MASK);
		else
			 
			priv->write_reg(priv, XCAN_ICR_OFFSET,
					XCAN_IXR_RXNEMP_MASK);
	}

	if (work_done)
		xcan_update_error_state_after_rxtx(ndev);

	if (work_done < quota) {
		if (napi_complete_done(napi, work_done)) {
			ier = priv->read_reg(priv, XCAN_IER_OFFSET);
			ier |= xcan_rx_int_mask(priv);
			priv->write_reg(priv, XCAN_IER_OFFSET, ier);
		}
	}
	return work_done;
}

 
static void xcan_tx_interrupt(struct net_device *ndev, u32 isr)
{
	struct xcan_priv *priv = netdev_priv(ndev);
	struct net_device_stats *stats = &ndev->stats;
	unsigned int frames_in_fifo;
	int frames_sent = 1;  
	unsigned long flags;
	int retries = 0;

	 
	spin_lock_irqsave(&priv->tx_lock, flags);

	frames_in_fifo = priv->tx_head - priv->tx_tail;

	if (WARN_ON_ONCE(frames_in_fifo == 0)) {
		 
		priv->write_reg(priv, XCAN_ICR_OFFSET, XCAN_IXR_TXOK_MASK);
		spin_unlock_irqrestore(&priv->tx_lock, flags);
		return;
	}

	 
	if (frames_in_fifo > 1) {
		WARN_ON(frames_in_fifo > priv->tx_max);

		 
		while ((isr & XCAN_IXR_TXOK_MASK) &&
		       !WARN_ON(++retries == 100)) {
			priv->write_reg(priv, XCAN_ICR_OFFSET,
					XCAN_IXR_TXOK_MASK);
			isr = priv->read_reg(priv, XCAN_ISR_OFFSET);
		}

		if (isr & XCAN_IXR_TXFEMP_MASK) {
			 
			frames_sent = frames_in_fifo;
		}
	} else {
		 
		priv->write_reg(priv, XCAN_ICR_OFFSET, XCAN_IXR_TXOK_MASK);
	}

	while (frames_sent--) {
		stats->tx_bytes += can_get_echo_skb(ndev, priv->tx_tail %
						    priv->tx_max, NULL);
		priv->tx_tail++;
		stats->tx_packets++;
	}

	netif_wake_queue(ndev);

	spin_unlock_irqrestore(&priv->tx_lock, flags);

	xcan_update_error_state_after_rxtx(ndev);
}

 
static irqreturn_t xcan_interrupt(int irq, void *dev_id)
{
	struct net_device *ndev = (struct net_device *)dev_id;
	struct xcan_priv *priv = netdev_priv(ndev);
	u32 isr, ier;
	u32 isr_errors;
	u32 rx_int_mask = xcan_rx_int_mask(priv);

	 
	isr = priv->read_reg(priv, XCAN_ISR_OFFSET);
	if (!isr)
		return IRQ_NONE;

	 
	if (isr & (XCAN_IXR_SLP_MASK | XCAN_IXR_WKUP_MASK)) {
		priv->write_reg(priv, XCAN_ICR_OFFSET, (XCAN_IXR_SLP_MASK |
				XCAN_IXR_WKUP_MASK));
		xcan_state_interrupt(ndev, isr);
	}

	 
	if (isr & XCAN_IXR_TXOK_MASK)
		xcan_tx_interrupt(ndev, isr);

	 
	isr_errors = isr & (XCAN_IXR_ERROR_MASK | XCAN_IXR_RXOFLW_MASK |
			    XCAN_IXR_BSOFF_MASK | XCAN_IXR_ARBLST_MASK |
			    XCAN_IXR_RXMNF_MASK);
	if (isr_errors) {
		priv->write_reg(priv, XCAN_ICR_OFFSET, isr_errors);
		xcan_err_interrupt(ndev, isr);
	}

	 
	if (isr & rx_int_mask) {
		ier = priv->read_reg(priv, XCAN_IER_OFFSET);
		ier &= ~rx_int_mask;
		priv->write_reg(priv, XCAN_IER_OFFSET, ier);
		napi_schedule(&priv->napi);
	}
	return IRQ_HANDLED;
}

 
static void xcan_chip_stop(struct net_device *ndev)
{
	struct xcan_priv *priv = netdev_priv(ndev);
	int ret;

	 
	ret = set_reset_mode(ndev);
	if (ret < 0)
		netdev_dbg(ndev, "set_reset_mode() Failed\n");

	priv->can.state = CAN_STATE_STOPPED;
}

 
static int xcan_open(struct net_device *ndev)
{
	struct xcan_priv *priv = netdev_priv(ndev);
	int ret;

	ret = phy_power_on(priv->transceiver);
	if (ret)
		return ret;

	ret = pm_runtime_get_sync(priv->dev);
	if (ret < 0) {
		netdev_err(ndev, "%s: pm_runtime_get failed(%d)\n",
			   __func__, ret);
		goto err;
	}

	ret = request_irq(ndev->irq, xcan_interrupt, priv->irq_flags,
			  ndev->name, ndev);
	if (ret < 0) {
		netdev_err(ndev, "irq allocation for CAN failed\n");
		goto err;
	}

	 
	ret = set_reset_mode(ndev);
	if (ret < 0) {
		netdev_err(ndev, "mode resetting failed!\n");
		goto err_irq;
	}

	 
	ret = open_candev(ndev);
	if (ret)
		goto err_irq;

	ret = xcan_chip_start(ndev);
	if (ret < 0) {
		netdev_err(ndev, "xcan_chip_start failed!\n");
		goto err_candev;
	}

	napi_enable(&priv->napi);
	netif_start_queue(ndev);

	return 0;

err_candev:
	close_candev(ndev);
err_irq:
	free_irq(ndev->irq, ndev);
err:
	pm_runtime_put(priv->dev);
	phy_power_off(priv->transceiver);

	return ret;
}

 
static int xcan_close(struct net_device *ndev)
{
	struct xcan_priv *priv = netdev_priv(ndev);

	netif_stop_queue(ndev);
	napi_disable(&priv->napi);
	xcan_chip_stop(ndev);
	free_irq(ndev->irq, ndev);
	close_candev(ndev);

	pm_runtime_put(priv->dev);
	phy_power_off(priv->transceiver);

	return 0;
}

 
static int xcan_get_berr_counter(const struct net_device *ndev,
				 struct can_berr_counter *bec)
{
	struct xcan_priv *priv = netdev_priv(ndev);
	int ret;

	ret = pm_runtime_get_sync(priv->dev);
	if (ret < 0) {
		netdev_err(ndev, "%s: pm_runtime_get failed(%d)\n",
			   __func__, ret);
		pm_runtime_put(priv->dev);
		return ret;
	}

	bec->txerr = priv->read_reg(priv, XCAN_ECR_OFFSET) & XCAN_ECR_TEC_MASK;
	bec->rxerr = ((priv->read_reg(priv, XCAN_ECR_OFFSET) &
			XCAN_ECR_REC_MASK) >> XCAN_ESR_REC_SHIFT);

	pm_runtime_put(priv->dev);

	return 0;
}

 
static int xcan_get_auto_tdcv(const struct net_device *ndev, u32 *tdcv)
{
	struct xcan_priv *priv = netdev_priv(ndev);

	*tdcv = FIELD_GET(XCAN_SR_TDCV_MASK, priv->read_reg(priv, XCAN_SR_OFFSET));

	return 0;
}

static const struct net_device_ops xcan_netdev_ops = {
	.ndo_open	= xcan_open,
	.ndo_stop	= xcan_close,
	.ndo_start_xmit	= xcan_start_xmit,
	.ndo_change_mtu	= can_change_mtu,
};

static const struct ethtool_ops xcan_ethtool_ops = {
	.get_ts_info = ethtool_op_get_ts_info,
};

 
static int __maybe_unused xcan_suspend(struct device *dev)
{
	struct net_device *ndev = dev_get_drvdata(dev);

	if (netif_running(ndev)) {
		netif_stop_queue(ndev);
		netif_device_detach(ndev);
		xcan_chip_stop(ndev);
	}

	return pm_runtime_force_suspend(dev);
}

 
static int __maybe_unused xcan_resume(struct device *dev)
{
	struct net_device *ndev = dev_get_drvdata(dev);
	int ret;

	ret = pm_runtime_force_resume(dev);
	if (ret) {
		dev_err(dev, "pm_runtime_force_resume failed on resume\n");
		return ret;
	}

	if (netif_running(ndev)) {
		ret = xcan_chip_start(ndev);
		if (ret) {
			dev_err(dev, "xcan_chip_start failed on resume\n");
			return ret;
		}

		netif_device_attach(ndev);
		netif_start_queue(ndev);
	}

	return 0;
}

 
static int __maybe_unused xcan_runtime_suspend(struct device *dev)
{
	struct net_device *ndev = dev_get_drvdata(dev);
	struct xcan_priv *priv = netdev_priv(ndev);

	clk_disable_unprepare(priv->bus_clk);
	clk_disable_unprepare(priv->can_clk);

	return 0;
}

 
static int __maybe_unused xcan_runtime_resume(struct device *dev)
{
	struct net_device *ndev = dev_get_drvdata(dev);
	struct xcan_priv *priv = netdev_priv(ndev);
	int ret;

	ret = clk_prepare_enable(priv->bus_clk);
	if (ret) {
		dev_err(dev, "Cannot enable clock.\n");
		return ret;
	}
	ret = clk_prepare_enable(priv->can_clk);
	if (ret) {
		dev_err(dev, "Cannot enable clock.\n");
		clk_disable_unprepare(priv->bus_clk);
		return ret;
	}

	return 0;
}

static const struct dev_pm_ops xcan_dev_pm_ops = {
	SET_SYSTEM_SLEEP_PM_OPS(xcan_suspend, xcan_resume)
	SET_RUNTIME_PM_OPS(xcan_runtime_suspend, xcan_runtime_resume, NULL)
};

static const struct xcan_devtype_data xcan_zynq_data = {
	.cantype = XZYNQ_CANPS,
	.flags = XCAN_FLAG_TXFEMP,
	.bittiming_const = &xcan_bittiming_const,
	.btr_ts2_shift = XCAN_BTR_TS2_SHIFT,
	.btr_sjw_shift = XCAN_BTR_SJW_SHIFT,
	.bus_clk_name = "pclk",
};

static const struct xcan_devtype_data xcan_axi_data = {
	.cantype = XAXI_CAN,
	.bittiming_const = &xcan_bittiming_const,
	.btr_ts2_shift = XCAN_BTR_TS2_SHIFT,
	.btr_sjw_shift = XCAN_BTR_SJW_SHIFT,
	.bus_clk_name = "s_axi_aclk",
};

static const struct xcan_devtype_data xcan_canfd_data = {
	.cantype = XAXI_CANFD,
	.flags = XCAN_FLAG_EXT_FILTERS |
		 XCAN_FLAG_RXMNF |
		 XCAN_FLAG_TX_MAILBOXES |
		 XCAN_FLAG_RX_FIFO_MULTI,
	.bittiming_const = &xcan_bittiming_const_canfd,
	.btr_ts2_shift = XCAN_BTR_TS2_SHIFT_CANFD,
	.btr_sjw_shift = XCAN_BTR_SJW_SHIFT_CANFD,
	.bus_clk_name = "s_axi_aclk",
};

static const struct xcan_devtype_data xcan_canfd2_data = {
	.cantype = XAXI_CANFD_2_0,
	.flags = XCAN_FLAG_EXT_FILTERS |
		 XCAN_FLAG_RXMNF |
		 XCAN_FLAG_TX_MAILBOXES |
		 XCAN_FLAG_CANFD_2 |
		 XCAN_FLAG_RX_FIFO_MULTI,
	.bittiming_const = &xcan_bittiming_const_canfd2,
	.btr_ts2_shift = XCAN_BTR_TS2_SHIFT_CANFD,
	.btr_sjw_shift = XCAN_BTR_SJW_SHIFT_CANFD,
	.bus_clk_name = "s_axi_aclk",
};

 
static const struct of_device_id xcan_of_match[] = {
	{ .compatible = "xlnx,zynq-can-1.0", .data = &xcan_zynq_data },
	{ .compatible = "xlnx,axi-can-1.00.a", .data = &xcan_axi_data },
	{ .compatible = "xlnx,canfd-1.0", .data = &xcan_canfd_data },
	{ .compatible = "xlnx,canfd-2.0", .data = &xcan_canfd2_data },
	{   },
};
MODULE_DEVICE_TABLE(of, xcan_of_match);

 
static int xcan_probe(struct platform_device *pdev)
{
	struct net_device *ndev;
	struct xcan_priv *priv;
	struct phy *transceiver;
	const struct of_device_id *of_id;
	const struct xcan_devtype_data *devtype = &xcan_axi_data;
	void __iomem *addr;
	int ret;
	int rx_max, tx_max;
	u32 hw_tx_max = 0, hw_rx_max = 0;
	const char *hw_tx_max_property;

	 
	addr = devm_platform_ioremap_resource(pdev, 0);
	if (IS_ERR(addr)) {
		ret = PTR_ERR(addr);
		goto err;
	}

	of_id = of_match_device(xcan_of_match, &pdev->dev);
	if (of_id && of_id->data)
		devtype = of_id->data;

	hw_tx_max_property = devtype->flags & XCAN_FLAG_TX_MAILBOXES ?
			     "tx-mailbox-count" : "tx-fifo-depth";

	ret = of_property_read_u32(pdev->dev.of_node, hw_tx_max_property,
				   &hw_tx_max);
	if (ret < 0) {
		dev_err(&pdev->dev, "missing %s property\n",
			hw_tx_max_property);
		goto err;
	}

	ret = of_property_read_u32(pdev->dev.of_node, "rx-fifo-depth",
				   &hw_rx_max);
	if (ret < 0) {
		dev_err(&pdev->dev,
			"missing rx-fifo-depth property (mailbox mode is not supported)\n");
		goto err;
	}

	 
	if (!(devtype->flags & XCAN_FLAG_TX_MAILBOXES) &&
	    (devtype->flags & XCAN_FLAG_TXFEMP))
		tx_max = min(hw_tx_max, 2U);
	else
		tx_max = 1;

	rx_max = hw_rx_max;

	 
	ndev = alloc_candev(sizeof(struct xcan_priv), tx_max);
	if (!ndev)
		return -ENOMEM;

	priv = netdev_priv(ndev);
	priv->dev = &pdev->dev;
	priv->can.bittiming_const = devtype->bittiming_const;
	priv->can.do_set_mode = xcan_do_set_mode;
	priv->can.do_get_berr_counter = xcan_get_berr_counter;
	priv->can.ctrlmode_supported = CAN_CTRLMODE_LOOPBACK |
					CAN_CTRLMODE_BERR_REPORTING;
	priv->rstc = devm_reset_control_get_optional_exclusive(&pdev->dev, NULL);
	if (IS_ERR(priv->rstc)) {
		dev_err(&pdev->dev, "Cannot get CAN reset.\n");
		ret = PTR_ERR(priv->rstc);
		goto err_free;
	}

	ret = reset_control_reset(priv->rstc);
	if (ret)
		goto err_free;

	if (devtype->cantype == XAXI_CANFD) {
		priv->can.data_bittiming_const =
			&xcan_data_bittiming_const_canfd;
		priv->can.tdc_const = &xcan_tdc_const_canfd;
	}

	if (devtype->cantype == XAXI_CANFD_2_0) {
		priv->can.data_bittiming_const =
			&xcan_data_bittiming_const_canfd2;
		priv->can.tdc_const = &xcan_tdc_const_canfd2;
	}

	if (devtype->cantype == XAXI_CANFD ||
	    devtype->cantype == XAXI_CANFD_2_0) {
		priv->can.ctrlmode_supported |= CAN_CTRLMODE_FD |
						CAN_CTRLMODE_TDC_AUTO;
		priv->can.do_get_auto_tdcv = xcan_get_auto_tdcv;
	}

	priv->reg_base = addr;
	priv->tx_max = tx_max;
	priv->devtype = *devtype;
	spin_lock_init(&priv->tx_lock);

	 
	ret = platform_get_irq(pdev, 0);
	if (ret < 0)
		goto err_reset;

	ndev->irq = ret;

	ndev->flags |= IFF_ECHO;	 

	platform_set_drvdata(pdev, ndev);
	SET_NETDEV_DEV(ndev, &pdev->dev);
	ndev->netdev_ops = &xcan_netdev_ops;
	ndev->ethtool_ops = &xcan_ethtool_ops;

	 
	priv->can_clk = devm_clk_get(&pdev->dev, "can_clk");
	if (IS_ERR(priv->can_clk)) {
		ret = dev_err_probe(&pdev->dev, PTR_ERR(priv->can_clk),
				    "device clock not found\n");
		goto err_reset;
	}

	priv->bus_clk = devm_clk_get(&pdev->dev, devtype->bus_clk_name);
	if (IS_ERR(priv->bus_clk)) {
		ret = dev_err_probe(&pdev->dev, PTR_ERR(priv->bus_clk),
				    "bus clock not found\n");
		goto err_reset;
	}

	transceiver = devm_phy_optional_get(&pdev->dev, NULL);
	if (IS_ERR(transceiver)) {
		ret = PTR_ERR(transceiver);
		dev_err_probe(&pdev->dev, ret, "failed to get phy\n");
		goto err_reset;
	}
	priv->transceiver = transceiver;

	priv->write_reg = xcan_write_reg_le;
	priv->read_reg = xcan_read_reg_le;

	pm_runtime_enable(&pdev->dev);
	ret = pm_runtime_get_sync(&pdev->dev);
	if (ret < 0) {
		netdev_err(ndev, "%s: pm_runtime_get failed(%d)\n",
			   __func__, ret);
		goto err_disableclks;
	}

	if (priv->read_reg(priv, XCAN_SR_OFFSET) != XCAN_SR_CONFIG_MASK) {
		priv->write_reg = xcan_write_reg_be;
		priv->read_reg = xcan_read_reg_be;
	}

	priv->can.clock.freq = clk_get_rate(priv->can_clk);

	netif_napi_add_weight(ndev, &priv->napi, xcan_rx_poll, rx_max);

	ret = register_candev(ndev);
	if (ret) {
		dev_err(&pdev->dev, "fail to register failed (err=%d)\n", ret);
		goto err_disableclks;
	}

	of_can_transceiver(ndev);
	pm_runtime_put(&pdev->dev);

	if (priv->devtype.flags & XCAN_FLAG_CANFD_2) {
		priv->write_reg(priv, XCAN_AFR_2_ID_OFFSET, 0x00000000);
		priv->write_reg(priv, XCAN_AFR_2_MASK_OFFSET, 0x00000000);
	}

	netdev_dbg(ndev, "reg_base=0x%p irq=%d clock=%d, tx buffers: actual %d, using %d\n",
		   priv->reg_base, ndev->irq, priv->can.clock.freq,
		   hw_tx_max, priv->tx_max);

	return 0;

err_disableclks:
	pm_runtime_put(priv->dev);
	pm_runtime_disable(&pdev->dev);
err_reset:
	reset_control_assert(priv->rstc);
err_free:
	free_candev(ndev);
err:
	return ret;
}

 
static void xcan_remove(struct platform_device *pdev)
{
	struct net_device *ndev = platform_get_drvdata(pdev);
	struct xcan_priv *priv = netdev_priv(ndev);

	unregister_candev(ndev);
	pm_runtime_disable(&pdev->dev);
	reset_control_assert(priv->rstc);
	free_candev(ndev);
}

static struct platform_driver xcan_driver = {
	.probe = xcan_probe,
	.remove_new = xcan_remove,
	.driver	= {
		.name = DRIVER_NAME,
		.pm = &xcan_dev_pm_ops,
		.of_match_table	= xcan_of_match,
	},
};

module_platform_driver(xcan_driver);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Xilinx Inc");
MODULE_DESCRIPTION("Xilinx CAN interface");
