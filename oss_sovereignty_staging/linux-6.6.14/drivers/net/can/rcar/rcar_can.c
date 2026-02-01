
 

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/types.h>
#include <linux/interrupt.h>
#include <linux/errno.h>
#include <linux/ethtool.h>
#include <linux/netdevice.h>
#include <linux/platform_device.h>
#include <linux/can/dev.h>
#include <linux/clk.h>
#include <linux/of.h>

#define RCAR_CAN_DRV_NAME	"rcar_can"

 
enum CLKR {
	CLKR_CLKP1 = 0,  
	CLKR_CLKP2 = 1,  
	CLKR_CLKEXT = 3,  
};

#define RCAR_SUPPORTED_CLOCKS	(BIT(CLKR_CLKP1) | BIT(CLKR_CLKP2) | \
				 BIT(CLKR_CLKEXT))

 
#define RCAR_CAN_N_MBX		64  
#define RCAR_CAN_RX_FIFO_MBX	60  
#define RCAR_CAN_TX_FIFO_MBX	56  
#define RCAR_CAN_FIFO_DEPTH	4

 
struct rcar_can_mbox_regs {
	u32 id;		 
	u8 stub;	 
	u8 dlc;		 
	u8 data[8];	 
	u8 tsh;		 
	u8 tsl;		 
};

struct rcar_can_regs {
	struct rcar_can_mbox_regs mb[RCAR_CAN_N_MBX];  
	u32 mkr_2_9[8];	 
	u32 fidcr[2];	 
	u32 mkivlr1;	 
	u32 mier1;	 
	u32 mkr_0_1[2];	 
	u32 mkivlr0;     
	u32 mier0;       
	u8 pad_440[0x3c0];
	u8 mctl[64];	 
	u16 ctlr;	 
	u16 str;	 
	u8 bcr[3];	 
	u8 clkr;	 
	u8 rfcr;	 
	u8 rfpcr;	 
	u8 tfcr;	 
	u8 tfpcr;        
	u8 eier;	 
	u8 eifr;	 
	u8 recr;	 
	u8 tecr;         
	u8 ecsr;	 
	u8 cssr;	 
	u8 mssr;	 
	u8 msmr;	 
	u16 tsr;	 
	u8 afsr;	 
	u8 pad_857;
	u8 tcr;		 
	u8 pad_859[7];
	u8 ier;		 
	u8 isr;		 
	u8 pad_862;
	u8 mbsmr;	 
};

struct rcar_can_priv {
	struct can_priv can;	 
	struct net_device *ndev;
	struct napi_struct napi;
	struct rcar_can_regs __iomem *regs;
	struct clk *clk;
	struct clk *can_clk;
	u32 tx_head;
	u32 tx_tail;
	u8 clock_select;
	u8 ier;
};

static const struct can_bittiming_const rcar_can_bittiming_const = {
	.name = RCAR_CAN_DRV_NAME,
	.tseg1_min = 4,
	.tseg1_max = 16,
	.tseg2_min = 2,
	.tseg2_max = 8,
	.sjw_max = 4,
	.brp_min = 1,
	.brp_max = 1024,
	.brp_inc = 1,
};

 
#define RCAR_CAN_CTLR_BOM	(3 << 11)  
#define RCAR_CAN_CTLR_BOM_ENT	(1 << 11)  
					 
#define RCAR_CAN_CTLR_SLPM	(1 << 10)
#define RCAR_CAN_CTLR_CANM	(3 << 8)  
#define RCAR_CAN_CTLR_CANM_HALT	(1 << 9)
#define RCAR_CAN_CTLR_CANM_RESET (1 << 8)
#define RCAR_CAN_CTLR_CANM_FORCE_RESET (3 << 8)
#define RCAR_CAN_CTLR_MLM	(1 << 3)  
#define RCAR_CAN_CTLR_IDFM	(3 << 1)  
#define RCAR_CAN_CTLR_IDFM_MIXED (1 << 2)  
#define RCAR_CAN_CTLR_MBM	(1 << 0)  

 
#define RCAR_CAN_STR_RSTST	(1 << 8)  

 
#define RCAR_CAN_FIDCR_IDE	(1 << 31)  
#define RCAR_CAN_FIDCR_RTR	(1 << 30)  

 
#define RCAR_CAN_RFCR_RFEST	(1 << 7)  
#define RCAR_CAN_RFCR_RFE	(1 << 0)  

 
#define RCAR_CAN_TFCR_TFUST	(7 << 1)  
					 
