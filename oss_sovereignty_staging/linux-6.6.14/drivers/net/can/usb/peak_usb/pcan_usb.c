
 
#include <asm/unaligned.h>

#include <linux/ethtool.h>
#include <linux/module.h>
#include <linux/netdevice.h>
#include <linux/usb.h>

#include <linux/can.h>
#include <linux/can/dev.h>
#include <linux/can/error.h>

#include "pcan_usb_core.h"

 
#define PCAN_USB_EP_CMDOUT		1
#define PCAN_USB_EP_CMDIN		(PCAN_USB_EP_CMDOUT | USB_DIR_IN)
#define PCAN_USB_EP_MSGOUT		2
#define PCAN_USB_EP_MSGIN		(PCAN_USB_EP_MSGOUT | USB_DIR_IN)

 
#define PCAN_USB_CMD_FUNC		0
#define PCAN_USB_CMD_NUM		1
#define PCAN_USB_CMD_ARGS		2
#define PCAN_USB_CMD_ARGS_LEN		14
#define PCAN_USB_CMD_LEN		(PCAN_USB_CMD_ARGS + \
					 PCAN_USB_CMD_ARGS_LEN)

 
#define PCAN_USB_CMD_BITRATE	1
#define PCAN_USB_CMD_SET_BUS	3
#define PCAN_USB_CMD_DEVID	4
#define PCAN_USB_CMD_SN		6
#define PCAN_USB_CMD_REGISTER	9
#define PCAN_USB_CMD_EXT_VCC	10
#define PCAN_USB_CMD_ERR_FR	11
#define PCAN_USB_CMD_LED	12

 
#define PCAN_USB_BUS_XCVER		2
#define PCAN_USB_BUS_SILENT_MODE	3

 
#define PCAN_USB_GET		1
#define PCAN_USB_SET		2

 
#define PCAN_USB_COMMAND_TIMEOUT	1000

 
#define PCAN_USB_STARTUP_TIMEOUT	10

 
#define PCAN_USB_RX_BUFFER_SIZE		64
#define PCAN_USB_TX_BUFFER_SIZE		64

#define PCAN_USB_MSG_HEADER_LEN		2

#define PCAN_USB_MSG_TX_CAN		2	 

 
#define PCAN_USB_CRYSTAL_HZ		16000000

 
#define PCAN_USB_STATUSLEN_TIMESTAMP	(1 << 7)
#define PCAN_USB_STATUSLEN_INTERNAL	(1 << 6)
#define PCAN_USB_STATUSLEN_EXT_ID	(1 << 5)
#define PCAN_USB_STATUSLEN_RTR		(1 << 4)
#define PCAN_USB_STATUSLEN_DLC		(0xf)

 
#define PCAN_USB_TX_SRR			0x01	 
#define PCAN_USB_TX_AT			0x02	 

 
#define PCAN_USB_ERROR_TXFULL		0x01
#define PCAN_USB_ERROR_RXQOVR		0x02
#define PCAN_USB_ERROR_BUS_LIGHT	0x04
#define PCAN_USB_ERROR_BUS_HEAVY	0x08
#define PCAN_USB_ERROR_BUS_OFF		0x10
#define PCAN_USB_ERROR_RXQEMPTY		0x20
#define PCAN_USB_ERROR_QOVR		0x40
#define PCAN_USB_ERROR_TXQFULL		0x80

#define PCAN_USB_ERROR_BUS		(PCAN_USB_ERROR_BUS_LIGHT | \
					 PCAN_USB_ERROR_BUS_HEAVY | \
					 PCAN_USB_ERROR_BUS_OFF)

 
