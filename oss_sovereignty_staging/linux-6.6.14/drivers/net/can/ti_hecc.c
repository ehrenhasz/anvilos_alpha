
 

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/types.h>
#include <linux/interrupt.h>
#include <linux/errno.h>
#include <linux/ethtool.h>
#include <linux/netdevice.h>
#include <linux/skbuff.h>
#include <linux/platform_device.h>
#include <linux/clk.h>
#include <linux/io.h>
#include <linux/of.h>
#include <linux/regulator/consumer.h>

#include <linux/can/dev.h>
#include <linux/can/error.h>
#include <linux/can/rx-offload.h>

#define DRV_NAME "ti_hecc"
#define HECC_MODULE_VERSION     "0.7"
MODULE_VERSION(HECC_MODULE_VERSION);
#define DRV_DESC "TI High End CAN Controller Driver " HECC_MODULE_VERSION

 
#define HECC_MAX_MAILBOXES	32	 
#define MAX_TX_PRIO		0x3F	 

 
#define HECC_MB_TX_SHIFT	2  
#define HECC_MAX_TX_MBOX	BIT(HECC_MB_TX_SHIFT)

#define HECC_TX_PRIO_SHIFT	(HECC_MB_TX_SHIFT)
#define HECC_TX_PRIO_MASK	(MAX_TX_PRIO << HECC_MB_TX_SHIFT)
#define HECC_TX_MB_MASK		(HECC_MAX_TX_MBOX - 1)
#define HECC_TX_MASK		((HECC_MAX_TX_MBOX - 1) | HECC_TX_PRIO_MASK)

 
#define HECC_MAX_RX_MBOX	(HECC_MAX_MAILBOXES - HECC_MAX_TX_MBOX)
#define HECC_RX_FIRST_MBOX	(HECC_MAX_MAILBOXES - 1)
#define HECC_RX_LAST_MBOX	(HECC_MAX_TX_MBOX)

 
#define HECC_CANME		0x0	 
#define HECC_CANMD		0x4	 
#define HECC_CANTRS		0x8	 
#define HECC_CANTRR		0xC	 
#define HECC_CANTA		0x10	 
#define HECC_CANAA		0x14	 
#define HECC_CANRMP		0x18	 
#define HECC_CANRML		0x1C	 
#define HECC_CANRFP		0x20	 
#define HECC_CANGAM		0x24	 
#define HECC_CANMC		0x28	 
#define HECC_CANBTC		0x2C	 
#define HECC_CANES		0x30	 
#define HECC_CANTEC		0x34	 
#define HECC_CANREC		0x38	 
#define HECC_CANGIF0		0x3C	 
#define HECC_CANGIM		0x40	 
#define HECC_CANGIF1		0x44	 
#define HECC_CANMIM		0x48	 
#define HECC_CANMIL		0x4C	 
#define HECC_CANOPC		0x50	 
#define HECC_CANTIOC		0x54	 
#define HECC_CANRIOC		0x58	 
#define HECC_CANLNT		0x5C	 
#define HECC_CANTOC		0x60	 
#define HECC_CANTOS		0x64	 
#define HECC_CANTIOCE		0x68	 
#define HECC_CANRIOCE		0x6C	 

 
#define HECC_CANMOTS		0x80	 

 
#define HECC_CANMID		0x0
#define HECC_CANMCF		0x4
#define HECC_CANMDL		0x8
#define HECC_CANMDH		0xC

#define HECC_SET_REG		0xFFFFFFFF
#define HECC_CANID_MASK		0x3FF	 
#define HECC_CCE_WAIT_COUNT     100	 

#define HECC_CANMC_SCM		BIT(13)	 
#define HECC_CANMC_CCR		BIT(12)	 
#define HECC_CANMC_PDR		BIT(11)	 
#define HECC_CANMC_ABO		BIT(7)	 
#define HECC_CANMC_STM		BIT(6)	 
#define HECC_CANMC_SRES		BIT(5)	 

#define HECC_CANTIOC_EN		BIT(3)	 
#define HECC_CANRIOC_EN		BIT(3)	 

#define HECC_CANMID_IDE		BIT(31)	 
#define HECC_CANMID_AME		BIT(30)	 
#define HECC_CANMID_AAM		BIT(29)	 

#define HECC_CANES_FE		BIT(24)	 
#define HECC_CANES_BE		BIT(23)	 
#define HECC_CANES_SA1		BIT(22)	 
#define HECC_CANES_CRCE		BIT(21)	 
#define HECC_CANES_SE		BIT(20)	 
#define HECC_CANES_ACKE		BIT(19)	 
#define HECC_CANES_BO		BIT(18)	 
#define HECC_CANES_EP		BIT(17)	 
#define HECC_CANES_EW		BIT(16)	 
#define HECC_CANES_SMA		BIT(5)	 
#define HECC_CANES_CCE		BIT(4)	 
#define HECC_CANES_PDA		BIT(3)	 