#define RCAR_CAN_TFCR_TFUST_SHIFT 1	 
					 
#define RCAR_CAN_TFCR_TFE	(1 << 0)  

#define RCAR_CAN_N_RX_MKREGS1	2	 
					 
#define RCAR_CAN_N_RX_MKREGS2	8

 
#define RCAR_CAN_BCR_TSEG1(x)	(((x) & 0x0f) << 20)
#define RCAR_CAN_BCR_BPR(x)	(((x) & 0x3ff) << 8)
#define RCAR_CAN_BCR_SJW(x)	(((x) & 0x3) << 4)
#define RCAR_CAN_BCR_TSEG2(x)	((x) & 0x07)

 
#define RCAR_CAN_IDE		(1 << 31)
#define RCAR_CAN_RTR		(1 << 30)
#define RCAR_CAN_SID_SHIFT	18

 
#define RCAR_CAN_MIER1_RXFIE	(1 << 28)  
#define RCAR_CAN_MIER1_TXFIE	(1 << 24)  

 
#define RCAR_CAN_IER_ERSIE	(1 << 5)  
#define RCAR_CAN_IER_RXFIE	(1 << 4)  
					 
#define RCAR_CAN_IER_TXFIE	(1 << 3)  
					 
 
#define RCAR_CAN_ISR_ERSF	(1 << 5)  
#define RCAR_CAN_ISR_RXFF	(1 << 4)  
					 
#define RCAR_CAN_ISR_TXFF	(1 << 3)  
					 

 
#define RCAR_CAN_EIER_BLIE	(1 << 7)  
#define RCAR_CAN_EIER_OLIE	(1 << 6)  
					 
#define RCAR_CAN_EIER_ORIE	(1 << 5)  
#define RCAR_CAN_EIER_BORIE	(1 << 4)  
#define RCAR_CAN_EIER_BOEIE	(1 << 3)  
#define RCAR_CAN_EIER_EPIE	(1 << 2)  
#define RCAR_CAN_EIER_EWIE	(1 << 1)  
#define RCAR_CAN_EIER_BEIE	(1 << 0)  

 
#define RCAR_CAN_EIFR_BLIF	(1 << 7)  
#define RCAR_CAN_EIFR_OLIF	(1 << 6)  
					  
#define RCAR_CAN_EIFR_ORIF	(1 << 5)  
#define RCAR_CAN_EIFR_BORIF	(1 << 4)  
#define RCAR_CAN_EIFR_BOEIF	(1 << 3)  
#define RCAR_CAN_EIFR_EPIF	(1 << 2)  
#define RCAR_CAN_EIFR_EWIF	(1 << 1)  
#define RCAR_CAN_EIFR_BEIF	(1 << 0)  

 
#define RCAR_CAN_ECSR_EDPM	(1 << 7)  
#define RCAR_CAN_ECSR_ADEF	(1 << 6)  
#define RCAR_CAN_ECSR_BE0F	(1 << 5)  
#define RCAR_CAN_ECSR_BE1F	(1 << 4)  
#define RCAR_CAN_ECSR_CEF	(1 << 3)  
#define RCAR_CAN_ECSR_AEF	(1 << 2)  
#define RCAR_CAN_ECSR_FEF	(1 << 1)  
#define RCAR_CAN_ECSR_SEF	(1 << 0)  

#define RCAR_CAN_NAPI_WEIGHT	4
#define MAX_STR_READS		0x100

static void tx_failure_cleanup(struct net_device *ndev)
{
	int i;

	for (i = 0; i < RCAR_CAN_FIFO_DEPTH; i++)
		can_free_echo_skb(ndev, i, NULL);
}