#define SJA1000_MODE_NORMAL		0x00
#define SJA1000_MODE_INIT		0x01

 
#define PCAN_USB_TS_DIV_SHIFTER		20
#define PCAN_USB_TS_US_PER_TICK		44739243

 
#define PCAN_USB_REC_ERROR		1
#define PCAN_USB_REC_ANALOG		2
#define PCAN_USB_REC_BUSLOAD		3
#define PCAN_USB_REC_TS			4
#define PCAN_USB_REC_BUSEVT		5

 
#define PCAN_USB_ERR_RXERR		0x02	 
#define PCAN_USB_ERR_TXERR		0x04	 

 
#define PCAN_USB_BERR_MASK	(PCAN_USB_ERR_RXERR | PCAN_USB_ERR_TXERR)

 
#define PCAN_USB_ERR_CNT_DEC		0x00	 
#define PCAN_USB_ERR_CNT_INC		0x80	 

 
struct pcan_usb {
	struct peak_usb_device dev;
	struct peak_time_ref time_ref;
	struct timer_list restart_timer;
	struct can_berr_counter bec;
};

 
struct pcan_usb_msg_context {
	u16 ts16;
	u8 prev_ts8;
	u8 *ptr;
	u8 *end;
	u8 rec_cnt;
	u8 rec_idx;
	u8 rec_ts_idx;
	struct net_device *netdev;
	struct pcan_usb *pdev;
};

 
static int pcan_usb_send_cmd(struct peak_usb_device *dev, u8 f, u8 n, u8 *p)
{
	int err;
	int actual_length;

	 
	if (!(dev->state & PCAN_USB_STATE_CONNECTED))
		return 0;

	dev->cmd_buf[PCAN_USB_CMD_FUNC] = f;
	dev->cmd_buf[PCAN_USB_CMD_NUM] = n;

	if (p)
		memcpy(dev->cmd_buf + PCAN_USB_CMD_ARGS,
			p, PCAN_USB_CMD_ARGS_LEN);

	err = usb_bulk_msg(dev->udev,
			usb_sndbulkpipe(dev->udev, PCAN_USB_EP_CMDOUT),
			dev->cmd_buf, PCAN_USB_CMD_LEN, &actual_length,
			PCAN_USB_COMMAND_TIMEOUT);
	if (err)
		netdev_err(dev->netdev,
			"sending cmd f=0x%x n=0x%x failure: %d\n",
			f, n, err);
	return err;
}

 
static int pcan_usb_wait_rsp(struct peak_usb_device *dev, u8 f, u8 n, u8 *p)
{
	int err;
	int actual_length;

	 
	if (!(dev->state & PCAN_USB_STATE_CONNECTED))
		return 0;

	 
	err = pcan_usb_send_cmd(dev, f, n, NULL);
	if (err)
		return err;

	err = usb_bulk_msg(dev->udev,
		usb_rcvbulkpipe(dev->udev, PCAN_USB_EP_CMDIN),
		dev->cmd_buf, PCAN_USB_CMD_LEN, &actual_length,
		PCAN_USB_COMMAND_TIMEOUT);
	if (err)
		netdev_err(dev->netdev,
			"waiting rsp f=0x%x n=0x%x failure: %d\n", f, n, err);
	else if (p)
		memcpy(p, dev->cmd_buf + PCAN_USB_CMD_ARGS,
			PCAN_USB_CMD_ARGS_LEN);

	return err;
}

static int pcan_usb_set_sja1000(struct peak_usb_device *dev, u8 mode)
{
	u8 args[PCAN_USB_CMD_ARGS_LEN] = {
		[1] = mode,
	};

	return pcan_usb_send_cmd(dev, PCAN_USB_CMD_REGISTER, PCAN_USB_SET,
				 args);
}

static int pcan_usb_set_bus(struct peak_usb_device *dev, u8 onoff)
{
	u8 args[PCAN_USB_CMD_ARGS_LEN] = {
		[0] = !!onoff,
	};

	return pcan_usb_send_cmd(dev, PCAN_USB_CMD_SET_BUS, PCAN_USB_BUS_XCVER,
				 args);
}