#define HECC_CANBTC_SAM		BIT(7)	 

#define HECC_BUS_ERROR		(HECC_CANES_FE | HECC_CANES_BE |\
				HECC_CANES_CRCE | HECC_CANES_SE |\
				HECC_CANES_ACKE)
#define HECC_CANES_FLAGS	(HECC_BUS_ERROR | HECC_CANES_BO |\
				HECC_CANES_EP | HECC_CANES_EW)

#define HECC_CANMCF_RTR		BIT(4)	 

#define HECC_CANGIF_MAIF	BIT(17)	 
#define HECC_CANGIF_TCOIF	BIT(16)  
#define HECC_CANGIF_GMIF	BIT(15)	 
#define HECC_CANGIF_AAIF	BIT(14)	 
#define HECC_CANGIF_WDIF	BIT(13)	 
#define HECC_CANGIF_WUIF	BIT(12)	 
#define HECC_CANGIF_RMLIF	BIT(11)	 
#define HECC_CANGIF_BOIF	BIT(10)	 
#define HECC_CANGIF_EPIF	BIT(9)	 
#define HECC_CANGIF_WLIF	BIT(8)	 
#define HECC_CANGIF_MBOX_MASK	0x1F	 
#define HECC_CANGIM_I1EN	BIT(1)	 
#define HECC_CANGIM_I0EN	BIT(0)	 
#define HECC_CANGIM_DEF_MASK	0x700	 
#define HECC_CANGIM_SIL		BIT(2)	 

 
static const struct can_bittiming_const ti_hecc_bittiming_const = {
	.name = DRV_NAME,
	.tseg1_min = 1,
	.tseg1_max = 16,
	.tseg2_min = 1,
	.tseg2_max = 8,
	.sjw_max = 4,
	.brp_min = 1,
	.brp_max = 256,
	.brp_inc = 1,
};

struct ti_hecc_priv {
	struct can_priv can;	 
	struct can_rx_offload offload;
	struct net_device *ndev;
	struct clk *clk;
	void __iomem *base;
	void __iomem *hecc_ram;
	void __iomem *mbx;
	bool use_hecc1int;
	spinlock_t mbx_lock;  
	u32 tx_head;
	u32 tx_tail;
	struct regulator *reg_xceiver;
};

static inline int get_tx_head_mb(struct ti_hecc_priv *priv)
{
	return priv->tx_head & HECC_TX_MB_MASK;
}

static inline int get_tx_tail_mb(struct ti_hecc_priv *priv)
{
	return priv->tx_tail & HECC_TX_MB_MASK;
}

static inline int get_tx_head_prio(struct ti_hecc_priv *priv)
{
	return (priv->tx_head >> HECC_TX_PRIO_SHIFT) & MAX_TX_PRIO;
}

static inline void hecc_write_lam(struct ti_hecc_priv *priv, u32 mbxno, u32 val)
{
	__raw_writel(val, priv->hecc_ram + mbxno * 4);
}

static inline u32 hecc_read_stamp(struct ti_hecc_priv *priv, u32 mbxno)
{
	return __raw_readl(priv->hecc_ram + HECC_CANMOTS + mbxno * 4);
}

static inline void hecc_write_mbx(struct ti_hecc_priv *priv, u32 mbxno,
				  u32 reg, u32 val)
{
	__raw_writel(val, priv->mbx + mbxno * 0x10 + reg);
}

static inline u32 hecc_read_mbx(struct ti_hecc_priv *priv, u32 mbxno, u32 reg)
{
	return __raw_readl(priv->mbx + mbxno * 0x10 + reg);
}

static inline void hecc_write(struct ti_hecc_priv *priv, u32 reg, u32 val)
{
	__raw_writel(val, priv->base + reg);
}

static inline u32 hecc_read(struct ti_hecc_priv *priv, int reg)
{
	return __raw_readl(priv->base + reg);
}

static inline void hecc_set_bit(struct ti_hecc_priv *priv, int reg,
				u32 bit_mask)
{
	hecc_write(priv, reg, hecc_read(priv, reg) | bit_mask);
}

static inline void hecc_clear_bit(struct ti_hecc_priv *priv, int reg,
				  u32 bit_mask)
{
	hecc_write(priv, reg, hecc_read(priv, reg) & ~bit_mask);
}