static void rcar_can_error(struct net_device *ndev)
{
	struct rcar_can_priv *priv = netdev_priv(ndev);
	struct can_frame *cf;
	struct sk_buff *skb;
	u8 eifr, txerr = 0, rxerr = 0;

	 
	skb = alloc_can_err_skb(ndev, &cf);

	eifr = readb(&priv->regs->eifr);
	if (eifr & (RCAR_CAN_EIFR_EWIF | RCAR_CAN_EIFR_EPIF)) {
		txerr = readb(&priv->regs->tecr);
		rxerr = readb(&priv->regs->recr);
		if (skb)
			cf->can_id |= CAN_ERR_CRTL;
	}
	if (eifr & RCAR_CAN_EIFR_BEIF) {
		int rx_errors = 0, tx_errors = 0;
		u8 ecsr;

		netdev_dbg(priv->ndev, "Bus error interrupt:\n");
		if (skb)
			cf->can_id |= CAN_ERR_BUSERROR | CAN_ERR_PROT;

		ecsr = readb(&priv->regs->ecsr);
		if (ecsr & RCAR_CAN_ECSR_ADEF) {
			netdev_dbg(priv->ndev, "ACK Delimiter Error\n");
			tx_errors++;
			writeb(~RCAR_CAN_ECSR_ADEF, &priv->regs->ecsr);
			if (skb)
				cf->data[3] = CAN_ERR_PROT_LOC_ACK_DEL;
		}
		if (ecsr & RCAR_CAN_ECSR_BE0F) {
			netdev_dbg(priv->ndev, "Bit Error (dominant)\n");
			tx_errors++;
			writeb(~RCAR_CAN_ECSR_BE0F, &priv->regs->ecsr);
			if (skb)
				cf->data[2] |= CAN_ERR_PROT_BIT0;
		}
		if (ecsr & RCAR_CAN_ECSR_BE1F) {
			netdev_dbg(priv->ndev, "Bit Error (recessive)\n");
			tx_errors++;
			writeb(~RCAR_CAN_ECSR_BE1F, &priv->regs->ecsr);
			if (skb)
				cf->data[2] |= CAN_ERR_PROT_BIT1;
		}
		if (ecsr & RCAR_CAN_ECSR_CEF) {
			netdev_dbg(priv->ndev, "CRC Error\n");
			rx_errors++;
			writeb(~RCAR_CAN_ECSR_CEF, &priv->regs->ecsr);
			if (skb)
				cf->data[3] = CAN_ERR_PROT_LOC_CRC_SEQ;
		}
		if (ecsr & RCAR_CAN_ECSR_AEF) {
			netdev_dbg(priv->ndev, "ACK Error\n");
			tx_errors++;
			writeb(~RCAR_CAN_ECSR_AEF, &priv->regs->ecsr);
			if (skb) {
				cf->can_id |= CAN_ERR_ACK;
				cf->data[3] = CAN_ERR_PROT_LOC_ACK;
			}
		}
		if (ecsr & RCAR_CAN_ECSR_FEF) {
			netdev_dbg(priv->ndev, "Form Error\n");
			rx_errors++;
			writeb(~RCAR_CAN_ECSR_FEF, &priv->regs->ecsr);
			if (skb)
				cf->data[2] |= CAN_ERR_PROT_FORM;
		}
		if (ecsr & RCAR_CAN_ECSR_SEF) {
			netdev_dbg(priv->ndev, "Stuff Error\n");
			rx_errors++;
			writeb(~RCAR_CAN_ECSR_SEF, &priv->regs->ecsr);
			if (skb)
				cf->data[2] |= CAN_ERR_PROT_STUFF;
		}

		priv->can.can_stats.bus_error++;
		ndev->stats.rx_errors += rx_errors;
		ndev->stats.tx_errors += tx_errors;
		writeb(~RCAR_CAN_EIFR_BEIF, &priv->regs->eifr);
	}
	if (eifr & RCAR_CAN_EIFR_EWIF) {
		netdev_dbg(priv->ndev, "Error warning interrupt\n");
		priv->can.state = CAN_STATE_ERROR_WARNING;
		priv->can.can_stats.error_warning++;
		 
		writeb(~RCAR_CAN_EIFR_EWIF, &priv->regs->eifr);
		if (skb)
			cf->data[1] = txerr > rxerr ? CAN_ERR_CRTL_TX_WARNING :
					      CAN_ERR_CRTL_RX_WARNING;
	}
	if (eifr & RCAR_CAN_EIFR_EPIF) {
		netdev_dbg(priv->ndev, "Error passive interrupt\n");
		priv->can.state = CAN_STATE_ERROR_PASSIVE;
		priv->can.can_stats.error_passive++;
		 
		writeb(~RCAR_CAN_EIFR_EPIF, &priv->regs->eifr);
		if (skb)
			cf->data[1] = txerr > rxerr ? CAN_ERR_CRTL_TX_PASSIVE :
					      CAN_ERR_CRTL_RX_PASSIVE;
	}
	if (eifr & RCAR_CAN_EIFR_BOEIF) {
		netdev_dbg(priv->ndev, "Bus-off entry interrupt\n");
		tx_failure_cleanup(ndev);
		priv->ier = RCAR_CAN_IER_ERSIE;
		writeb(priv->ier, &priv->regs->ier);
		priv->can.state = CAN_STATE_BUS_OFF;
		 
		writeb(~RCAR_CAN_EIFR_BOEIF, &priv->regs->eifr);
		priv->can.can_stats.bus_off++;
		can_bus_off(ndev);
		if (skb)
			cf->can_id |= CAN_ERR_BUSOFF;
	} else if (skb) {
		cf->can_id |= CAN_ERR_CNT;
		cf->data[6] = txerr;
		cf->data[7] = rxerr;
	}
	if (eifr & RCAR_CAN_EIFR_ORIF) {
		netdev_dbg(priv->ndev, "Receive overrun error interrupt\n");
		ndev->stats.rx_over_errors++;
		ndev->stats.rx_errors++;
		writeb(~RCAR_CAN_EIFR_ORIF, &priv->regs->eifr);
		if (skb) {
			cf->can_id |= CAN_ERR_CRTL;
			cf->data[1] = CAN_ERR_CRTL_RX_OVERFLOW;
		}
	}
	if (eifr & RCAR_CAN_EIFR_OLIF) {
		netdev_dbg(priv->ndev,
			   "Overload Frame Transmission error interrupt\n");
		ndev->stats.rx_over_errors++;
		ndev->stats.rx_errors++;
		writeb(~RCAR_CAN_EIFR_OLIF, &priv->regs->eifr);
		if (skb) {
			cf->can_id |= CAN_ERR_PROT;
			cf->data[2] |= CAN_ERR_PROT_OVERLOAD;
		}
	}

	if (skb)
		netif_rx(skb);
}