static int pcan_usb_set_silent(struct peak_usb_device *dev, u8 onoff)
{
	u8 args[PCAN_USB_CMD_ARGS_LEN] = {
		[0] = !!onoff,
	};

	return pcan_usb_send_cmd(dev, PCAN_USB_CMD_SET_BUS,
				 PCAN_USB_BUS_SILENT_MODE, args);
}

 
static int pcan_usb_set_err_frame(struct peak_usb_device *dev, u8 err_mask)
{
	u8 args[PCAN_USB_CMD_ARGS_LEN] = {
		[0] = err_mask,
	};

	return pcan_usb_send_cmd(dev, PCAN_USB_CMD_ERR_FR, PCAN_USB_SET, args);
}

static int pcan_usb_set_ext_vcc(struct peak_usb_device *dev, u8 onoff)
{
	u8 args[PCAN_USB_CMD_ARGS_LEN] = {
		[0] = !!onoff,
	};

	return pcan_usb_send_cmd(dev, PCAN_USB_CMD_EXT_VCC, PCAN_USB_SET, args);
}

static int pcan_usb_set_led(struct peak_usb_device *dev, u8 onoff)
{
	u8 args[PCAN_USB_CMD_ARGS_LEN] = {
		[0] = !!onoff,
	};

	return pcan_usb_send_cmd(dev, PCAN_USB_CMD_LED, PCAN_USB_SET, args);
}

 
static int pcan_usb_set_bittiming(struct peak_usb_device *dev,
				  struct can_bittiming *bt)
{
	u8 args[PCAN_USB_CMD_ARGS_LEN];
	u8 btr0, btr1;

	btr0 = ((bt->brp - 1) & 0x3f) | (((bt->sjw - 1) & 0x3) << 6);
	btr1 = ((bt->prop_seg + bt->phase_seg1 - 1) & 0xf) |
		(((bt->phase_seg2 - 1) & 0x7) << 4);
	if (dev->can.ctrlmode & CAN_CTRLMODE_3_SAMPLES)
		btr1 |= 0x80;

	netdev_info(dev->netdev, "setting BTR0=0x%02x BTR1=0x%02x\n",
		btr0, btr1);

	args[0] = btr1;
	args[1] = btr0;