static inline u32 hecc_get_bit(struct ti_hecc_priv *priv, int reg, u32 bit_mask)
{
	return (hecc_read(priv, reg) & bit_mask) ? 1 : 0;
}

static int ti_hecc_set_btc(struct ti_hecc_priv *priv)
{
	struct can_bittiming *bit_timing = &priv->can.bittiming;
	u32 can_btc;

	can_btc = (bit_timing->phase_seg2 - 1) & 0x7;
	can_btc |= ((bit_timing->phase_seg1 + bit_timing->prop_seg - 1)
			& 0xF) << 3;
	if (priv->can.ctrlmode & CAN_CTRLMODE_3_SAMPLES) {
		if (bit_timing->brp > 4)
			can_btc |= HECC_CANBTC_SAM;
		else
			netdev_warn(priv->ndev,
				    "WARN: Triple sampling not set due to h/w limitations");
	}
	can_btc |= ((bit_timing->sjw - 1) & 0x3) << 8;
	can_btc |= ((bit_timing->brp - 1) & 0xFF) << 16;

	 

	hecc_write(priv, HECC_CANBTC, can_btc);
	netdev_info(priv->ndev, "setting CANBTC=%#x\n", can_btc);

	return 0;
}

static int ti_hecc_transceiver_switch(const struct ti_hecc_priv *priv,
				      int on)
{
	if (!priv->reg_xceiver)
		return 0;

	if (on)
		return regulator_enable(priv->reg_xceiver);
	else
		return regulator_disable(priv->reg_xceiver);
}

static void ti_hecc_reset(struct net_device *ndev)
{
	u32 cnt;
	struct ti_hecc_priv *priv = netdev_priv(ndev);

	netdev_dbg(ndev, "resetting hecc ...\n");
	hecc_set_bit(priv, HECC_CANMC, HECC_CANMC_SRES);

	 
	hecc_set_bit(priv, HECC_CANMC, HECC_CANMC_CCR);

	 
	cnt = HECC_CCE_WAIT_COUNT;
	while (!hecc_get_bit(priv, HECC_CANES, HECC_CANES_CCE) && cnt != 0) {
		--cnt;
		udelay(10);
	}

	 
	ti_hecc_set_btc(priv);

	 
	hecc_write(priv, HECC_CANMC, 0);

	 

	 
	cnt = HECC_CCE_WAIT_COUNT;
	while (hecc_get_bit(priv, HECC_CANES, HECC_CANES_CCE) && cnt != 0) {
		--cnt;
		udelay(10);
	}

	 
	hecc_write(priv, HECC_CANTIOC, HECC_CANTIOC_EN);
	hecc_write(priv, HECC_CANRIOC, HECC_CANRIOC_EN);

	 
	hecc_write(priv, HECC_CANTA, HECC_SET_REG);
	hecc_write(priv, HECC_CANRMP, HECC_SET_REG);
	hecc_write(priv, HECC_CANGIF0, HECC_SET_REG);
	hecc_write(priv, HECC_CANGIF1, HECC_SET_REG);
	hecc_write(priv, HECC_CANME, 0);
	hecc_write(priv, HECC_CANMD, 0);

	 
	hecc_set_bit(priv, HECC_CANMC, HECC_CANMC_SCM);
}

static void ti_hecc_start(struct net_device *ndev)
{
	struct ti_hecc_priv *priv = netdev_priv(ndev);
	u32 cnt, mbxno, mbx_mask;

	 
	ti_hecc_reset(ndev);

	priv->tx_head = HECC_TX_MASK;
	priv->tx_tail = HECC_TX_MASK;

	 
	hecc_write(priv, HECC_CANGAM, HECC_SET_REG);

	 
	for (cnt = 0; cnt < HECC_MAX_RX_MBOX; cnt++) {
		mbxno = HECC_MAX_MAILBOXES - 1 - cnt;
		mbx_mask = BIT(mbxno);
		hecc_clear_bit(priv, HECC_CANME, mbx_mask);
		hecc_write_mbx(priv, mbxno, HECC_CANMID, HECC_CANMID_AME);
		hecc_write_lam(priv, mbxno, HECC_SET_REG);
		hecc_set_bit(priv, HECC_CANMD, mbx_mask);
		hecc_set_bit(priv, HECC_CANME, mbx_mask);
		hecc_set_bit(priv, HECC_CANMIM, mbx_mask);
	}

	 
	hecc_set_bit(priv, HECC_CANMIM, BIT(HECC_MAX_TX_MBOX) - 1);

	 
	mbx_mask = ~BIT(HECC_RX_LAST_MBOX);
	hecc_write(priv, HECC_CANOPC, mbx_mask);

	 
	if (priv->use_hecc1int) {
		hecc_write(priv, HECC_CANMIL, HECC_SET_REG);
		hecc_write(priv, HECC_CANGIM, HECC_CANGIM_DEF_MASK |
			HECC_CANGIM_I1EN | HECC_CANGIM_SIL);
	} else {
		hecc_write(priv, HECC_CANMIL, 0);
		hecc_write(priv, HECC_CANGIM,
			   HECC_CANGIM_DEF_MASK | HECC_CANGIM_I0EN);
	}
	priv->can.state = CAN_STATE_ERROR_ACTIVE;
}