static void rcar_can_tx_done(struct net_device *ndev)
{
	struct rcar_can_priv *priv = netdev_priv(ndev);
	struct net_device_stats *stats = &ndev->stats;
	u8 isr;

	while (1) {
		u8 unsent = readb(&priv->regs->tfcr);

		unsent = (unsent & RCAR_CAN_TFCR_TFUST) >>
			  RCAR_CAN_TFCR_TFUST_SHIFT;
		if (priv->tx_head - priv->tx_tail <= unsent)
			break;
		stats->tx_packets++;
		stats->tx_bytes +=
			can_get_echo_skb(ndev,
					 priv->tx_tail % RCAR_CAN_FIFO_DEPTH,
					 NULL);

		priv->tx_tail++;
		netif_wake_queue(ndev);
	}
	 
	isr = readb(&priv->regs->isr);
	writeb(isr & ~RCAR_CAN_ISR_TXFF, &priv->regs->isr);
}

static irqreturn_t rcar_can_interrupt(int irq, void *dev_id)
{
	struct net_device *ndev = dev_id;
	struct rcar_can_priv *priv = netdev_priv(ndev);
	u8 isr;

	isr = readb(&priv->regs->isr);
	if (!(isr & priv->ier))
		return IRQ_NONE;

	if (isr & RCAR_CAN_ISR_ERSF)
		rcar_can_error(ndev);

	if (isr & RCAR_CAN_ISR_TXFF)
		rcar_can_tx_done(ndev);

	if (isr & RCAR_CAN_ISR_RXFF) {
		if (napi_schedule_prep(&priv->napi)) {
			 
			priv->ier &= ~RCAR_CAN_IER_RXFIE;
			writeb(priv->ier, &priv->regs->ier);
			__napi_schedule(&priv->napi);
		}
	}

	return IRQ_HANDLED;
}

static void rcar_can_set_bittiming(struct net_device *dev)
{
	struct rcar_can_priv *priv = netdev_priv(dev);
	struct can_bittiming *bt = &priv->can.bittiming;
	u32 bcr;

	bcr = RCAR_CAN_BCR_TSEG1(bt->phase_seg1 + bt->prop_seg - 1) |
	      RCAR_CAN_BCR_BPR(bt->brp - 1) | RCAR_CAN_BCR_SJW(bt->sjw - 1) |
	      RCAR_CAN_BCR_TSEG2(bt->phase_seg2 - 1);
	 
	writel((bcr << 8) | priv->clock_select, &priv->regs->bcr);
}