	return pcan_usb_send_cmd(dev, PCAN_USB_CMD_BITRATE, PCAN_USB_SET, args);
}

 
static int pcan_usb_write_mode(struct peak_usb_device *dev, u8 onoff)
{
	int err;

	err = pcan_usb_set_bus(dev, onoff);
	if (err)
		return err;

	if (!onoff) {
		err = pcan_usb_set_sja1000(dev, SJA1000_MODE_INIT);
	} else {
		 
		set_current_state(TASK_INTERRUPTIBLE);
		schedule_timeout(msecs_to_jiffies(PCAN_USB_STARTUP_TIMEOUT));
	}

	return err;
}

 
static void pcan_usb_restart(struct timer_list *t)
{
	struct pcan_usb *pdev = from_timer(pdev, t, restart_timer);
	struct peak_usb_device *dev = &pdev->dev;

	 
	peak_usb_restart_complete(dev);
}

 
static void pcan_usb_restart_pending(struct urb *urb)
{
	struct pcan_usb *pdev = urb->context;

	 
	mod_timer(&pdev->restart_timer,
			jiffies + msecs_to_jiffies(PCAN_USB_STARTUP_TIMEOUT));

	 
	peak_usb_async_complete(urb);
}

 
static int pcan_usb_restart_async(struct peak_usb_device *dev, struct urb *urb,
				  u8 *buf)
{
	struct pcan_usb *pdev = container_of(dev, struct pcan_usb, dev);

	if (timer_pending(&pdev->restart_timer))
		return -EBUSY;

	 
	buf[PCAN_USB_CMD_FUNC] = 3;
	buf[PCAN_USB_CMD_NUM] = 2;
	buf[PCAN_USB_CMD_ARGS] = 1;

	usb_fill_bulk_urb(urb, dev->udev,
			usb_sndbulkpipe(dev->udev, PCAN_USB_EP_CMDOUT),
			buf, PCAN_USB_CMD_LEN,
			pcan_usb_restart_pending, pdev);

	return usb_submit_urb(urb, GFP_ATOMIC);
}

 
static int pcan_usb_get_serial(struct peak_usb_device *dev, u32 *serial_number)
{
	u8 args[PCAN_USB_CMD_ARGS_LEN];
	int err;

	err = pcan_usb_wait_rsp(dev, PCAN_USB_CMD_SN, PCAN_USB_GET, args);
	if (err)
		return err;
	*serial_number = le32_to_cpup((__le32 *)args);

	return 0;
}

 
static int pcan_usb_get_can_channel_id(struct peak_usb_device *dev, u32 *can_ch_id)
{
	u8 args[PCAN_USB_CMD_ARGS_LEN];
	int err;

	err = pcan_usb_wait_rsp(dev, PCAN_USB_CMD_DEVID, PCAN_USB_GET, args);
	if (err)
		netdev_err(dev->netdev, "getting can channel id failure: %d\n", err);

	else
		*can_ch_id = args[0];

	return err;
}

 
static int pcan_usb_set_can_channel_id(struct peak_usb_device *dev, u32 can_ch_id)
{
	u8 args[PCAN_USB_CMD_ARGS_LEN];

	 
	if (can_ch_id > U8_MAX)
		return -EINVAL;

	 
	if (dev->netdev->flags & IFF_UP)
		return -EBUSY;

	args[0] = can_ch_id;
	return pcan_usb_send_cmd(dev, PCAN_USB_CMD_DEVID, PCAN_USB_SET, args);
}

 
static int pcan_usb_update_ts(struct pcan_usb_msg_context *mc)
{
	if ((mc->ptr + 2) > mc->end)
		return -EINVAL;

	mc->ts16 = get_unaligned_le16(mc->ptr);

	if (mc->rec_idx > 0)
		peak_usb_update_ts_now(&mc->pdev->time_ref, mc->ts16);
	else
		peak_usb_set_ts_now(&mc->pdev->time_ref, mc->ts16);

	return 0;
}

 
static int pcan_usb_decode_ts(struct pcan_usb_msg_context *mc, u8 first_packet)
{
	 
	if (first_packet) {
		if ((mc->ptr + 2) > mc->end)
			return -EINVAL;

		mc->ts16 = get_unaligned_le16(mc->ptr);
		mc->prev_ts8 = mc->ts16 & 0x00ff;

		mc->ptr += 2;
	} else {
		u8 ts8;

		if ((mc->ptr + 1) > mc->end)
			return -EINVAL;

		ts8 = *mc->ptr++;

		if (ts8 < mc->prev_ts8)
			mc->ts16 += 0x100;

		mc->ts16 &= 0xff00;
		mc->ts16 |= ts8;
		mc->prev_ts8 = ts8;
	}

	return 0;
}