static void ti_hecc_stop(struct net_device *ndev)
{
	struct ti_hecc_priv *priv = netdev_priv(ndev);

	 
	hecc_set_bit(priv, HECC_CANMC, HECC_CANMC_CCR);

	 
	hecc_write(priv, HECC_CANGIM, 0);
	hecc_write(priv, HECC_CANMIM, 0);
	hecc_write(priv, HECC_CANME, 0);
	priv->can.state = CAN_STATE_STOPPED;
}

static int ti_hecc_do_set_mode(struct net_device *ndev, enum can_mode mode)
{
	int ret = 0;

	switch (mode) {
	case CAN_MODE_START:
		ti_hecc_start(ndev);
		netif_wake_queue(ndev);
		break;
	default:
		ret = -EOPNOTSUPP;
		break;
	}

	return ret;
}

static int ti_hecc_get_berr_counter(const struct net_device *ndev,
				    struct can_berr_counter *bec)
{
	struct ti_hecc_priv *priv = netdev_priv(ndev);

	bec->txerr = hecc_read(priv, HECC_CANTEC);
	bec->rxerr = hecc_read(priv, HECC_CANREC);

	return 0;
}

 
static netdev_tx_t ti_hecc_xmit(struct sk_buff *skb, struct net_device *ndev)
{
	struct ti_hecc_priv *priv = netdev_priv(ndev);
	struct can_frame *cf = (struct can_frame *)skb->data;
	u32 mbxno, mbx_mask, data;
	unsigned long flags;

	if (can_dev_dropped_skb(ndev, skb))
		return NETDEV_TX_OK;

	mbxno = get_tx_head_mb(priv);
	mbx_mask = BIT(mbxno);
	spin_lock_irqsave(&priv->mbx_lock, flags);
	if (unlikely(hecc_read(priv, HECC_CANME) & mbx_mask)) {
		spin_unlock_irqrestore(&priv->mbx_lock, flags);
		netif_stop_queue(ndev);
		netdev_err(priv->ndev,
			   "BUG: TX mbx not ready tx_head=%08X, tx_tail=%08X\n",
			   priv->tx_head, priv->tx_tail);
		return NETDEV_TX_BUSY;
	}
	spin_unlock_irqrestore(&priv->mbx_lock, flags);

	 
	data = cf->len | (get_tx_head_prio(priv) << 8);
	if (cf->can_id & CAN_RTR_FLAG)  
		data |= HECC_CANMCF_RTR;
	hecc_write_mbx(priv, mbxno, HECC_CANMCF, data);

	if (cf->can_id & CAN_EFF_FLAG)  
		data = (cf->can_id & CAN_EFF_MASK) | HECC_CANMID_IDE;
	else  
		data = (cf->can_id & CAN_SFF_MASK) << 18;
	hecc_write_mbx(priv, mbxno, HECC_CANMID, data);
	hecc_write_mbx(priv, mbxno, HECC_CANMDL,
		       be32_to_cpu(*(__be32 *)(cf->data)));
	if (cf->len > 4)
		hecc_write_mbx(priv, mbxno, HECC_CANMDH,
			       be32_to_cpu(*(__be32 *)(cf->data + 4)));
	else
		*(u32 *)(cf->data + 4) = 0;
	can_put_echo_skb(skb, ndev, mbxno, 0);

	spin_lock_irqsave(&priv->mbx_lock, flags);
	--priv->tx_head;
	if ((hecc_read(priv, HECC_CANME) & BIT(get_tx_head_mb(priv))) ||
	    (priv->tx_head & HECC_TX_MASK) == HECC_TX_MASK) {
		netif_stop_queue(ndev);
	}
	hecc_set_bit(priv, HECC_CANME, mbx_mask);
	spin_unlock_irqrestore(&priv->mbx_lock, flags);

	hecc_write(priv, HECC_CANTRS, mbx_mask);

	return NETDEV_TX_OK;
}

static inline
struct ti_hecc_priv *rx_offload_to_priv(struct can_rx_offload *offload)
{
	return container_of(offload, struct ti_hecc_priv, offload);
}