static void rcar_can_start(struct net_device *ndev)
{
	struct rcar_can_priv *priv = netdev_priv(ndev);
	u16 ctlr;
	int i;

	 
	ctlr = readw(&priv->regs->ctlr);
	ctlr &= ~RCAR_CAN_CTLR_SLPM;
	writew(ctlr, &priv->regs->ctlr);
	 
	ctlr |= RCAR_CAN_CTLR_CANM_FORCE_RESET;
	writew(ctlr, &priv->regs->ctlr);
	for (i = 0; i < MAX_STR_READS; i++) {
		if (readw(&priv->regs->str) & RCAR_CAN_STR_RSTST)
			break;
	}
	rcar_can_set_bittiming(ndev);
	ctlr |= RCAR_CAN_CTLR_IDFM_MIXED;  
	ctlr |= RCAR_CAN_CTLR_BOM_ENT;	 
					 
	ctlr |= RCAR_CAN_CTLR_MBM;	 
	ctlr |= RCAR_CAN_CTLR_MLM;	 
	writew(ctlr, &priv->regs->ctlr);

	 
	writel(0, &priv->regs->mkr_2_9[6]);
	writel(0, &priv->regs->mkr_2_9[7]);
	 
	writel(0, &priv->regs->mkivlr1);
	 
	writel(0, &priv->regs->fidcr[0]);
	writel(RCAR_CAN_FIDCR_IDE | RCAR_CAN_FIDCR_RTR, &priv->regs->fidcr[1]);
	 
	writel(RCAR_CAN_MIER1_RXFIE | RCAR_CAN_MIER1_TXFIE, &priv->regs->mier1);

	priv->ier = RCAR_CAN_IER_ERSIE | RCAR_CAN_IER_RXFIE |
		    RCAR_CAN_IER_TXFIE;
	writeb(priv->ier, &priv->regs->ier);

	 
	writeb(RCAR_CAN_ECSR_EDPM, &priv->regs->ecsr);
	 
	writeb(RCAR_CAN_EIER_EWIE | RCAR_CAN_EIER_EPIE | RCAR_CAN_EIER_BOEIE |
	       (priv->can.ctrlmode & CAN_CTRLMODE_BERR_REPORTING ?
	       RCAR_CAN_EIER_BEIE : 0) | RCAR_CAN_EIER_ORIE |
	       RCAR_CAN_EIER_OLIE, &priv->regs->eier);
	priv->can.state = CAN_STATE_ERROR_ACTIVE;

	 
	writew(ctlr & ~RCAR_CAN_CTLR_CANM, &priv->regs->ctlr);
	for (i = 0; i < MAX_STR_READS; i++) {
		if (!(readw(&priv->regs->str) & RCAR_CAN_STR_RSTST))
			break;
	}
	 
	writeb(RCAR_CAN_RFCR_RFE, &priv->regs->rfcr);
	writeb(RCAR_CAN_TFCR_TFE, &priv->regs->tfcr);
}

static int rcar_can_open(struct net_device *ndev)
{
	struct rcar_can_priv *priv = netdev_priv(ndev);
	int err;

	err = clk_prepare_enable(priv->clk);
	if (err) {
		netdev_err(ndev,
			   "failed to enable peripheral clock, error %d\n",
			   err);
		goto out;
	}
	err = clk_prepare_enable(priv->can_clk);
	if (err) {
		netdev_err(ndev, "failed to enable CAN clock, error %d\n",
			   err);
		goto out_clock;
	}
	err = open_candev(ndev);
	if (err) {
		netdev_err(ndev, "open_candev() failed, error %d\n", err);
		goto out_can_clock;
	}
	napi_enable(&priv->napi);
	err = request_irq(ndev->irq, rcar_can_interrupt, 0, ndev->name, ndev);
	if (err) {
		netdev_err(ndev, "request_irq(%d) failed, error %d\n",
			   ndev->irq, err);
		goto out_close;
	}
	rcar_can_start(ndev);
	netif_start_queue(ndev);
	return 0;
out_close:
	napi_disable(&priv->napi);
	close_candev(ndev);
out_can_clock:
	clk_disable_unprepare(priv->can_clk);
out_clock:
	clk_disable_unprepare(priv->clk);
out:
	return err;
}