static int pcan_usb_decode_error(struct pcan_usb_msg_context *mc, u8 n,
				 u8 status_len)
{
	struct sk_buff *skb;
	struct can_frame *cf;
	enum can_state new_state = CAN_STATE_ERROR_ACTIVE;

	 
	if (n == PCAN_USB_ERROR_QOVR)
		if (!mc->pdev->time_ref.tick_count)
			return 0;

	 
	skb = alloc_can_err_skb(mc->netdev, &cf);

	if (n & PCAN_USB_ERROR_RXQOVR) {
		 
		netdev_dbg(mc->netdev, "data overrun interrupt\n");
		mc->netdev->stats.rx_over_errors++;
		mc->netdev->stats.rx_errors++;
		if (cf) {
			cf->can_id |= CAN_ERR_CRTL;
			cf->data[1] |= CAN_ERR_CRTL_RX_OVERFLOW;
		}
	}

	if (n & PCAN_USB_ERROR_TXQFULL)
		netdev_dbg(mc->netdev, "device Tx queue full)\n");

	if (n & PCAN_USB_ERROR_BUS_OFF) {
		new_state = CAN_STATE_BUS_OFF;
	} else if (n & PCAN_USB_ERROR_BUS_HEAVY) {
		new_state = ((mc->pdev->bec.txerr >= 128) ||
			     (mc->pdev->bec.rxerr >= 128)) ?
				CAN_STATE_ERROR_PASSIVE :
				CAN_STATE_ERROR_WARNING;
	} else {
		new_state = CAN_STATE_ERROR_ACTIVE;
	}

	 
	if (new_state != mc->pdev->dev.can.state) {
		enum can_state tx_state =
			(mc->pdev->bec.txerr >= mc->pdev->bec.rxerr) ?
				new_state : 0;
		enum can_state rx_state =
			(mc->pdev->bec.txerr <= mc->pdev->bec.rxerr) ?
				new_state : 0;

		can_change_state(mc->netdev, cf, tx_state, rx_state);

		if (new_state == CAN_STATE_BUS_OFF) {
			can_bus_off(mc->netdev);
		} else if (cf && (cf->can_id & CAN_ERR_CRTL)) {
			 
			cf->can_id = CAN_ERR_CNT;
			cf->data[6] = mc->pdev->bec.txerr;
			cf->data[7] = mc->pdev->bec.rxerr;
		}
	}

	if (!skb)
		return -ENOMEM;

	if (status_len & PCAN_USB_STATUSLEN_TIMESTAMP) {
		struct skb_shared_hwtstamps *hwts = skb_hwtstamps(skb);

		peak_usb_get_ts_time(&mc->pdev->time_ref, mc->ts16,
				     &hwts->hwtstamp);
	}

	netif_rx(skb);

	return 0;
}

 
static int pcan_usb_handle_bus_evt(struct pcan_usb_msg_context *mc, u8 ir)
{
	struct pcan_usb *pdev = mc->pdev;

	 
	switch (ir) {
	case PCAN_USB_ERR_CNT_DEC:
	case PCAN_USB_ERR_CNT_INC:

		 
		pdev->bec.rxerr = mc->ptr[1];
		pdev->bec.txerr = mc->ptr[2];
		break;

	default:
		 
		break;
	}

	return 0;
}

 
static int pcan_usb_decode_status(struct pcan_usb_msg_context *mc,
				  u8 status_len)
{
	u8 rec_len = status_len & PCAN_USB_STATUSLEN_DLC;
	u8 f, n;
	int err;

	 
	if ((mc->ptr + 2) > mc->end)
		return -EINVAL;

	f = mc->ptr[PCAN_USB_CMD_FUNC];
	n = mc->ptr[PCAN_USB_CMD_NUM];
	mc->ptr += PCAN_USB_CMD_ARGS;

	if (status_len & PCAN_USB_STATUSLEN_TIMESTAMP) {
		int err = pcan_usb_decode_ts(mc, !mc->rec_ts_idx);

		if (err)
			return err;

		 
		mc->rec_ts_idx++;
	}

	switch (f) {
	case PCAN_USB_REC_ERROR:
		err = pcan_usb_decode_error(mc, n, status_len);
		if (err)
			return err;
		break;

	case PCAN_USB_REC_ANALOG:
		 
		rec_len = 2;
		break;

	case PCAN_USB_REC_BUSLOAD:
		 
		rec_len = 1;
		break;

	case PCAN_USB_REC_TS:
		 
		if (pcan_usb_update_ts(mc))
			return -EINVAL;
		break;

	case PCAN_USB_REC_BUSEVT:
		 
		err = pcan_usb_handle_bus_evt(mc, n);
		if (err)
			return err;
		break;
	default:
		netdev_err(mc->netdev, "unexpected function %u\n", f);
		break;
	}

	if ((mc->ptr + rec_len) > mc->end)
		return -EINVAL;

	mc->ptr += rec_len;

	return 0;
}

 
static int pcan_usb_decode_data(struct pcan_usb_msg_context *mc, u8 status_len)
{
	u8 rec_len = status_len & PCAN_USB_STATUSLEN_DLC;
	struct sk_buff *skb;
	struct can_frame *cf;
	struct skb_shared_hwtstamps *hwts;
	u32 can_id_flags;

	skb = alloc_can_skb(mc->netdev, &cf);
	if (!skb)
		return -ENOMEM;

	if (status_len & PCAN_USB_STATUSLEN_EXT_ID) {
		if ((mc->ptr + 4) > mc->end)
			goto decode_failed;

		can_id_flags = get_unaligned_le32(mc->ptr);
		cf->can_id = can_id_flags >> 3 | CAN_EFF_FLAG;
		mc->ptr += 4;
	} else {
		if ((mc->ptr + 2) > mc->end)
			goto decode_failed;

		can_id_flags = get_unaligned_le16(mc->ptr);
		cf->can_id = can_id_flags >> 5;
		mc->ptr += 2;
	}

	can_frame_set_cc_len(cf, rec_len, mc->pdev->dev.can.ctrlmode);

	 
	if (pcan_usb_decode_ts(mc, !mc->rec_ts_idx))
		goto decode_failed;

	 
	mc->rec_ts_idx++;

	 
	memset(cf->data, 0x0, sizeof(cf->data));
	if (status_len & PCAN_USB_STATUSLEN_RTR) {
		cf->can_id |= CAN_RTR_FLAG;
	} else {
		if ((mc->ptr + rec_len) > mc->end)
			goto decode_failed;

		memcpy(cf->data, mc->ptr, cf->len);
		mc->ptr += rec_len;

		 
		if (can_id_flags & PCAN_USB_TX_SRR)
			mc->ptr++;

		 
		mc->netdev->stats.rx_bytes += cf->len;
	}
	mc->netdev->stats.rx_packets++;

	 
	hwts = skb_hwtstamps(skb);
	peak_usb_get_ts_time(&mc->pdev->time_ref, mc->ts16, &hwts->hwtstamp);

	 
	netif_rx(skb);

	return 0;

decode_failed:
	dev_kfree_skb(skb);
	return -EINVAL;
}

 
static int pcan_usb_decode_msg(struct peak_usb_device *dev, u8 *ibuf, u32 lbuf)
{
	struct pcan_usb_msg_context mc = {
		.rec_cnt = ibuf[1],
		.ptr = ibuf + PCAN_USB_MSG_HEADER_LEN,
		.end = ibuf + lbuf,
		.netdev = dev->netdev,
		.pdev = container_of(dev, struct pcan_usb, dev),
	};
	int err;

	for (err = 0; mc.rec_idx < mc.rec_cnt && !err; mc.rec_idx++) {
		u8 sl = *mc.ptr++;

		 
		if (sl & PCAN_USB_STATUSLEN_INTERNAL) {
			err = pcan_usb_decode_status(&mc, sl);
		 
		} else {
			err = pcan_usb_decode_data(&mc, sl);
		}
	}

	return err;
}

 
static int pcan_usb_decode_buf(struct peak_usb_device *dev, struct urb *urb)
{
	int err = 0;

	if (urb->actual_length > PCAN_USB_MSG_HEADER_LEN) {
		err = pcan_usb_decode_msg(dev, urb->transfer_buffer,
			urb->actual_length);

	} else if (urb->actual_length > 0) {
		netdev_err(dev->netdev, "usb message length error (%u)\n",
			urb->actual_length);
		err = -EINVAL;
	}

	return err;
}

 
static int pcan_usb_encode_msg(struct peak_usb_device *dev, struct sk_buff *skb,
			       u8 *obuf, size_t *size)
{
	struct net_device *netdev = dev->netdev;
	struct net_device_stats *stats = &netdev->stats;
	struct can_frame *cf = (struct can_frame *)skb->data;
	u32 can_id_flags = cf->can_id & CAN_ERR_MASK;
	u8 *pc;

	obuf[0] = PCAN_USB_MSG_TX_CAN;
	obuf[1] = 1;	 

	pc = obuf + PCAN_USB_MSG_HEADER_LEN;

	 
	*pc = can_get_cc_dlc(cf, dev->can.ctrlmode);

	if (cf->can_id & CAN_RTR_FLAG)
		*pc |= PCAN_USB_STATUSLEN_RTR;

	 
	if (cf->can_id & CAN_EFF_FLAG) {
		*pc |= PCAN_USB_STATUSLEN_EXT_ID;
		pc++;

		can_id_flags <<= 3;

		if (dev->can.ctrlmode & CAN_CTRLMODE_LOOPBACK)
			can_id_flags |= PCAN_USB_TX_SRR;

		if (dev->can.ctrlmode & CAN_CTRLMODE_ONE_SHOT)
			can_id_flags |= PCAN_USB_TX_AT;

		put_unaligned_le32(can_id_flags, pc);
		pc += 4;
	} else {
		pc++;

		can_id_flags <<= 5;

		if (dev->can.ctrlmode & CAN_CTRLMODE_LOOPBACK)
			can_id_flags |= PCAN_USB_TX_SRR;

		if (dev->can.ctrlmode & CAN_CTRLMODE_ONE_SHOT)
			can_id_flags |= PCAN_USB_TX_AT;

		put_unaligned_le16(can_id_flags, pc);
		pc += 2;
	}

	 
	if (!(cf->can_id & CAN_RTR_FLAG)) {
		memcpy(pc, cf->data, cf->len);
		pc += cf->len;
	}

	 
	if (can_id_flags & PCAN_USB_TX_SRR)
		*pc++ = 0x80;

	obuf[(*size)-1] = (u8)(stats->tx_packets & 0xff);

	return 0;
}

 
static int pcan_usb_get_berr_counter(const struct net_device *netdev,
				     struct can_berr_counter *bec)
{
	struct peak_usb_device *dev = netdev_priv(netdev);
	struct pcan_usb *pdev = container_of(dev, struct pcan_usb, dev);