static struct sk_buff *ti_hecc_mailbox_read(struct can_rx_offload *offload,
					    unsigned int mbxno, u32 *timestamp,
					    bool drop)
{
	struct ti_hecc_priv *priv = rx_offload_to_priv(offload);
	struct sk_buff *skb;
	struct can_frame *cf;
	u32 data, mbx_mask;

	mbx_mask = BIT(mbxno);

	if (unlikely(drop)) {
		skb = ERR_PTR(-ENOBUFS);
		goto mark_as_read;
	}

	skb = alloc_can_skb(offload->dev, &cf);
	if (unlikely(!skb)) {
		skb = ERR_PTR(-ENOMEM);
		goto mark_as_read;
	}

	data = hecc_read_mbx(priv, mbxno, HECC_CANMID);
	if (data & HECC_CANMID_IDE)
		cf->can_id = (data & CAN_EFF_MASK) | CAN_EFF_FLAG;
	else
		cf->can_id = (data >> 18) & CAN_SFF_MASK;

	data = hecc_read_mbx(priv, mbxno, HECC_CANMCF);
	if (data & HECC_CANMCF_RTR)
		cf->can_id |= CAN_RTR_FLAG;
	cf->len = can_cc_dlc2len(data & 0xF);

	data = hecc_read_mbx(priv, mbxno, HECC_CANMDL);
	*(__be32 *)(cf->data) = cpu_to_be32(data);
	if (cf->len > 4) {
		data = hecc_read_mbx(priv, mbxno, HECC_CANMDH);
		*(__be32 *)(cf->data + 4) = cpu_to_be32(data);
	}

	*timestamp = hecc_read_stamp(priv, mbxno);

	 
	if (unlikely(mbxno == HECC_RX_LAST_MBOX &&
		     hecc_read(priv, HECC_CANRML) & mbx_mask))
		skb = ERR_PTR(-ENOBUFS);

 mark_as_read:
	hecc_write(priv, HECC_CANRMP, mbx_mask);

	return skb;
}

static int ti_hecc_error(struct net_device *ndev, int int_status,
			 int err_status)
{
	struct ti_hecc_priv *priv = netdev_priv(ndev);
	struct can_frame *cf;
	struct sk_buff *skb;
	u32 timestamp;
	int err;

	if (err_status & HECC_BUS_ERROR) {
		 
		skb = alloc_can_err_skb(ndev, &cf);
		if (!skb) {
			if (net_ratelimit())
				netdev_err(priv->ndev,
					   "%s: alloc_can_err_skb() failed\n",
					   __func__);
			return -ENOMEM;
		}

		++priv->can.can_stats.bus_error;
		cf->can_id |= CAN_ERR_BUSERROR | CAN_ERR_PROT;
		if (err_status & HECC_CANES_FE)
			cf->data[2] |= CAN_ERR_PROT_FORM;
		if (err_status & HECC_CANES_BE)
			cf->data[2] |= CAN_ERR_PROT_BIT;
		if (err_status & HECC_CANES_SE)
			cf->data[2] |= CAN_ERR_PROT_STUFF;
		if (err_status & HECC_CANES_CRCE)
			cf->data[3] = CAN_ERR_PROT_LOC_CRC_SEQ;
		if (err_status & HECC_CANES_ACKE)
			cf->data[3] = CAN_ERR_PROT_LOC_ACK;

		timestamp = hecc_read(priv, HECC_CANLNT);
		err = can_rx_offload_queue_timestamp(&priv->offload, skb,
						     timestamp);
		if (err)
			ndev->stats.rx_fifo_errors++;
	}

	hecc_write(priv, HECC_CANES, HECC_CANES_FLAGS);

	return 0;
}

static void ti_hecc_change_state(struct net_device *ndev,
				 enum can_state rx_state,
				 enum can_state tx_state)
{
	struct ti_hecc_priv *priv = netdev_priv(ndev);
	struct can_frame *cf;
	struct sk_buff *skb;
	u32 timestamp;
	int err;

	skb = alloc_can_err_skb(priv->ndev, &cf);
	if (unlikely(!skb)) {
		priv->can.state = max(tx_state, rx_state);
		return;
	}

	can_change_state(priv->ndev, cf, tx_state, rx_state);

	if (max(tx_state, rx_state) != CAN_STATE_BUS_OFF) {
		cf->can_id |= CAN_ERR_CNT;
		cf->data[6] = hecc_read(priv, HECC_CANTEC);
		cf->data[7] = hecc_read(priv, HECC_CANREC);
	}

	timestamp = hecc_read(priv, HECC_CANLNT);
	err = can_rx_offload_queue_timestamp(&priv->offload, skb, timestamp);
	if (err)
		ndev->stats.rx_fifo_errors++;
}