static void rcar_can_stop(struct net_device *ndev)
{
	struct rcar_can_priv *priv = netdev_priv(ndev);
	u16 ctlr;
	int i;

	 
	ctlr = readw(&priv->regs->ctlr);
	ctlr |= RCAR_CAN_CTLR_CANM_FORCE_RESET;
	writew(ctlr, &priv->regs->ctlr);
	for (i = 0; i < MAX_STR_READS; i++) {
		if (readw(&priv->regs->str) & RCAR_CAN_STR_RSTST)
			break;
	}
	writel(0, &priv->regs->mier0);
	writel(0, &priv->regs->mier1);
	writeb(0, &priv->regs->ier);
	writeb(0, &priv->regs->eier);
	 
	ctlr |= RCAR_CAN_CTLR_SLPM;
	writew(ctlr, &priv->regs->ctlr);
	priv->can.state = CAN_STATE_STOPPED;
}

static int rcar_can_close(struct net_device *ndev)
{
	struct rcar_can_priv *priv = netdev_priv(ndev);

	netif_stop_queue(ndev);
	rcar_can_stop(ndev);
	free_irq(ndev->irq, ndev);
	napi_disable(&priv->napi);
	clk_disable_unprepare(priv->can_clk);
	clk_disable_unprepare(priv->clk);
	close_candev(ndev);
	return 0;
}

static netdev_tx_t rcar_can_start_xmit(struct sk_buff *skb,
				       struct net_device *ndev)
{
	struct rcar_can_priv *priv = netdev_priv(ndev);
	struct can_frame *cf = (struct can_frame *)skb->data;
	u32 data, i;

	if (can_dev_dropped_skb(ndev, skb))
		return NETDEV_TX_OK;

	if (cf->can_id & CAN_EFF_FLAG)	 
		data = (cf->can_id & CAN_EFF_MASK) | RCAR_CAN_IDE;
	else				 
		data = (cf->can_id & CAN_SFF_MASK) << RCAR_CAN_SID_SHIFT;

	if (cf->can_id & CAN_RTR_FLAG) {  
		data |= RCAR_CAN_RTR;
	} else {
		for (i = 0; i < cf->len; i++)
			writeb(cf->data[i],
			       &priv->regs->mb[RCAR_CAN_TX_FIFO_MBX].data[i]);
	}

	writel(data, &priv->regs->mb[RCAR_CAN_TX_FIFO_MBX].id);

	writeb(cf->len, &priv->regs->mb[RCAR_CAN_TX_FIFO_MBX].dlc);

	can_put_echo_skb(skb, ndev, priv->tx_head % RCAR_CAN_FIFO_DEPTH, 0);
	priv->tx_head++;
	 
	writeb(0xff, &priv->regs->tfpcr);
	 
	if (priv->tx_head - priv->tx_tail >= RCAR_CAN_FIFO_DEPTH)
		netif_stop_queue(ndev);

	return NETDEV_TX_OK;
}

static const struct net_device_ops rcar_can_netdev_ops = {
	.ndo_open = rcar_can_open,
	.ndo_stop = rcar_can_close,
	.ndo_start_xmit = rcar_can_start_xmit,
	.ndo_change_mtu = can_change_mtu,
};

static const struct ethtool_ops rcar_can_ethtool_ops = {
	.get_ts_info = ethtool_op_get_ts_info,
};

static void rcar_can_rx_pkt(struct rcar_can_priv *priv)
{
	struct net_device_stats *stats = &priv->ndev->stats;
	struct can_frame *cf;
	struct sk_buff *skb;
	u32 data;
	u8 dlc;

	skb = alloc_can_skb(priv->ndev, &cf);
	if (!skb) {
		stats->rx_dropped++;
		return;
	}

	data = readl(&priv->regs->mb[RCAR_CAN_RX_FIFO_MBX].id);
	if (data & RCAR_CAN_IDE)
		cf->can_id = (data & CAN_EFF_MASK) | CAN_EFF_FLAG;
	else
		cf->can_id = (data >> RCAR_CAN_SID_SHIFT) & CAN_SFF_MASK;

	dlc = readb(&priv->regs->mb[RCAR_CAN_RX_FIFO_MBX].dlc);
	cf->len = can_cc_dlc2len(dlc);
	if (data & RCAR_CAN_RTR) {
		cf->can_id |= CAN_RTR_FLAG;
	} else {
		for (dlc = 0; dlc < cf->len; dlc++)
			cf->data[dlc] =
			readb(&priv->regs->mb[RCAR_CAN_RX_FIFO_MBX].data[dlc]);

		stats->rx_bytes += cf->len;
	}
	stats->rx_packets++;

	netif_receive_skb(skb);
}