	*bec = pdev->bec;

	 
	return 0;
}

 
static int pcan_usb_start(struct peak_usb_device *dev)
{
	struct pcan_usb *pdev = container_of(dev, struct pcan_usb, dev);
	int err;

	 
	peak_usb_init_time_ref(&pdev->time_ref, &pcan_usb);

	pdev->bec.rxerr = 0;
	pdev->bec.txerr = 0;

	 
	err = pcan_usb_set_err_frame(dev, PCAN_USB_BERR_MASK);
	if (err)
		netdev_warn(dev->netdev,
			    "Asking for BERR reporting error %u\n",
			    err);

	 
	if (dev->device_rev > 3) {
		err = pcan_usb_set_silent(dev,
				dev->can.ctrlmode & CAN_CTRLMODE_LISTENONLY);
		if (err)
			return err;
	}

	return pcan_usb_set_ext_vcc(dev, 0);
}

static int pcan_usb_init(struct peak_usb_device *dev)
{
	struct pcan_usb *pdev = container_of(dev, struct pcan_usb, dev);
	u32 serial_number;
	int err;

	 
	timer_setup(&pdev->restart_timer, pcan_usb_restart, 0);

	 
	err = pcan_usb_get_serial(dev, &serial_number);
	if (err) {
		dev_err(dev->netdev->dev.parent,
			"unable to read %s serial number (err %d)\n",
			pcan_usb.name, err);
		return err;
	}

	dev_info(dev->netdev->dev.parent,
		 "PEAK-System %s adapter hwrev %u serial %08X (%u channel)\n",
		 pcan_usb.name, dev->device_rev, serial_number,
		 pcan_usb.ctrl_count);

	 
	if (dev->device_rev >= 41) {
		struct can_priv *priv = netdev_priv(dev->netdev);

		priv->ctrlmode_supported |= CAN_CTRLMODE_ONE_SHOT |
					    CAN_CTRLMODE_LOOPBACK;
	} else {
		dev_info(dev->netdev->dev.parent,
			 "Firmware update available. Please contact support@peak-system.com\n");
	}

	return 0;
}

 
static int pcan_usb_probe(struct usb_interface *intf)
{
	struct usb_host_interface *if_desc;
	int i;

	if_desc = intf->altsetting;

	 
	for (i = 0; i < if_desc->desc.bNumEndpoints; i++) {
		struct usb_endpoint_descriptor *ep = &if_desc->endpoint[i].desc;

		switch (ep->bEndpointAddress) {
		case PCAN_USB_EP_CMDOUT:
		case PCAN_USB_EP_CMDIN:
		case PCAN_USB_EP_MSGOUT:
		case PCAN_USB_EP_MSGIN:
			break;
		default:
			return -ENODEV;
		}
	}

	return 0;
}