static irqreturn_t ti_hecc_interrupt(int irq, void *dev_id)
{
	struct net_device *ndev = (struct net_device *)dev_id;
	struct ti_hecc_priv *priv = netdev_priv(ndev);
	struct net_device_stats *stats = &ndev->stats;
	u32 mbxno, mbx_mask, int_status, err_status, stamp;
	unsigned long flags, rx_pending;
	u32 handled = 0;

	int_status = hecc_read(priv,
			       priv->use_hecc1int ?
			       HECC_CANGIF1 : HECC_CANGIF0);

	if (!int_status)
		return IRQ_NONE;

	err_status = hecc_read(priv, HECC_CANES);
	if (unlikely(err_status & HECC_CANES_FLAGS))
		ti_hecc_error(ndev, int_status, err_status);

	if (unlikely(int_status & HECC_CANGIM_DEF_MASK)) {
		enum can_state rx_state, tx_state;
		u32 rec = hecc_read(priv, HECC_CANREC);
		u32 tec = hecc_read(priv, HECC_CANTEC);

		if (int_status & HECC_CANGIF_WLIF) {
			handled |= HECC_CANGIF_WLIF;
			rx_state = rec >= tec ? CAN_STATE_ERROR_WARNING : 0;
			tx_state = rec <= tec ? CAN_STATE_ERROR_WARNING : 0;
			netdev_dbg(priv->ndev, "Error Warning interrupt\n");
			ti_hecc_change_state(ndev, rx_state, tx_state);
		}

		if (int_status & HECC_CANGIF_EPIF) {
			handled |= HECC_CANGIF_EPIF;
			rx_state = rec >= tec ? CAN_STATE_ERROR_PASSIVE : 0;
			tx_state = rec <= tec ? CAN_STATE_ERROR_PASSIVE : 0;
			netdev_dbg(priv->ndev, "Error passive interrupt\n");
			ti_hecc_change_state(ndev, rx_state, tx_state);
		}

		if (int_status & HECC_CANGIF_BOIF) {
			handled |= HECC_CANGIF_BOIF;
			rx_state = CAN_STATE_BUS_OFF;
			tx_state = CAN_STATE_BUS_OFF;
			netdev_dbg(priv->ndev, "Bus off interrupt\n");

			 
			hecc_write(priv, HECC_CANGIM, 0);
			can_bus_off(ndev);
			ti_hecc_change_state(ndev, rx_state, tx_state);
		}
	} else if (unlikely(priv->can.state != CAN_STATE_ERROR_ACTIVE)) {
		enum can_state new_state, tx_state, rx_state;
		u32 rec = hecc_read(priv, HECC_CANREC);
		u32 tec = hecc_read(priv, HECC_CANTEC);

		if (rec >= 128 || tec >= 128)
			new_state = CAN_STATE_ERROR_PASSIVE;
		else if (rec >= 96 || tec >= 96)
			new_state = CAN_STATE_ERROR_WARNING;
		else
			new_state = CAN_STATE_ERROR_ACTIVE;

		if (new_state < priv->can.state) {
			rx_state = rec >= tec ? new_state : 0;
			tx_state = rec <= tec ? new_state : 0;
			ti_hecc_change_state(ndev, rx_state, tx_state);
		}
	}

	if (int_status & HECC_CANGIF_GMIF) {
		while (priv->tx_tail - priv->tx_head > 0) {
			mbxno = get_tx_tail_mb(priv);
			mbx_mask = BIT(mbxno);
			if (!(mbx_mask & hecc_read(priv, HECC_CANTA)))
				break;
			hecc_write(priv, HECC_CANTA, mbx_mask);
			spin_lock_irqsave(&priv->mbx_lock, flags);
			hecc_clear_bit(priv, HECC_CANME, mbx_mask);
			spin_unlock_irqrestore(&priv->mbx_lock, flags);
			stamp = hecc_read_stamp(priv, mbxno);
			stats->tx_bytes +=
				can_rx_offload_get_echo_skb_queue_timestamp(&priv->offload,
									    mbxno, stamp, NULL);
			stats->tx_packets++;
			--priv->tx_tail;
		}

		 
		if ((priv->tx_head == priv->tx_tail &&
		     ((priv->tx_head & HECC_TX_MASK) != HECC_TX_MASK)) ||
		    (((priv->tx_tail & HECC_TX_MASK) == HECC_TX_MASK) &&
		     ((priv->tx_head & HECC_TX_MASK) == HECC_TX_MASK)))
			netif_wake_queue(ndev);

		 
		while ((rx_pending = hecc_read(priv, HECC_CANRMP))) {
			can_rx_offload_irq_offload_timestamp(&priv->offload,
							     rx_pending);
		}
	}

	 
	if (priv->use_hecc1int) {
		hecc_write(priv, HECC_CANGIF1, handled);
		int_status = hecc_read(priv, HECC_CANGIF1);
	} else {
		hecc_write(priv, HECC_CANGIF0, handled);
		int_status = hecc_read(priv, HECC_CANGIF0);
	}

	can_rx_offload_irq_finish(&priv->offload);

	return IRQ_HANDLED;
}