static int rcar_can_rx_poll(struct napi_struct *napi, int quota)
{
	struct rcar_can_priv *priv = container_of(napi,
						  struct rcar_can_priv, napi);
	int num_pkts;

	for (num_pkts = 0; num_pkts < quota; num_pkts++) {
		u8 rfcr, isr;

		isr = readb(&priv->regs->isr);
		 
		if (isr & RCAR_CAN_ISR_RXFF)
			writeb(isr & ~RCAR_CAN_ISR_RXFF, &priv->regs->isr);
		rfcr = readb(&priv->regs->rfcr);
		if (rfcr & RCAR_CAN_RFCR_RFEST)
			break;
		rcar_can_rx_pkt(priv);
		 
		writeb(0xff, &priv->regs->rfpcr);
	}
	 
	if (num_pkts < quota) {
		napi_complete_done(napi, num_pkts);
		priv->ier |= RCAR_CAN_IER_RXFIE;
		writeb(priv->ier, &priv->regs->ier);
	}
	return num_pkts;
}

static int rcar_can_do_set_mode(struct net_device *ndev, enum can_mode mode)
{
	switch (mode) {
	case CAN_MODE_START:
		rcar_can_start(ndev);
		netif_wake_queue(ndev);
		return 0;
	default:
		return -EOPNOTSUPP;
	}
}

static int rcar_can_get_berr_counter(const struct net_device *dev,
				     struct can_berr_counter *bec)
{
	struct rcar_can_priv *priv = netdev_priv(dev);
	int err;

	err = clk_prepare_enable(priv->clk);
	if (err)
		return err;
	bec->txerr = readb(&priv->regs->tecr);
	bec->rxerr = readb(&priv->regs->recr);
	clk_disable_unprepare(priv->clk);
	return 0;
}

static const char * const clock_names[] = {
	[CLKR_CLKP1]	= "clkp1",
	[CLKR_CLKP2]	= "clkp2",
	[CLKR_CLKEXT]	= "can_clk",
};

static int rcar_can_probe(struct platform_device *pdev)
{
	struct rcar_can_priv *priv;
	struct net_device *ndev;
	void __iomem *addr;
	u32 clock_select = CLKR_CLKP1;
	int err = -ENODEV;
	int irq;

	of_property_read_u32(pdev->dev.of_node, "renesas,can-clock-select",
			     &clock_select);

	irq = platform_get_irq(pdev, 0);
	if (irq < 0) {
		err = irq;
		goto fail;
	}

	addr = devm_platform_ioremap_resource(pdev, 0);
	if (IS_ERR(addr)) {
		err = PTR_ERR(addr);
		goto fail;
	}

	ndev = alloc_candev(sizeof(struct rcar_can_priv), RCAR_CAN_FIFO_DEPTH);
	if (!ndev) {
		dev_err(&pdev->dev, "alloc_candev() failed\n");
		err = -ENOMEM;
		goto fail;
	}

	priv = netdev_priv(ndev);

	priv->clk = devm_clk_get(&pdev->dev, "clkp1");
	if (IS_ERR(priv->clk)) {
		err = PTR_ERR(priv->clk);
		dev_err(&pdev->dev, "cannot get peripheral clock, error %d\n",
			err);
		goto fail_clk;
	}

	if (!(BIT(clock_select) & RCAR_SUPPORTED_CLOCKS)) {
		err = -EINVAL;
		dev_err(&pdev->dev, "invalid CAN clock selected\n");
		goto fail_clk;
	}
	priv->can_clk = devm_clk_get(&pdev->dev, clock_names[clock_select]);
	if (IS_ERR(priv->can_clk)) {
		err = PTR_ERR(priv->can_clk);
		dev_err(&pdev->dev, "cannot get CAN clock, error %d\n", err);
		goto fail_clk;
	}

	ndev->netdev_ops = &rcar_can_netdev_ops;
	ndev->ethtool_ops = &rcar_can_ethtool_ops;
	ndev->irq = irq;
	ndev->flags |= IFF_ECHO;
	priv->ndev = ndev;
	priv->regs = addr;
	priv->clock_select = clock_select;
	priv->can.clock.freq = clk_get_rate(priv->can_clk);
	priv->can.bittiming_const = &rcar_can_bittiming_const;
	priv->can.do_set_mode = rcar_can_do_set_mode;
	priv->can.do_get_berr_counter = rcar_can_get_berr_counter;
	priv->can.ctrlmode_supported = CAN_CTRLMODE_BERR_REPORTING;
	platform_set_drvdata(pdev, ndev);
	SET_NETDEV_DEV(ndev, &pdev->dev);

	netif_napi_add_weight(ndev, &priv->napi, rcar_can_rx_poll,
			      RCAR_CAN_NAPI_WEIGHT);
	err = register_candev(ndev);
	if (err) {
		dev_err(&pdev->dev, "register_candev() failed, error %d\n",
			err);
		goto fail_candev;
	}

	dev_info(&pdev->dev, "device registered (IRQ%d)\n", ndev->irq);

	return 0;
fail_candev:
	netif_napi_del(&priv->napi);
fail_clk:
	free_candev(ndev);
fail:
	return err;
}

