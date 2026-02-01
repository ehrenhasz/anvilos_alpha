
 

#include <linux/clk.h>
#include <linux/errno.h>
#include <linux/ethtool.h>
#include <linux/init.h>
#include <linux/bitfield.h>
#include <linux/interrupt.h>
#include <linux/io.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/skbuff.h>
#include <linux/string.h>
#include <linux/types.h>
#include <linux/can/error.h>
#include <linux/pm_runtime.h>

#include "ctucanfd.h"
#include "ctucanfd_kregs.h"
#include "ctucanfd_kframe.h"

#ifdef DEBUG
#define  ctucan_netdev_dbg(ndev, args...) \
		netdev_dbg(ndev, args)
#else
#define ctucan_netdev_dbg(...) do { } while (0)
#endif

#define CTUCANFD_ID 0xCAFD

 

#define CTUCANFD_FLAG_RX_FFW_BUFFERED	1

#define CTUCAN_STATE_TO_TEXT_ENTRY(st) \
		[st] = #st

enum ctucan_txtb_status {
	TXT_NOT_EXIST       = 0x0,
	TXT_RDY             = 0x1,
	TXT_TRAN            = 0x2,
	TXT_ABTP            = 0x3,
	TXT_TOK             = 0x4,
	TXT_ERR             = 0x6,
	TXT_ABT             = 0x7,
	TXT_ETY             = 0x8,
};

enum ctucan_txtb_command {
	TXT_CMD_SET_EMPTY   = 0x01,
	TXT_CMD_SET_READY   = 0x02,
	TXT_CMD_SET_ABORT   = 0x04
};

static const struct can_bittiming_const ctu_can_fd_bit_timing_max = {
	.name = "ctu_can_fd",
	.tseg1_min = 2,
	.tseg1_max = 190,
	.tseg2_min = 1,
	.tseg2_max = 63,
	.sjw_max = 31,
	.brp_min = 1,
	.brp_max = 8,
	.brp_inc = 1,
};

static const struct can_bittiming_const ctu_can_fd_bit_timing_data_max = {
	.name = "ctu_can_fd",
	.tseg1_min = 2,
	.tseg1_max = 94,
	.tseg2_min = 1,
	.tseg2_max = 31,
	.sjw_max = 31,
	.brp_min = 1,
	.brp_max = 2,
	.brp_inc = 1,
};

static const char * const ctucan_state_strings[CAN_STATE_MAX] = {
	CTUCAN_STATE_TO_TEXT_ENTRY(CAN_STATE_ERROR_ACTIVE),
	CTUCAN_STATE_TO_TEXT_ENTRY(CAN_STATE_ERROR_WARNING),
	CTUCAN_STATE_TO_TEXT_ENTRY(CAN_STATE_ERROR_PASSIVE),
	CTUCAN_STATE_TO_TEXT_ENTRY(CAN_STATE_BUS_OFF),
	CTUCAN_STATE_TO_TEXT_ENTRY(CAN_STATE_STOPPED),
	CTUCAN_STATE_TO_TEXT_ENTRY(CAN_STATE_SLEEPING)
};

static void ctucan_write32_le(struct ctucan_priv *priv,
			      enum ctu_can_fd_can_registers reg, u32 val)
{
	iowrite32(val, priv->mem_base + reg);
}

static void ctucan_write32_be(struct ctucan_priv *priv,
			      enum ctu_can_fd_can_registers reg, u32 val)
{
	iowrite32be(val, priv->mem_base + reg);
}

static u32 ctucan_read32_le(struct ctucan_priv *priv,
			    enum ctu_can_fd_can_registers reg)
{
	return ioread32(priv->mem_base + reg);
}

static u32 ctucan_read32_be(struct ctucan_priv *priv,
			    enum ctu_can_fd_can_registers reg)
{
	return ioread32be(priv->mem_base + reg);
}

static void ctucan_write32(struct ctucan_priv *priv, enum ctu_can_fd_can_registers reg, u32 val)
{
	priv->write_reg(priv, reg, val);
}

static u32 ctucan_read32(struct ctucan_priv *priv, enum ctu_can_fd_can_registers reg)
{
	return priv->read_reg(priv, reg);
}

static void ctucan_write_txt_buf(struct ctucan_priv *priv, enum ctu_can_fd_can_registers buf_base,
				 u32 offset, u32 val)
{
	priv->write_reg(priv, buf_base + offset, val);
}