static int ti_hecc_open(struct net_device *ndev)
{
	struct ti_hecc_priv *priv = netdev_priv(ndev);
	int err;

	err = request_irq(ndev->irq, ti_hecc_interrupt, IRQF_SHARED,
			  ndev->name, ndev);
	if (err) {
		netdev_err(ndev, "error requesting interrupt\n");
		return err;
	}

	ti_hecc_transceiver_switch(priv, 1);

	 
	err = open_candev(ndev);
	if (err) {
		netdev_err(ndev, "open_candev() failed %d\n", err);
		ti_hecc_transceiver_switch(priv, 0);
		free_irq(ndev->irq, ndev);
		return err;
	}

	ti_hecc_start(ndev);
	can_rx_offload_enable(&priv->offload);
	netif_start_queue(ndev);

	return 0;
}

static int ti_hecc_close(struct net_device *ndev)
{
	struct ti_hecc_priv *priv = netdev_priv(ndev);

	netif_stop_queue(ndev);
	can_rx_offload_disable(&priv->offload);
	ti_hecc_stop(ndev);
	free_irq(ndev->irq, ndev);
	close_candev(ndev);
	ti_hecc_transceiver_switch(priv, 0);

	return 0;
}

static const struct net_device_ops ti_hecc_netdev_ops = {
	.ndo_open		= ti_hecc_open,
	.ndo_stop		= ti_hecc_close,
	.ndo_start_xmit		= ti_hecc_xmit,
	.ndo_change_mtu		= can_change_mtu,
};

static const struct ethtool_ops ti_hecc_ethtool_ops = {
	.get_ts_info = ethtool_op_get_ts_info,
};

static const struct of_device_id ti_hecc_dt_ids[] = {
	{
		.compatible = "ti,am3517-hecc",
	},
	{ }
};
MODULE_DEVICE_TABLE(of, ti_hecc_dt_ids);