static void rcar_can_remove(struct platform_device *pdev)
{
	struct net_device *ndev = platform_get_drvdata(pdev);
	struct rcar_can_priv *priv = netdev_priv(ndev);

	unregister_candev(ndev);
	netif_napi_del(&priv->napi);
	free_candev(ndev);
}

static int __maybe_unused rcar_can_suspend(struct device *dev)
{
	struct net_device *ndev = dev_get_drvdata(dev);
	struct rcar_can_priv *priv = netdev_priv(ndev);
	u16 ctlr;

	if (!netif_running(ndev))
		return 0;

	netif_stop_queue(ndev);
	netif_device_detach(ndev);

	ctlr = readw(&priv->regs->ctlr);
	ctlr |= RCAR_CAN_CTLR_CANM_HALT;
	writew(ctlr, &priv->regs->ctlr);
	ctlr |= RCAR_CAN_CTLR_SLPM;
	writew(ctlr, &priv->regs->ctlr);
	priv->can.state = CAN_STATE_SLEEPING;

	clk_disable(priv->clk);
	return 0;
}

static int __maybe_unused rcar_can_resume(struct device *dev)
{
	struct net_device *ndev = dev_get_drvdata(dev);
	struct rcar_can_priv *priv = netdev_priv(ndev);
	u16 ctlr;
	int err;

	if (!netif_running(ndev))
		return 0;

	err = clk_enable(priv->clk);
	if (err) {
		netdev_err(ndev, "clk_enable() failed, error %d\n", err);
		return err;
	}

	ctlr = readw(&priv->regs->ctlr);
	ctlr &= ~RCAR_CAN_CTLR_SLPM;
	writew(ctlr, &priv->regs->ctlr);
	ctlr &= ~RCAR_CAN_CTLR_CANM;
	writew(ctlr, &priv->regs->ctlr);
	priv->can.state = CAN_STATE_ERROR_ACTIVE;

	netif_device_attach(ndev);
	netif_start_queue(ndev);

	return 0;
}

static SIMPLE_DEV_PM_OPS(rcar_can_pm_ops, rcar_can_suspend, rcar_can_resume);

static const struct of_device_id rcar_can_of_table[] __maybe_unused = {
	{ .compatible = "renesas,can-r8a7778" },
	{ .compatible = "renesas,can-r8a7779" },
	{ .compatible = "renesas,can-r8a7790" },
	{ .compatible = "renesas,can-r8a7791" },
	{ .compatible = "renesas,rcar-gen1-can" },
	{ .compatible = "renesas,rcar-gen2-can" },
	{ .compatible = "renesas,rcar-gen3-can" },
	{ }
};
MODULE_DEVICE_TABLE(of, rcar_can_of_table);

static struct platform_driver rcar_can_driver = {
	.driver = {
		.name = RCAR_CAN_DRV_NAME,
		.of_match_table = of_match_ptr(rcar_can_of_table),
		.pm = &rcar_can_pm_ops,
	},
	.probe = rcar_can_probe,
	.remove_new = rcar_can_remove,
};

module_platform_driver(rcar_can_driver);

MODULE_AUTHOR("Cogent Embedded, Inc.");
MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("CAN driver for Renesas R-Car SoC");
MODULE_ALIAS("platform:" RCAR_CAN_DRV_NAME);