#define CTU_CAN_FD_TXTNF(priv) (!!FIELD_GET(REG_STATUS_TXNF, ctucan_read32(priv, CTUCANFD_STATUS)))
#define CTU_CAN_FD_ENABLED(priv) (!!FIELD_GET(REG_MODE_ENA, ctucan_read32(priv, CTUCANFD_MODE)))

 
static const char *ctucan_state_to_str(enum can_state state)
{
	const char *txt = NULL;

	if (state >= 0 && state < CAN_STATE_MAX)
		txt = ctucan_state_strings[state];
	return txt ? txt : "UNKNOWN";
}

 
static int ctucan_reset(struct net_device *ndev)
{
	struct ctucan_priv *priv = netdev_priv(ndev);
	int i = 100;

	ctucan_write32(priv, CTUCANFD_MODE, REG_MODE_RST);
	clear_bit(CTUCANFD_FLAG_RX_FFW_BUFFERED, &priv->drv_flags);

	do {
		u16 device_id = FIELD_GET(REG_DEVICE_ID_DEVICE_ID,
					  ctucan_read32(priv, CTUCANFD_DEVICE_ID));

		if (device_id == 0xCAFD)
			return 0;
		if (!i--) {
			netdev_warn(ndev, "device did not leave reset\n");
			return -ETIMEDOUT;
		}
		usleep_range(100, 200);
	} while (1);
}

 
static int ctucan_set_btr(struct net_device *ndev, struct can_bittiming *bt, bool nominal)
{
	struct ctucan_priv *priv = netdev_priv(ndev);
	int max_ph1_len = 31;
	u32 btr = 0;
	u32 prop_seg = bt->prop_seg;
	u32 phase_seg1 = bt->phase_seg1;

	if (CTU_CAN_FD_ENABLED(priv)) {
		netdev_err(ndev, "BUG! Cannot set bittiming - CAN is enabled\n");
		return -EPERM;
	}

	if (nominal)
		max_ph1_len = 63;

	 
	if (phase_seg1 > max_ph1_len) {
		prop_seg += phase_seg1 - max_ph1_len;
		phase_seg1 = max_ph1_len;
		bt->prop_seg = prop_seg;
		bt->phase_seg1 = phase_seg1;
	}

	if (nominal) {
		btr = FIELD_PREP(REG_BTR_PROP, prop_seg);
		btr |= FIELD_PREP(REG_BTR_PH1, phase_seg1);
		btr |= FIELD_PREP(REG_BTR_PH2, bt->phase_seg2);
		btr |= FIELD_PREP(REG_BTR_BRP, bt->brp);
		btr |= FIELD_PREP(REG_BTR_SJW, bt->sjw);

		ctucan_write32(priv, CTUCANFD_BTR, btr);
	} else {
		btr = FIELD_PREP(REG_BTR_FD_PROP_FD, prop_seg);
		btr |= FIELD_PREP(REG_BTR_FD_PH1_FD, phase_seg1);
		btr |= FIELD_PREP(REG_BTR_FD_PH2_FD, bt->phase_seg2);
		btr |= FIELD_PREP(REG_BTR_FD_BRP_FD, bt->brp);
		btr |= FIELD_PREP(REG_BTR_FD_SJW_FD, bt->sjw);

		ctucan_write32(priv, CTUCANFD_BTR_FD, btr);
	}

	return 0;
}

 
static int ctucan_set_bittiming(struct net_device *ndev)
{
	struct ctucan_priv *priv = netdev_priv(ndev);
	struct can_bittiming *bt = &priv->can.bittiming;

	 
	return ctucan_set_btr(ndev, bt, true);
}

 
static int ctucan_set_data_bittiming(struct net_device *ndev)
{
	struct ctucan_priv *priv = netdev_priv(ndev);
	struct can_bittiming *dbt = &priv->can.data_bittiming;

	 
	return ctucan_set_btr(ndev, dbt, false);
}

 
static int ctucan_set_secondary_sample_point(struct net_device *ndev)
{
	struct ctucan_priv *priv = netdev_priv(ndev);
	struct can_bittiming *dbt = &priv->can.data_bittiming;
	int ssp_offset = 0;
	u32 ssp_cfg = 0;  

	if (CTU_CAN_FD_ENABLED(priv)) {
		netdev_err(ndev, "BUG! Cannot set SSP - CAN is enabled\n");
		return -EPERM;
	}

	 
	if (dbt->bitrate > 1000000) {
		 
		ssp_offset = (priv->can.clock.freq / 1000) * dbt->sample_point / dbt->bitrate;

		if (ssp_offset > 127) {
			netdev_warn(ndev, "SSP offset saturated to 127\n");
			ssp_offset = 127;
		}

		ssp_cfg = FIELD_PREP(REG_TRV_DELAY_SSP_OFFSET, ssp_offset);
		ssp_cfg |= FIELD_PREP(REG_TRV_DELAY_SSP_SRC, 0x1);
	}

	ctucan_write32(priv, CTUCANFD_TRV_DELAY, ssp_cfg);

	return 0;
}

 
static void ctucan_set_mode(struct ctucan_priv *priv, const struct can_ctrlmode *mode)
{
	u32 mode_reg = ctucan_read32(priv, CTUCANFD_MODE);

	mode_reg = (mode->flags & CAN_CTRLMODE_LOOPBACK) ?
			(mode_reg | REG_MODE_ILBP) :
			(mode_reg & ~REG_MODE_ILBP);

	mode_reg = (mode->flags & CAN_CTRLMODE_LISTENONLY) ?
			(mode_reg | REG_MODE_BMM) :
			(mode_reg & ~REG_MODE_BMM);

	mode_reg = (mode->flags & CAN_CTRLMODE_FD) ?
			(mode_reg | REG_MODE_FDE) :
			(mode_reg & ~REG_MODE_FDE);

	mode_reg = (mode->flags & CAN_CTRLMODE_PRESUME_ACK) ?
			(mode_reg | REG_MODE_ACF) :
			(mode_reg & ~REG_MODE_ACF);

	mode_reg = (mode->flags & CAN_CTRLMODE_FD_NON_ISO) ?
			(mode_reg | REG_MODE_NISOFD) :
			(mode_reg & ~REG_MODE_NISOFD);

	 
	mode_reg &= ~FIELD_PREP(REG_MODE_RTRTH, 0xF);
	mode_reg = (mode->flags & CAN_CTRLMODE_ONE_SHOT) ?
			(mode_reg | REG_MODE_RTRLE) :
			(mode_reg & ~REG_MODE_RTRLE);

	 
	mode_reg &= ~REG_MODE_TSTM;

	ctucan_write32(priv, CTUCANFD_MODE, mode_reg);
}

 
static int ctucan_chip_start(struct net_device *ndev)
{
	struct ctucan_priv *priv = netdev_priv(ndev);
	u32 int_ena, int_msk;
	u32 mode_reg;
	int err;
	struct can_ctrlmode mode;

	priv->txb_prio = 0x01234567;
	priv->txb_head = 0;
	priv->txb_tail = 0;
	ctucan_write32(priv, CTUCANFD_TX_PRIORITY, priv->txb_prio);

	 
	err = ctucan_set_bittiming(ndev);
	if (err < 0)
		return err;

	err = ctucan_set_data_bittiming(ndev);
	if (err < 0)
		return err;

	err = ctucan_set_secondary_sample_point(ndev);
	if (err < 0)
		return err;

	 
	mode.flags = priv->can.ctrlmode;
	mode.mask = 0xFFFFFFFF;
	ctucan_set_mode(priv, &mode);

	 
	int_ena = REG_INT_STAT_RBNEI |
		  REG_INT_STAT_TXBHCI |
		  REG_INT_STAT_EWLI |
		  REG_INT_STAT_FCSI;

	 
	if (priv->can.ctrlmode & CAN_CTRLMODE_BERR_REPORTING) {
		int_ena |= REG_INT_STAT_ALI |
			   REG_INT_STAT_BEI;
	}

	int_msk = ~int_ena;  

	 
	ctucan_write32(priv, CTUCANFD_INT_MASK_SET, int_msk);
	ctucan_write32(priv, CTUCANFD_INT_ENA_SET, int_ena);

	 
	priv->can.state = CAN_STATE_STOPPED;

	 
	mode_reg = ctucan_read32(priv, CTUCANFD_MODE);
	mode_reg |= REG_MODE_ENA;
	ctucan_write32(priv, CTUCANFD_MODE, mode_reg);

	return 0;
}

 
static int ctucan_do_set_mode(struct net_device *ndev, enum can_mode mode)
{
	int ret;

	switch (mode) {
	case CAN_MODE_START:
		ret = ctucan_reset(ndev);
		if (ret < 0)
			return ret;
		ret = ctucan_chip_start(ndev);
		if (ret < 0) {
			netdev_err(ndev, "ctucan_chip_start failed!\n");
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

 
static enum ctucan_txtb_status ctucan_get_tx_status(struct ctucan_priv *priv, u8 buf)
{
	u32 tx_status = ctucan_read32(priv, CTUCANFD_TX_STATUS);
	enum ctucan_txtb_status status = (tx_status >> (buf * 4)) & 0x7;

	return status;
}

 
static bool ctucan_is_txt_buf_writable(struct ctucan_priv *priv, u8 buf)
{
	enum ctucan_txtb_status buf_status;

	buf_status = ctucan_get_tx_status(priv, buf);
	if (buf_status == TXT_RDY || buf_status == TXT_TRAN || buf_status == TXT_ABTP)
		return false;

	return true;
}

 
static bool ctucan_insert_frame(struct ctucan_priv *priv, const struct canfd_frame *cf, u8 buf,
				bool isfdf)
{
	u32 buf_base;
	u32 ffw = 0;
	u32 idw = 0;
	unsigned int i;

	if (buf >= priv->ntxbufs)
		return false;

	if (!ctucan_is_txt_buf_writable(priv, buf))
		return false;

	if (cf->len > CANFD_MAX_DLEN)
		return false;

	 
	if (cf->can_id & CAN_RTR_FLAG)
		ffw |= REG_FRAME_FORMAT_W_RTR;

	if (cf->can_id & CAN_EFF_FLAG)
		ffw |= REG_FRAME_FORMAT_W_IDE;

	if (isfdf) {
		ffw |= REG_FRAME_FORMAT_W_FDF;
		if (cf->flags & CANFD_BRS)
			ffw |= REG_FRAME_FORMAT_W_BRS;
	}

	ffw |= FIELD_PREP(REG_FRAME_FORMAT_W_DLC, can_fd_len2dlc(cf->len));

	 
	if (cf->can_id & CAN_EFF_FLAG)
		idw = cf->can_id & CAN_EFF_MASK;
	else
		idw = FIELD_PREP(REG_IDENTIFIER_W_IDENTIFIER_BASE, cf->can_id & CAN_SFF_MASK);

	 
	buf_base = (buf + 1) * 0x100;
	ctucan_write_txt_buf(priv, buf_base, CTUCANFD_FRAME_FORMAT_W, ffw);
	ctucan_write_txt_buf(priv, buf_base, CTUCANFD_IDENTIFIER_W, idw);

	 
	if (!(cf->can_id & CAN_RTR_FLAG)) {
		for (i = 0; i < cf->len; i += 4) {
			u32 data = le32_to_cpu(*(__le32 *)(cf->data + i));

			ctucan_write_txt_buf(priv, buf_base, CTUCANFD_DATA_1_4_W + i, data);
		}
	}

	return true;
}

 
static void ctucan_give_txtb_cmd(struct ctucan_priv *priv, enum ctucan_txtb_command cmd, u8 buf)
{
	u32 tx_cmd = cmd;

	tx_cmd |= 1 << (buf + 8);
	ctucan_write32(priv, CTUCANFD_TX_COMMAND, tx_cmd);
}

 
static netdev_tx_t ctucan_start_xmit(struct sk_buff *skb, struct net_device *ndev)
{
	struct ctucan_priv *priv = netdev_priv(ndev);
	struct canfd_frame *cf = (struct canfd_frame *)skb->data;
	u32 txtb_id;
	bool ok;
	unsigned long flags;

	if (can_dev_dropped_skb(ndev, skb))
		return NETDEV_TX_OK;

	if (unlikely(!CTU_CAN_FD_TXTNF(priv))) {
		netif_stop_queue(ndev);
		netdev_err(ndev, "BUG!, no TXB free when queue awake!\n");
		return NETDEV_TX_BUSY;
	}

	txtb_id = priv->txb_head % priv->ntxbufs;
	ctucan_netdev_dbg(ndev, "%s: using TXB#%u\n", __func__, txtb_id);
	ok = ctucan_insert_frame(priv, cf, txtb_id, can_is_canfd_skb(skb));

	if (!ok) {
		netdev_err(ndev, "BUG! TXNF set but cannot insert frame into TXTB! HW Bug?");
		kfree_skb(skb);
		ndev->stats.tx_dropped++;
		return NETDEV_TX_OK;
	}

	can_put_echo_skb(skb, ndev, txtb_id, 0);

	spin_lock_irqsave(&priv->tx_lock, flags);
	ctucan_give_txtb_cmd(priv, TXT_CMD_SET_READY, txtb_id);
	priv->txb_head++;

	 
	if (!CTU_CAN_FD_TXTNF(priv))
		netif_stop_queue(ndev);

	spin_unlock_irqrestore(&priv->tx_lock, flags);

	return NETDEV_TX_OK;
}

 
static void ctucan_read_rx_frame(struct ctucan_priv *priv, struct canfd_frame *cf, u32 ffw)
{
	u32 idw;
	unsigned int i;
	unsigned int wc;
	unsigned int len;

	idw = ctucan_read32(priv, CTUCANFD_RX_DATA);
	if (FIELD_GET(REG_FRAME_FORMAT_W_IDE, ffw))
		cf->can_id = (idw & CAN_EFF_MASK) | CAN_EFF_FLAG;
	else
		cf->can_id = (idw >> 18) & CAN_SFF_MASK;

	 
	if (FIELD_GET(REG_FRAME_FORMAT_W_FDF, ffw)) {
		if (FIELD_GET(REG_FRAME_FORMAT_W_BRS, ffw))
			cf->flags |= CANFD_BRS;
		if (FIELD_GET(REG_FRAME_FORMAT_W_ESI_RSV, ffw))
			cf->flags |= CANFD_ESI;
	} else if (FIELD_GET(REG_FRAME_FORMAT_W_RTR, ffw)) {
		cf->can_id |= CAN_RTR_FLAG;
	}

	wc = FIELD_GET(REG_FRAME_FORMAT_W_RWCNT, ffw) - 3;

	 
	if (FIELD_GET(REG_FRAME_FORMAT_W_DLC, ffw) <= 8) {
		len = FIELD_GET(REG_FRAME_FORMAT_W_DLC, ffw);
	} else {
		if (FIELD_GET(REG_FRAME_FORMAT_W_FDF, ffw))
			len = wc << 2;
		else
			len = 8;
	}
	cf->len = len;
	if (unlikely(len > wc * 4))
		len = wc * 4;

	 
	ctucan_read32(priv, CTUCANFD_RX_DATA);
	ctucan_read32(priv, CTUCANFD_RX_DATA);

	 
	for (i = 0; i < len; i += 4) {
		u32 data = ctucan_read32(priv, CTUCANFD_RX_DATA);
		*(__le32 *)(cf->data + i) = cpu_to_le32(data);
	}
	while (unlikely(i < wc * 4)) {
		ctucan_read32(priv, CTUCANFD_RX_DATA);
		i += 4;
	}
}

 
static int ctucan_rx(struct net_device *ndev)
{
	struct ctucan_priv *priv = netdev_priv(ndev);
	struct net_device_stats *stats = &ndev->stats;
	struct canfd_frame *cf;
	struct sk_buff *skb;
	u32 ffw;

	if (test_bit(CTUCANFD_FLAG_RX_FFW_BUFFERED, &priv->drv_flags)) {
		ffw = priv->rxfrm_first_word;
		clear_bit(CTUCANFD_FLAG_RX_FFW_BUFFERED, &priv->drv_flags);
	} else {
		ffw = ctucan_read32(priv, CTUCANFD_RX_DATA);
	}

	if (!FIELD_GET(REG_FRAME_FORMAT_W_RWCNT, ffw))
		return -EAGAIN;

	if (FIELD_GET(REG_FRAME_FORMAT_W_FDF, ffw))
		skb = alloc_canfd_skb(ndev, &cf);
	else
		skb = alloc_can_skb(ndev, (struct can_frame **)&cf);

	if (unlikely(!skb)) {
		priv->rxfrm_first_word = ffw;
		set_bit(CTUCANFD_FLAG_RX_FFW_BUFFERED, &priv->drv_flags);
		return 0;
	}

	ctucan_read_rx_frame(priv, cf, ffw);

	stats->rx_bytes += cf->len;
	stats->rx_packets++;
	netif_receive_skb(skb);

	return 1;
}

 
static enum can_state ctucan_read_fault_state(struct ctucan_priv *priv)
{
	u32 fs;
	u32 rec_tec;
	u32 ewl;

	fs = ctucan_read32(priv, CTUCANFD_EWL);
	rec_tec = ctucan_read32(priv, CTUCANFD_REC);
	ewl = FIELD_GET(REG_EWL_EW_LIMIT, fs);

	if (FIELD_GET(REG_EWL_ERA, fs)) {
		if (ewl > FIELD_GET(REG_REC_REC_VAL, rec_tec) &&
		    ewl > FIELD_GET(REG_REC_TEC_VAL, rec_tec))
			return CAN_STATE_ERROR_ACTIVE;
		else
			return CAN_STATE_ERROR_WARNING;
	} else if (FIELD_GET(REG_EWL_ERP, fs)) {
		return CAN_STATE_ERROR_PASSIVE;
	} else if (FIELD_GET(REG_EWL_BOF, fs)) {
		return CAN_STATE_BUS_OFF;
	}

	WARN(true, "Invalid error state");
	return CAN_STATE_ERROR_PASSIVE;
}

 
static void ctucan_get_rec_tec(struct ctucan_priv *priv, struct can_berr_counter *bec)
{
	u32 err_ctrs = ctucan_read32(priv, CTUCANFD_REC);

	bec->rxerr = FIELD_GET(REG_REC_REC_VAL, err_ctrs);
	bec->txerr = FIELD_GET(REG_REC_TEC_VAL, err_ctrs);
}

 
static void ctucan_err_interrupt(struct net_device *ndev, u32 isr)
{
	struct ctucan_priv *priv = netdev_priv(ndev);
	struct net_device_stats *stats = &ndev->stats;
	struct can_frame *cf;
	struct sk_buff *skb;
	enum can_state state;
	struct can_berr_counter bec;
	u32 err_capt_alc;
	int dologerr = net_ratelimit();

	ctucan_get_rec_tec(priv, &bec);
	state = ctucan_read_fault_state(priv);
	err_capt_alc = ctucan_read32(priv, CTUCANFD_ERR_CAPT);

	if (dologerr)
		netdev_info(ndev, "%s: ISR = 0x%08x, rxerr %d, txerr %d, error type %lu, pos %lu, ALC id_field %lu, bit %lu\n",
			    __func__, isr, bec.rxerr, bec.txerr,
			    FIELD_GET(REG_ERR_CAPT_ERR_TYPE, err_capt_alc),
			    FIELD_GET(REG_ERR_CAPT_ERR_POS, err_capt_alc),
			    FIELD_GET(REG_ERR_CAPT_ALC_ID_FIELD, err_capt_alc),
			    FIELD_GET(REG_ERR_CAPT_ALC_BIT, err_capt_alc));

	skb = alloc_can_err_skb(ndev, &cf);

	 
	if (FIELD_GET(REG_INT_STAT_FCSI, isr) || FIELD_GET(REG_INT_STAT_EWLI, isr)) {
		netdev_info(ndev, "state changes from %s to %s\n",
			    ctucan_state_to_str(priv->can.state),
			    ctucan_state_to_str(state));

		if (priv->can.state == state)
			netdev_warn(ndev,
				    "current and previous state is the same! (missed interrupt?)\n");

		priv->can.state = state;
		switch (state) {
		case CAN_STATE_BUS_OFF:
			priv->can.can_stats.bus_off++;
			can_bus_off(ndev);
			if (skb)
				cf->can_id |= CAN_ERR_BUSOFF;
			break;
		case CAN_STATE_ERROR_PASSIVE:
			priv->can.can_stats.error_passive++;
			if (skb) {
				cf->can_id |= CAN_ERR_CRTL | CAN_ERR_CNT;
				cf->data[1] = (bec.rxerr > 127) ?
						CAN_ERR_CRTL_RX_PASSIVE :
						CAN_ERR_CRTL_TX_PASSIVE;
				cf->data[6] = bec.txerr;
				cf->data[7] = bec.rxerr;
			}
			break;
		case CAN_STATE_ERROR_WARNING:
			priv->can.can_stats.error_warning++;
			if (skb) {
				cf->can_id |= CAN_ERR_CRTL | CAN_ERR_CNT;
				cf->data[1] |= (bec.txerr > bec.rxerr) ?
					CAN_ERR_CRTL_TX_WARNING :
					CAN_ERR_CRTL_RX_WARNING;
				cf->data[6] = bec.txerr;
				cf->data[7] = bec.rxerr;
			}
			break;
		case CAN_STATE_ERROR_ACTIVE:
			cf->can_id |= CAN_ERR_CNT;
			cf->data[1] = CAN_ERR_CRTL_ACTIVE;
			cf->data[6] = bec.txerr;
			cf->data[7] = bec.rxerr;
			break;
		default:
			netdev_warn(ndev, "unhandled error state (%d:%s)!\n",
				    state, ctucan_state_to_str(state));
			break;
		}
	}

	 
	if (FIELD_GET(REG_INT_STAT_ALI, isr)) {
		if (dologerr)
			netdev_info(ndev, "arbitration lost\n");
		priv->can.can_stats.arbitration_lost++;
		if (skb) {
			cf->can_id |= CAN_ERR_LOSTARB;
			cf->data[0] = CAN_ERR_LOSTARB_UNSPEC;
		}
	}

	 
	if (FIELD_GET(REG_INT_STAT_BEI, isr)) {
		netdev_info(ndev, "bus error\n");
		priv->can.can_stats.bus_error++;
		stats->rx_errors++;
		if (skb) {
			cf->can_id |= CAN_ERR_PROT | CAN_ERR_BUSERROR;
			cf->data[2] = CAN_ERR_PROT_UNSPEC;
			cf->data[3] = CAN_ERR_PROT_LOC_UNSPEC;
		}
	}

	if (skb) {
		stats->rx_packets++;
		stats->rx_bytes += cf->can_dlc;
		netif_rx(skb);
	}
}

 
static int ctucan_rx_poll(struct napi_struct *napi, int quota)
{
	struct net_device *ndev = napi->dev;
	struct ctucan_priv *priv = netdev_priv(ndev);
	int work_done = 0;
	u32 status;
	u32 framecnt;
	int res = 1;

	framecnt = FIELD_GET(REG_RX_STATUS_RXFRC, ctucan_read32(priv, CTUCANFD_RX_STATUS));
	while (framecnt && work_done < quota && res > 0) {
		res = ctucan_rx(ndev);
		work_done++;
		framecnt = FIELD_GET(REG_RX_STATUS_RXFRC, ctucan_read32(priv, CTUCANFD_RX_STATUS));
	}

	 
	status = ctucan_read32(priv, CTUCANFD_STATUS);
	if (FIELD_GET(REG_STATUS_DOR, status)) {
		struct net_device_stats *stats = &ndev->stats;
		struct can_frame *cf;
		struct sk_buff *skb;

		netdev_info(ndev, "rx_poll: rx fifo overflow\n");
		stats->rx_over_errors++;
		stats->rx_errors++;
		skb = alloc_can_err_skb(ndev, &cf);
		if (skb) {
			cf->can_id |= CAN_ERR_CRTL;
			cf->data[1] |= CAN_ERR_CRTL_RX_OVERFLOW;
			stats->rx_packets++;
			stats->rx_bytes += cf->can_dlc;
			netif_rx(skb);
		}

		 
		ctucan_write32(priv, CTUCANFD_COMMAND, REG_COMMAND_CDO);
	}

	if (!framecnt && res != 0) {
		if (napi_complete_done(napi, work_done)) {
			 
			ctucan_write32(priv, CTUCANFD_INT_STAT, REG_INT_STAT_RBNEI);
			ctucan_write32(priv, CTUCANFD_INT_MASK_CLR, REG_INT_STAT_RBNEI);
		}
	}

	return work_done;
}

 
static void ctucan_rotate_txb_prio(struct net_device *ndev)
{
	struct ctucan_priv *priv = netdev_priv(ndev);
	u32 prio = priv->txb_prio;

	prio = (prio << 4) | ((prio >> ((priv->ntxbufs - 1) * 4)) & 0xF);
	ctucan_netdev_dbg(ndev, "%s: from 0x%08x to 0x%08x\n", __func__, priv->txb_prio, prio);
	priv->txb_prio = prio;
	ctucan_write32(priv, CTUCANFD_TX_PRIORITY, prio);
}

 
static void ctucan_tx_interrupt(struct net_device *ndev)
{
	struct ctucan_priv *priv = netdev_priv(ndev);
	struct net_device_stats *stats = &ndev->stats;
	bool first = true;
	bool some_buffers_processed;
	unsigned long flags;
	enum ctucan_txtb_status txtb_status;
	u32 txtb_id;

	 
	do {
		spin_lock_irqsave(&priv->tx_lock, flags);

		some_buffers_processed = false;
		while ((int)(priv->txb_head - priv->txb_tail) > 0) {
			txtb_id = priv->txb_tail % priv->ntxbufs;
			txtb_status = ctucan_get_tx_status(priv, txtb_id);

			ctucan_netdev_dbg(ndev, "TXI: TXB#%u: status 0x%x\n", txtb_id, txtb_status);

			switch (txtb_status) {
			case TXT_TOK:
				ctucan_netdev_dbg(ndev, "TXT_OK\n");
				stats->tx_bytes += can_get_echo_skb(ndev, txtb_id, NULL);
				stats->tx_packets++;
				break;
			case TXT_ERR:
				 
				netdev_warn(ndev, "TXB in Error state\n");
				can_free_echo_skb(ndev, txtb_id, NULL);
				stats->tx_dropped++;
				break;
			case TXT_ABT:
				 
				netdev_warn(ndev, "TXB in Aborted state\n");
				can_free_echo_skb(ndev, txtb_id, NULL);
				stats->tx_dropped++;
				break;
			default:
				 
				if (first) {
					netdev_err(ndev,
						   "BUG: TXB#%u not in a finished state (0x%x)!\n",
						   txtb_id, txtb_status);
					spin_unlock_irqrestore(&priv->tx_lock, flags);
					 
					return;
				}
				goto clear;
			}
			priv->txb_tail++;
			first = false;
			some_buffers_processed = true;
			 
			ctucan_rotate_txb_prio(ndev);
			ctucan_give_txtb_cmd(priv, TXT_CMD_SET_EMPTY, txtb_id);
		}
clear:
		spin_unlock_irqrestore(&priv->tx_lock, flags);

		 
		if (some_buffers_processed) {
			 
			ctucan_write32(priv, CTUCANFD_INT_STAT, REG_INT_STAT_TXBHCI);
		}
	} while (some_buffers_processed);

	spin_lock_irqsave(&priv->tx_lock, flags);

	 
	if (CTU_CAN_FD_TXTNF(priv))
		netif_wake_queue(ndev);

	spin_unlock_irqrestore(&priv->tx_lock, flags);
}

 
static irqreturn_t ctucan_interrupt(int irq, void *dev_id)
{
	struct net_device *ndev = (struct net_device *)dev_id;
	struct ctucan_priv *priv = netdev_priv(ndev);
	u32 isr, icr;
	u32 imask;
	int irq_loops;

	for (irq_loops = 0; irq_loops < 10000; irq_loops++) {
		 
		isr = ctucan_read32(priv, CTUCANFD_INT_STAT);

		if (!isr)
			return irq_loops ? IRQ_HANDLED : IRQ_NONE;

		 
		if (FIELD_GET(REG_INT_STAT_RBNEI, isr)) {
			ctucan_netdev_dbg(ndev, "RXBNEI\n");
			 
			icr = REG_INT_STAT_RBNEI;
			ctucan_write32(priv, CTUCANFD_INT_MASK_SET, icr);
			ctucan_write32(priv, CTUCANFD_INT_STAT, icr);
			napi_schedule(&priv->napi);
		}

		 
		if (FIELD_GET(REG_INT_STAT_TXBHCI, isr)) {
			ctucan_netdev_dbg(ndev, "TXBHCI\n");
			 
			ctucan_tx_interrupt(ndev);
		}

		 
		if (FIELD_GET(REG_INT_STAT_EWLI, isr) ||
		    FIELD_GET(REG_INT_STAT_FCSI, isr) ||
		    FIELD_GET(REG_INT_STAT_ALI, isr)) {
			icr = isr & (REG_INT_STAT_EWLI | REG_INT_STAT_FCSI | REG_INT_STAT_ALI);

			ctucan_netdev_dbg(ndev, "some ERR interrupt: clearing 0x%08x\n", icr);
			ctucan_write32(priv, CTUCANFD_INT_STAT, icr);
			ctucan_err_interrupt(ndev, isr);
		}
		 
	}

	netdev_err(ndev, "%s: stuck interrupt (isr=0x%08x), stopping\n", __func__, isr);

	if (FIELD_GET(REG_INT_STAT_TXBHCI, isr)) {
		int i;

		netdev_err(ndev, "txb_head=0x%08x txb_tail=0x%08x\n",
			   priv->txb_head, priv->txb_tail);
		for (i = 0; i < priv->ntxbufs; i++) {
			u32 status = ctucan_get_tx_status(priv, i);

			netdev_err(ndev, "txb[%d] txb status=0x%08x\n", i, status);
		}
	}

	imask = 0xffffffff;
	ctucan_write32(priv, CTUCANFD_INT_ENA_CLR, imask);
	ctucan_write32(priv, CTUCANFD_INT_MASK_SET, imask);

	return IRQ_HANDLED;
}

 
static void ctucan_chip_stop(struct net_device *ndev)
{
	struct ctucan_priv *priv = netdev_priv(ndev);
	u32 mask = 0xffffffff;
	u32 mode;

	 
	ctucan_write32(priv, CTUCANFD_INT_ENA_CLR, mask);
	ctucan_write32(priv, CTUCANFD_INT_MASK_SET, mask);
	mode = ctucan_read32(priv, CTUCANFD_MODE);
	mode &= ~REG_MODE_ENA;
	ctucan_write32(priv, CTUCANFD_MODE, mode);

	priv->can.state = CAN_STATE_STOPPED;
}

 
static int ctucan_open(struct net_device *ndev)
{
	struct ctucan_priv *priv = netdev_priv(ndev);
	int ret;

	ret = pm_runtime_get_sync(priv->dev);
	if (ret < 0) {
		netdev_err(ndev, "%s: pm_runtime_get failed(%d)\n",
			   __func__, ret);
		pm_runtime_put_noidle(priv->dev);
		return ret;
	}

	ret = ctucan_reset(ndev);
	if (ret < 0)
		goto err_reset;

	 
	ret = open_candev(ndev);
	if (ret) {
		netdev_warn(ndev, "open_candev failed!\n");
		goto err_open;
	}

	ret = request_irq(ndev->irq, ctucan_interrupt, priv->irq_flags, ndev->name, ndev);
	if (ret < 0) {
		netdev_err(ndev, "irq allocation for CAN failed\n");
		goto err_irq;
	}

	ret = ctucan_chip_start(ndev);
	if (ret < 0) {
		netdev_err(ndev, "ctucan_chip_start failed!\n");
		goto err_chip_start;
	}

	netdev_info(ndev, "ctu_can_fd device registered\n");
	napi_enable(&priv->napi);
	netif_start_queue(ndev);

	return 0;

err_chip_start:
	free_irq(ndev->irq, ndev);
err_irq:
	close_candev(ndev);
err_open:
err_reset:
	pm_runtime_put(priv->dev);

	return ret;
}

 
static int ctucan_close(struct net_device *ndev)
{
	struct ctucan_priv *priv = netdev_priv(ndev);

	netif_stop_queue(ndev);
	napi_disable(&priv->napi);
	ctucan_chip_stop(ndev);
	free_irq(ndev->irq, ndev);
	close_candev(ndev);

	pm_runtime_put(priv->dev);

	return 0;
}

 
static int ctucan_get_berr_counter(const struct net_device *ndev, struct can_berr_counter *bec)
{
	struct ctucan_priv *priv = netdev_priv(ndev);
	int ret;

	ret = pm_runtime_get_sync(priv->dev);
	if (ret < 0) {
		netdev_err(ndev, "%s: pm_runtime_get failed(%d)\n", __func__, ret);
		pm_runtime_put_noidle(priv->dev);
		return ret;
	}

	ctucan_get_rec_tec(priv, bec);
	pm_runtime_put(priv->dev);

	return 0;
}

static const struct net_device_ops ctucan_netdev_ops = {
	.ndo_open	= ctucan_open,
	.ndo_stop	= ctucan_close,
	.ndo_start_xmit	= ctucan_start_xmit,
	.ndo_change_mtu	= can_change_mtu,
};

static const struct ethtool_ops ctucan_ethtool_ops = {
	.get_ts_info = ethtool_op_get_ts_info,
};

int ctucan_suspend(struct device *dev)
{
	struct net_device *ndev = dev_get_drvdata(dev);
	struct ctucan_priv *priv = netdev_priv(ndev);

	if (netif_running(ndev)) {
		netif_stop_queue(ndev);
		netif_device_detach(ndev);
	}

	priv->can.state = CAN_STATE_SLEEPING;

	return 0;
}
EXPORT_SYMBOL(ctucan_suspend);

int ctucan_resume(struct device *dev)
{
	struct net_device *ndev = dev_get_drvdata(dev);
	struct ctucan_priv *priv = netdev_priv(ndev);

	priv->can.state = CAN_STATE_ERROR_ACTIVE;

	if (netif_running(ndev)) {
		netif_device_attach(ndev);
		netif_start_queue(ndev);
	}

	return 0;
}
EXPORT_SYMBOL(ctucan_resume);

int ctucan_probe_common(struct device *dev, void __iomem *addr, int irq, unsigned int ntxbufs,
			unsigned long can_clk_rate, int pm_enable_call,
			void (*set_drvdata_fnc)(struct device *dev, struct net_device *ndev))
{
	struct ctucan_priv *priv;
	struct net_device *ndev;
	int ret;

	 
	ndev = alloc_candev(sizeof(struct ctucan_priv), ntxbufs);
	if (!ndev)
		return -ENOMEM;

	priv = netdev_priv(ndev);
	spin_lock_init(&priv->tx_lock);
	INIT_LIST_HEAD(&priv->peers_on_pdev);
	priv->ntxbufs = ntxbufs;
	priv->dev = dev;
	priv->can.bittiming_const = &ctu_can_fd_bit_timing_max;
	priv->can.data_bittiming_const = &ctu_can_fd_bit_timing_data_max;
	priv->can.do_set_mode = ctucan_do_set_mode;

	 
	priv->can.do_set_bittiming = ctucan_set_bittiming;
	priv->can.do_set_data_bittiming = ctucan_set_data_bittiming;

	priv->can.do_get_berr_counter = ctucan_get_berr_counter;
	priv->can.ctrlmode_supported = CAN_CTRLMODE_LOOPBACK
					| CAN_CTRLMODE_LISTENONLY
					| CAN_CTRLMODE_FD
					| CAN_CTRLMODE_PRESUME_ACK
					| CAN_CTRLMODE_BERR_REPORTING
					| CAN_CTRLMODE_FD_NON_ISO
					| CAN_CTRLMODE_ONE_SHOT;
	priv->mem_base = addr;

	 
	ndev->irq = irq;
	ndev->flags |= IFF_ECHO;	 

	if (set_drvdata_fnc)
		set_drvdata_fnc(dev, ndev);
	SET_NETDEV_DEV(ndev, dev);
	ndev->netdev_ops = &ctucan_netdev_ops;
	ndev->ethtool_ops = &ctucan_ethtool_ops;

	 
	if (!can_clk_rate) {
		priv->can_clk = devm_clk_get(dev, NULL);
		if (IS_ERR(priv->can_clk)) {
			dev_err(dev, "Device clock not found.\n");
			ret = PTR_ERR(priv->can_clk);
			goto err_free;
		}
		can_clk_rate = clk_get_rate(priv->can_clk);
	}

	priv->write_reg = ctucan_write32_le;
	priv->read_reg = ctucan_read32_le;

	if (pm_enable_call)
		pm_runtime_enable(dev);
	ret = pm_runtime_get_sync(dev);
	if (ret < 0) {
		netdev_err(ndev, "%s: pm_runtime_get failed(%d)\n",
			   __func__, ret);
		pm_runtime_put_noidle(priv->dev);
		goto err_pmdisable;
	}

	 
	if ((ctucan_read32(priv, CTUCANFD_DEVICE_ID) & 0xFFFF) != CTUCANFD_ID) {
		priv->write_reg = ctucan_write32_be;
		priv->read_reg = ctucan_read32_be;
		if ((ctucan_read32(priv, CTUCANFD_DEVICE_ID) & 0xFFFF) != CTUCANFD_ID) {
			netdev_err(ndev, "CTU_CAN_FD signature not found\n");
			ret = -ENODEV;
			goto err_deviceoff;
		}
	}

	ret = ctucan_reset(ndev);
	if (ret < 0)
		goto err_deviceoff;

	priv->can.clock.freq = can_clk_rate;

	netif_napi_add(ndev, &priv->napi, ctucan_rx_poll);

	ret = register_candev(ndev);
	if (ret) {
		dev_err(dev, "fail to register failed (err=%d)\n", ret);
		goto err_deviceoff;
	}

	pm_runtime_put(dev);

	netdev_dbg(ndev, "mem_base=0x%p irq=%d clock=%d, no. of txt buffers:%d\n",
		   priv->mem_base, ndev->irq, priv->can.clock.freq, priv->ntxbufs);

	return 0;

err_deviceoff:
	pm_runtime_put(priv->dev);
err_pmdisable:
	if (pm_enable_call)
		pm_runtime_disable(dev);
err_free:
	list_del_init(&priv->peers_on_pdev);
	free_candev(ndev);
	return ret;
}
EXPORT_SYMBOL(ctucan_probe_common);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Martin Jerabek <martin.jerabek01@gmail.com>");
MODULE_AUTHOR("Pavel Pisa <pisa@cmp.felk.cvut.cz>");
MODULE_AUTHOR("Ondrej Ille <ondrej.ille@gmail.com>");
MODULE_DESCRIPTION("CTU CAN FD interface");