static int pcan_usb_set_phys_id(struct net_device *netdev,
				enum ethtool_phys_id_state state)
{
	struct peak_usb_device *dev = netdev_priv(netdev);
	int err = 0;

	switch (state) {
	case ETHTOOL_ID_ACTIVE:
		 
		return 2;

	case ETHTOOL_ID_OFF:
		err = pcan_usb_set_led(dev, 0);
		break;

	case ETHTOOL_ID_ON:
		fallthrough;

	case ETHTOOL_ID_INACTIVE:
		 
		err = pcan_usb_set_led(dev, 1);
		break;

	default:
		break;
	}

	return err;
}

 
static int pcan_usb_get_eeprom_len(struct net_device *netdev)
{
	return sizeof(u8);
}

static const struct ethtool_ops pcan_usb_ethtool_ops = {
	.set_phys_id = pcan_usb_set_phys_id,
	.get_ts_info = pcan_get_ts_info,
	.get_eeprom_len	= pcan_usb_get_eeprom_len,
	.get_eeprom = peak_usb_get_eeprom,
	.set_eeprom = peak_usb_set_eeprom,
};

 
static const struct can_bittiming_const pcan_usb_const = {
	.name = "pcan_usb",
	.tseg1_min = 1,
	.tseg1_max = 16,
	.tseg2_min = 1,
	.tseg2_max = 8,
	.sjw_max = 4,
	.brp_min = 1,
	.brp_max = 64,
	.brp_inc = 1,
};