static int ti_hecc_probe(struct platform_device *pdev)
{
	struct net_device *ndev = (struct net_device *)0;
	struct ti_hecc_priv *priv;
	struct device_node *np = pdev->dev.of_node;
	struct regulator *reg_xceiver;
	int err = -ENODEV;

	if (!IS_ENABLED(CONFIG_OF) || !np)
		return -EINVAL;

	reg_xceiver = devm_regulator_get(&pdev->dev, "xceiver");
	if (PTR_ERR(reg_xceiver) == -EPROBE_DEFER)
		return -EPROBE_DEFER;
	else if (IS_ERR(reg_xceiver))
		reg_xceiver = NULL;

	ndev = alloc_candev(sizeof(struct ti_hecc_priv), HECC_MAX_TX_MBOX);
	if (!ndev) {
		dev_err(&pdev->dev, "alloc_candev failed\n");
		return -ENOMEM;
	}
	priv = netdev_priv(ndev);

	 
	priv->base = devm_platform_ioremap_resource_byname(pdev, "hecc");
	if (IS_ERR(priv->base)) {
		dev_err(&pdev->dev, "hecc ioremap failed\n");
		err = PTR_ERR(priv->base);
		goto probe_exit_candev;
	}

	 
	priv->hecc_ram = devm_platform_ioremap_resource_byname(pdev,
							       "hecc-ram");
	if (IS_ERR(priv->hecc_ram)) {
		dev_err(&pdev->dev, "hecc-ram ioremap failed\n");
		err = PTR_ERR(priv->hecc_ram);
		goto probe_exit_candev;
	}

	 
	priv->mbx = devm_platform_ioremap_resource_byname(pdev, "mbx");
	if (IS_ERR(priv->mbx)) {
		dev_err(&pdev->dev, "mbx ioremap failed\n");
		err = PTR_ERR(priv->mbx);
		goto probe_exit_candev;
	}

	ndev->irq = platform_get_irq(pdev, 0);
	if (ndev->irq < 0) {
		err = ndev->irq;
		goto probe_exit_candev;
	}

	priv->ndev = ndev;
	priv->reg_xceiver = reg_xceiver;
	priv->use_hecc1int = of_property_read_bool(np, "ti,use-hecc1int");

	priv->can.bittiming_const = &ti_hecc_bittiming_const;
	priv->can.do_set_mode = ti_hecc_do_set_mode;
	priv->can.do_get_berr_counter = ti_hecc_get_berr_counter;
	priv->can.ctrlmode_supported = CAN_CTRLMODE_3_SAMPLES;

	spin_lock_init(&priv->mbx_lock);
	ndev->flags |= IFF_ECHO;
	platform_set_drvdata(pdev, ndev);
	SET_NETDEV_DEV(ndev, &pdev->dev);
	ndev->netdev_ops = &ti_hecc_netdev_ops;
	ndev->ethtool_ops = &ti_hecc_ethtool_ops;

	priv->clk = clk_get(&pdev->dev, "hecc_ck");
	if (IS_ERR(priv->clk)) {
		dev_err(&pdev->dev, "No clock available\n");
		err = PTR_ERR(priv->clk);
		priv->clk = NULL;
		goto probe_exit_candev;
	}
	priv->can.clock.freq = clk_get_rate(priv->clk);

	err = clk_prepare_enable(priv->clk);
	if (err) {
		dev_err(&pdev->dev, "clk_prepare_enable() failed\n");
		goto probe_exit_release_clk;
	}

	priv->offload.mailbox_read = ti_hecc_mailbox_read;
	priv->offload.mb_first = HECC_RX_FIRST_MBOX;
	priv->offload.mb_last = HECC_RX_LAST_MBOX;
	err = can_rx_offload_add_timestamp(ndev, &priv->offload);
	if (err) {
		dev_err(&pdev->dev, "can_rx_offload_add_timestamp() failed\n");
		goto probe_exit_disable_clk;
	}

	err = register_candev(ndev);
	if (err) {
		dev_err(&pdev->dev, "register_candev() failed\n");
		goto probe_exit_offload;
	}

	dev_info(&pdev->dev, "device registered (reg_base=%p, irq=%u)\n",
		 priv->base, (u32)ndev->irq);

	return 0;

probe_exit_offload:
	can_rx_offload_del(&priv->offload);
probe_exit_disable_clk:
	clk_disable_unprepare(priv->clk);
probe_exit_release_clk:
	clk_put(priv->clk);
probe_exit_candev:
	free_candev(ndev);

	return err;
}

static void ti_hecc_remove(struct platform_device *pdev)
{
	struct net_device *ndev = platform_get_drvdata(pdev);
	struct ti_hecc_priv *priv = netdev_priv(ndev);

	unregister_candev(ndev);
	clk_disable_unprepare(priv->clk);
	clk_put(priv->clk);
	can_rx_offload_del(&priv->offload);
	free_candev(ndev);
}

#ifdef CONFIG_PM
static int ti_hecc_suspend(struct platform_device *pdev, pm_message_t state)
{
	struct net_device *dev = platform_get_drvdata(pdev);
	struct ti_hecc_priv *priv = netdev_priv(dev);

	if (netif_running(dev)) {
		netif_stop_queue(dev);
		netif_device_detach(dev);
	}

	hecc_set_bit(priv, HECC_CANMC, HECC_CANMC_PDR);
	priv->can.state = CAN_STATE_SLEEPING;

	clk_disable_unprepare(priv->clk);

	return 0;
}

static int ti_hecc_resume(struct platform_device *pdev)
{
	struct net_device *dev = platform_get_drvdata(pdev);
	struct ti_hecc_priv *priv = netdev_priv(dev);
	int err;

	err = clk_prepare_enable(priv->clk);
	if (err)
		return err;

	hecc_clear_bit(priv, HECC_CANMC, HECC_CANMC_PDR);
	priv->can.state = CAN_STATE_ERROR_ACTIVE;

	if (netif_running(dev)) {
		netif_device_attach(dev);
		netif_start_queue(dev);
	}

	return 0;
}
#else
#define ti_hecc_suspend NULL
#define ti_hecc_resume NULL
#endif

 
static struct platform_driver ti_hecc_driver = {
	.driver = {
		.name    = DRV_NAME,
		.of_match_table = ti_hecc_dt_ids,
	},
	.probe = ti_hecc_probe,
	.remove_new = ti_hecc_remove,
	.suspend = ti_hecc_suspend,
	.resume = ti_hecc_resume,
};

module_platform_driver(ti_hecc_driver);

MODULE_AUTHOR("Anant Gole <anantgole@ti.com>");
MODULE_LICENSE("GPL v2");
MODULE_DESCRIPTION(DRV_DESC);
MODULE_ALIAS("platform:" DRV_NAME);