const struct peak_usb_adapter pcan_usb = {
	.name = "PCAN-USB",
	.device_id = PCAN_USB_PRODUCT_ID,
	.ctrl_count = 1,
	.ctrlmode_supported = CAN_CTRLMODE_3_SAMPLES | CAN_CTRLMODE_LISTENONLY |
			      CAN_CTRLMODE_CC_LEN8_DLC,
	.clock = {
		.freq = PCAN_USB_CRYSTAL_HZ / 2,
	},
	.bittiming_const = &pcan_usb_const,

	 
	.sizeof_dev_private = sizeof(struct pcan_usb),

	.ethtool_ops = &pcan_usb_ethtool_ops,

	 
	.ts_used_bits = 16,
	.us_per_ts_scale = PCAN_USB_TS_US_PER_TICK,  
	.us_per_ts_shift = PCAN_USB_TS_DIV_SHIFTER,  

	 
	.ep_msg_in = PCAN_USB_EP_MSGIN,
	.ep_msg_out = {PCAN_USB_EP_MSGOUT},

	 
	.rx_buffer_size = PCAN_USB_RX_BUFFER_SIZE,
	.tx_buffer_size = PCAN_USB_TX_BUFFER_SIZE,

	 
	.intf_probe = pcan_usb_probe,
	.dev_init = pcan_usb_init,
	.dev_set_bus = pcan_usb_write_mode,
	.dev_set_bittiming = pcan_usb_set_bittiming,
	.dev_get_can_channel_id = pcan_usb_get_can_channel_id,
	.dev_set_can_channel_id = pcan_usb_set_can_channel_id,
	.dev_decode_buf = pcan_usb_decode_buf,
	.dev_encode_msg = pcan_usb_encode_msg,
	.dev_start = pcan_usb_start,
	.dev_restart_async = pcan_usb_restart_async,
	.do_get_berr_counter = pcan_usb_get_berr_counter,
};
