

 

#include <asm/unaligned.h>
#include <linux/crc16.h>
#include <linux/ethtool.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/usb.h>
#include <net/devlink.h>

#include "es58x_core.h"

MODULE_AUTHOR("Vincent Mailhol <mailhol.vincent@wanadoo.fr>");
MODULE_AUTHOR("Arunachalam Santhanam <arunachalam.santhanam@in.bosch.com>");
MODULE_DESCRIPTION("Socket CAN driver for ETAS ES58X USB adapters");
MODULE_LICENSE("GPL v2");

#define ES58X_VENDOR_ID 0x108C
#define ES581_4_PRODUCT_ID 0x0159
#define ES582_1_PRODUCT_ID 0x0168
#define ES584_1_PRODUCT_ID 0x0169

 
#define ES58X_FD_INTERFACE_PROTOCOL 0

 
static const struct usb_device_id es58x_id_table[] = {
	{
		 
		USB_DEVICE(ES58X_VENDOR_ID, ES581_4_PRODUCT_ID),
		.driver_info = ES58X_DUAL_CHANNEL
	}, {
		 
		USB_DEVICE_INTERFACE_PROTOCOL(ES58X_VENDOR_ID, ES582_1_PRODUCT_ID,
					      ES58X_FD_INTERFACE_PROTOCOL),
		.driver_info = ES58X_DUAL_CHANNEL | ES58X_FD_FAMILY
	}, {
		 
		USB_DEVICE_INTERFACE_PROTOCOL(ES58X_VENDOR_ID, ES584_1_PRODUCT_ID,
					      ES58X_FD_INTERFACE_PROTOCOL),
		.driver_info = ES58X_FD_FAMILY
	}, {
		 
	}
};

MODULE_DEVICE_TABLE(usb, es58x_id_table);

#define es58x_print_hex_dump(buf, len)					\
	print_hex_dump(KERN_DEBUG,					\
		       KBUILD_MODNAME " " __stringify(buf) ": ",	\
		       DUMP_PREFIX_NONE, 16, 1, buf, len, false)

#define es58x_print_hex_dump_debug(buf, len)				 \
	print_hex_dump_debug(KBUILD_MODNAME " " __stringify(buf) ": ",\
			     DUMP_PREFIX_NONE, 16, 1, buf, len, false)

 
#define ES58X_CRC_CALC_OFFSET sizeof_field(union es58x_urb_cmd, sof)

 
static u16 es58x_calculate_crc(const union es58x_urb_cmd *urb_cmd, u16 urb_len)
{
	u16 crc;
	ssize_t len = urb_len - ES58X_CRC_CALC_OFFSET - sizeof(crc);

	crc = crc16(0, &urb_cmd->raw_cmd[ES58X_CRC_CALC_OFFSET], len);
	return crc;
}

 
static u16 es58x_get_crc(const union es58x_urb_cmd *urb_cmd, u16 urb_len)
{
	u16 crc;
	const __le16 *crc_addr;

	crc_addr = (__le16 *)&urb_cmd->raw_cmd[urb_len - sizeof(crc)];
	crc = get_unaligned_le16(crc_addr);
	return crc;
}

 
static void es58x_set_crc(union es58x_urb_cmd *urb_cmd, u16 urb_len)
{
	u16 crc;
	__le16 *crc_addr;

	crc = es58x_calculate_crc(urb_cmd, urb_len);
	crc_addr = (__le16 *)&urb_cmd->raw_cmd[urb_len - sizeof(crc)];
	put_unaligned_le16(crc, crc_addr);
}

 
static int es58x_check_crc(struct es58x_device *es58x_dev,
			   const union es58x_urb_cmd *urb_cmd, u16 urb_len)
{
	u16 calculated_crc = es58x_calculate_crc(urb_cmd, urb_len);
	u16 expected_crc = es58x_get_crc(urb_cmd, urb_len);

	if (expected_crc != calculated_crc) {
		dev_err_ratelimited(es58x_dev->dev,
				    "%s: Bad CRC, urb_len: %d\n",
				    __func__, urb_len);
		return -EBADMSG;
	}

	return 0;
}

 
static u64 es58x_timestamp_to_ns(u64 timestamp)
{
	const u64 es58x_timestamp_ns_mult_coef = 500ULL;

	return es58x_timestamp_ns_mult_coef * timestamp;
}

 
static void es58x_set_skb_timestamp(struct net_device *netdev,
				    struct sk_buff *skb, u64 timestamp)
{
	struct es58x_device *es58x_dev = es58x_priv(netdev)->es58x_dev;
	struct skb_shared_hwtstamps *hwts;

	hwts = skb_hwtstamps(skb);
	 
	hwts->hwtstamp = ns_to_ktime(es58x_timestamp_to_ns(timestamp) +
				     es58x_dev->realtime_diff_ns);
}

 
void es58x_rx_timestamp(struct es58x_device *es58x_dev, u64 timestamp)
{
	u64 ktime_real_ns = ktime_get_real_ns();
	u64 device_timestamp = es58x_timestamp_to_ns(timestamp);

	dev_dbg(es58x_dev->dev, "%s: request round-trip time: %llu ns\n",
		__func__, ktime_real_ns - es58x_dev->ktime_req_ns);

	es58x_dev->realtime_diff_ns =
	    (es58x_dev->ktime_req_ns + ktime_real_ns) / 2 - device_timestamp;
	es58x_dev->ktime_req_ns = 0;

	dev_dbg(es58x_dev->dev,
		"%s: Device timestamp: %llu, diff with kernel: %llu\n",
		__func__, device_timestamp, es58x_dev->realtime_diff_ns);
}

 
static int es58x_set_realtime_diff_ns(struct es58x_device *es58x_dev)
{
	if (es58x_dev->ktime_req_ns) {
		dev_warn(es58x_dev->dev,
			 "%s: Previous request to set timestamp has not completed yet\n",
			 __func__);
		return -EBUSY;
	}

	es58x_dev->ktime_req_ns = ktime_get_real_ns();
	return es58x_dev->ops->get_timestamp(es58x_dev);
}

 
static bool es58x_is_can_state_active(struct net_device *netdev)
{
	return es58x_priv(netdev)->can.state < CAN_STATE_BUS_OFF;
}

 
static bool es58x_is_echo_skb_threshold_reached(struct es58x_priv *priv)
{
	u32 num_echo_skb =  priv->tx_head - priv->tx_tail;
	u32 threshold = priv->can.echo_skb_max -
		priv->es58x_dev->param->tx_bulk_max + 1;

	return num_echo_skb >= threshold;
}

 
static void es58x_can_free_echo_skb_tail(struct net_device *netdev)
{
	struct es58x_priv *priv = es58x_priv(netdev);
	u16 fifo_mask = priv->es58x_dev->param->fifo_mask;
	unsigned int frame_len = 0;

	can_free_echo_skb(netdev, priv->tx_tail & fifo_mask, &frame_len);
	netdev_completed_queue(netdev, 1, frame_len);

	priv->tx_tail++;

	netdev->stats.tx_dropped++;
}

 
static int es58x_can_get_echo_skb_recovery(struct net_device *netdev,
					   u32 rcv_packet_idx)
{
	struct es58x_priv *priv = es58x_priv(netdev);
	int ret = 0;

	netdev->stats.tx_errors++;

	if (net_ratelimit())
		netdev_warn(netdev,
			    "Bad echo packet index: %u. First index: %u, end index %u, num_echo_skb: %02u/%02u\n",
			    rcv_packet_idx, priv->tx_tail, priv->tx_head,
			    priv->tx_head - priv->tx_tail,
			    priv->can.echo_skb_max);

	if ((s32)(rcv_packet_idx - priv->tx_tail) < 0) {
		if (net_ratelimit())
			netdev_warn(netdev,
				    "Received echo index is from the past. Ignoring it\n");
		ret = -EINVAL;
	} else if ((s32)(rcv_packet_idx - priv->tx_head) >= 0) {
		if (net_ratelimit())
			netdev_err(netdev,
				   "Received echo index is from the future. Ignoring it\n");
		ret = -EINVAL;
	} else {
		if (net_ratelimit())
			netdev_warn(netdev,
				    "Recovery: dropping %u echo skb from index %u to %u\n",
				    rcv_packet_idx - priv->tx_tail,
				    priv->tx_tail, rcv_packet_idx - 1);
		while (priv->tx_tail != rcv_packet_idx) {
			if (priv->tx_tail == priv->tx_head)
				return -EINVAL;
			es58x_can_free_echo_skb_tail(netdev);
		}
	}
	return ret;
}

 
int es58x_can_get_echo_skb(struct net_device *netdev, u32 rcv_packet_idx,
			   u64 *tstamps, unsigned int pkts)
{
	struct es58x_priv *priv = es58x_priv(netdev);
	unsigned int rx_total_frame_len = 0;
	unsigned int num_echo_skb = priv->tx_head - priv->tx_tail;
	int i;
	u16 fifo_mask = priv->es58x_dev->param->fifo_mask;

	if (!netif_running(netdev)) {
		if (net_ratelimit())
			netdev_info(netdev,
				    "%s: %s is down, dropping %d echo packets\n",
				    __func__, netdev->name, pkts);
		netdev->stats.tx_dropped += pkts;
		return 0;
	} else if (!es58x_is_can_state_active(netdev)) {
		if (net_ratelimit())
			netdev_dbg(netdev,
				   "Bus is off or device is restarting. Ignoring %u echo packets from index %u\n",
				   pkts, rcv_packet_idx);
		 
		return 0;
	} else if (num_echo_skb == 0) {
		if (net_ratelimit())
			netdev_warn(netdev,
				    "Received %u echo packets from index: %u but echo skb queue is empty.\n",
				    pkts, rcv_packet_idx);
		netdev->stats.tx_dropped += pkts;
		return 0;
	}

	if (priv->tx_tail != rcv_packet_idx) {
		if (es58x_can_get_echo_skb_recovery(netdev, rcv_packet_idx) < 0) {
			if (net_ratelimit())
				netdev_warn(netdev,
					    "Could not find echo skb for echo packet index: %u\n",
					    rcv_packet_idx);
			return 0;
		}
	}
	if (num_echo_skb < pkts) {
		int pkts_drop = pkts - num_echo_skb;

		if (net_ratelimit())
			netdev_err(netdev,
				   "Received %u echo packets but have only %d echo skb. Dropping %d echo skb\n",
				   pkts, num_echo_skb, pkts_drop);
		netdev->stats.tx_dropped += pkts_drop;
		pkts -= pkts_drop;
	}

	for (i = 0; i < pkts; i++) {
		unsigned int skb_idx = priv->tx_tail & fifo_mask;
		struct sk_buff *skb = priv->can.echo_skb[skb_idx];
		unsigned int frame_len = 0;

		if (skb)
			es58x_set_skb_timestamp(netdev, skb, tstamps[i]);

		netdev->stats.tx_bytes += can_get_echo_skb(netdev, skb_idx,
							   &frame_len);
		rx_total_frame_len += frame_len;

		priv->tx_tail++;
	}

	netdev_completed_queue(netdev, pkts, rx_total_frame_len);
	netdev->stats.tx_packets += pkts;

	priv->err_passive_before_rtx_success = 0;
	if (!es58x_is_echo_skb_threshold_reached(priv))
		netif_wake_queue(netdev);

	return 0;
}

 
static void es58x_can_reset_echo_fifo(struct net_device *netdev)
{
	struct es58x_priv *priv = es58x_priv(netdev);

	priv->tx_tail = 0;
	priv->tx_head = 0;
	priv->tx_urb = NULL;
	priv->err_passive_before_rtx_success = 0;
	netdev_reset_queue(netdev);
}

 
static void es58x_flush_pending_tx_msg(struct net_device *netdev)
{
	struct es58x_priv *priv = es58x_priv(netdev);
	struct es58x_device *es58x_dev = priv->es58x_dev;

	if (priv->tx_urb) {
		netdev_warn(netdev, "%s: dropping %d TX messages\n",
			    __func__, priv->tx_can_msg_cnt);
		netdev->stats.tx_dropped += priv->tx_can_msg_cnt;
		while (priv->tx_can_msg_cnt > 0) {
			unsigned int frame_len = 0;
			u16 fifo_mask = priv->es58x_dev->param->fifo_mask;

			priv->tx_head--;
			priv->tx_can_msg_cnt--;
			can_free_echo_skb(netdev, priv->tx_head & fifo_mask,
					  &frame_len);
			netdev_completed_queue(netdev, 1, frame_len);
		}
		usb_anchor_urb(priv->tx_urb, &priv->es58x_dev->tx_urbs_idle);
		atomic_inc(&es58x_dev->tx_urbs_idle_cnt);
		usb_free_urb(priv->tx_urb);
	}
	priv->tx_urb = NULL;
}

 
int es58x_tx_ack_msg(struct net_device *netdev, u16 tx_free_entries,
		     enum es58x_ret_u32 rx_cmd_ret_u32)
{
	struct es58x_priv *priv = es58x_priv(netdev);

	if (tx_free_entries <= priv->es58x_dev->param->tx_bulk_max) {
		if (net_ratelimit())
			netdev_err(netdev,
				   "Only %d entries left in device queue, num_echo_skb: %d/%d\n",
				   tx_free_entries,
				   priv->tx_head - priv->tx_tail,
				   priv->can.echo_skb_max);
		netif_stop_queue(netdev);
	}

	return es58x_rx_cmd_ret_u32(netdev, ES58X_RET_TYPE_TX_MSG,
				    rx_cmd_ret_u32);
}

 
int es58x_rx_can_msg(struct net_device *netdev, u64 timestamp, const u8 *data,
		     canid_t can_id, enum es58x_flag es58x_flags, u8 dlc)
{
	struct canfd_frame *cfd;
	struct can_frame *ccf;
	struct sk_buff *skb;
	u8 len;
	bool is_can_fd = !!(es58x_flags & ES58X_FLAG_FD_DATA);

	if (dlc > CAN_MAX_RAW_DLC) {
		netdev_err(netdev,
			   "%s: DLC is %d but maximum should be %d\n",
			   __func__, dlc, CAN_MAX_RAW_DLC);
		return -EMSGSIZE;
	}

	if (is_can_fd) {
		len = can_fd_dlc2len(dlc);
		skb = alloc_canfd_skb(netdev, &cfd);
	} else {
		len = can_cc_dlc2len(dlc);
		skb = alloc_can_skb(netdev, &ccf);
		cfd = (struct canfd_frame *)ccf;
	}
	if (!skb) {
		netdev->stats.rx_dropped++;
		return 0;
	}

	cfd->can_id = can_id;
	if (es58x_flags & ES58X_FLAG_EFF)
		cfd->can_id |= CAN_EFF_FLAG;
	if (is_can_fd) {
		cfd->len = len;
		if (es58x_flags & ES58X_FLAG_FD_BRS)
			cfd->flags |= CANFD_BRS;
		if (es58x_flags & ES58X_FLAG_FD_ESI)
			cfd->flags |= CANFD_ESI;
	} else {
		can_frame_set_cc_len(ccf, dlc, es58x_priv(netdev)->can.ctrlmode);
		if (es58x_flags & ES58X_FLAG_RTR) {
			ccf->can_id |= CAN_RTR_FLAG;
			len = 0;
		}
	}
	memcpy(cfd->data, data, len);
	netdev->stats.rx_packets++;
	netdev->stats.rx_bytes += len;

	es58x_set_skb_timestamp(netdev, skb, timestamp);
	netif_rx(skb);

	es58x_priv(netdev)->err_passive_before_rtx_success = 0;

	return 0;
}

 
int es58x_rx_err_msg(struct net_device *netdev, enum es58x_err error,
		     enum es58x_event event, u64 timestamp)
{
	struct es58x_priv *priv = es58x_priv(netdev);
	struct can_priv *can = netdev_priv(netdev);
	struct can_device_stats *can_stats = &can->can_stats;
	struct can_frame *cf = NULL;
	struct sk_buff *skb;
	int ret = 0;

	if (!netif_running(netdev)) {
		if (net_ratelimit())
			netdev_info(netdev, "%s: %s is down, dropping packet\n",
				    __func__, netdev->name);
		netdev->stats.rx_dropped++;
		return 0;
	}

	if (error == ES58X_ERR_OK && event == ES58X_EVENT_OK) {
		netdev_err(netdev, "%s: Both error and event are zero\n",
			   __func__);
		return -EINVAL;
	}

	skb = alloc_can_err_skb(netdev, &cf);

	switch (error) {
	case ES58X_ERR_OK:	 
		break;

	case ES58X_ERR_PROT_STUFF:
		if (net_ratelimit())
			netdev_dbg(netdev, "Error BITSTUFF\n");
		if (cf)
			cf->data[2] |= CAN_ERR_PROT_STUFF;
		break;

	case ES58X_ERR_PROT_FORM:
		if (net_ratelimit())
			netdev_dbg(netdev, "Error FORMAT\n");
		if (cf)
			cf->data[2] |= CAN_ERR_PROT_FORM;
		break;

	case ES58X_ERR_ACK:
		if (net_ratelimit())
			netdev_dbg(netdev, "Error ACK\n");
		if (cf)
			cf->can_id |= CAN_ERR_ACK;
		break;

	case ES58X_ERR_PROT_BIT:
		if (net_ratelimit())
			netdev_dbg(netdev, "Error BIT\n");
		if (cf)
			cf->data[2] |= CAN_ERR_PROT_BIT;
		break;

	case ES58X_ERR_PROT_CRC:
		if (net_ratelimit())
			netdev_dbg(netdev, "Error CRC\n");
		if (cf)
			cf->data[3] |= CAN_ERR_PROT_LOC_CRC_SEQ;
		break;

	case ES58X_ERR_PROT_BIT1:
		if (net_ratelimit())
			netdev_dbg(netdev,
				   "Error: expected a recessive bit but monitored a dominant one\n");
		if (cf)
			cf->data[2] |= CAN_ERR_PROT_BIT1;
		break;

	case ES58X_ERR_PROT_BIT0:
		if (net_ratelimit())
			netdev_dbg(netdev,
				   "Error expected a dominant bit but monitored a recessive one\n");
		if (cf)
			cf->data[2] |= CAN_ERR_PROT_BIT0;
		break;

	case ES58X_ERR_PROT_OVERLOAD:
		if (net_ratelimit())
			netdev_dbg(netdev, "Error OVERLOAD\n");
		if (cf)
			cf->data[2] |= CAN_ERR_PROT_OVERLOAD;
		break;

	case ES58X_ERR_PROT_UNSPEC:
		if (net_ratelimit())
			netdev_dbg(netdev, "Unspecified error\n");
		if (cf)
			cf->can_id |= CAN_ERR_PROT;
		break;

	default:
		if (net_ratelimit())
			netdev_err(netdev,
				   "%s: Unspecified error code 0x%04X\n",
				   __func__, (int)error);
		if (cf)
			cf->can_id |= CAN_ERR_PROT;
		break;
	}

	switch (event) {
	case ES58X_EVENT_OK:	 
		break;

	case ES58X_EVENT_CRTL_ACTIVE:
		if (can->state == CAN_STATE_BUS_OFF) {
			netdev_err(netdev,
				   "%s: state transition: BUS OFF -> ACTIVE\n",
				   __func__);
		}
		if (net_ratelimit())
			netdev_dbg(netdev, "Event CAN BUS ACTIVE\n");
		if (cf)
			cf->data[1] |= CAN_ERR_CRTL_ACTIVE;
		can->state = CAN_STATE_ERROR_ACTIVE;
		break;

	case ES58X_EVENT_CRTL_PASSIVE:
		if (net_ratelimit())
			netdev_dbg(netdev, "Event CAN BUS PASSIVE\n");
		 
		if (cf) {
			cf->data[1] |= CAN_ERR_CRTL_RX_PASSIVE;
			cf->data[1] |= CAN_ERR_CRTL_TX_PASSIVE;
		}
		if (can->state < CAN_STATE_BUS_OFF)
			can->state = CAN_STATE_ERROR_PASSIVE;
		can_stats->error_passive++;
		if (priv->err_passive_before_rtx_success < U8_MAX)
			priv->err_passive_before_rtx_success++;
		break;

	case ES58X_EVENT_CRTL_WARNING:
		if (net_ratelimit())
			netdev_dbg(netdev, "Event CAN BUS WARNING\n");
		 
		if (cf) {
			cf->data[1] |= CAN_ERR_CRTL_RX_WARNING;
			cf->data[1] |= CAN_ERR_CRTL_TX_WARNING;
		}
		if (can->state < CAN_STATE_BUS_OFF)
			can->state = CAN_STATE_ERROR_WARNING;
		can_stats->error_warning++;
		break;

	case ES58X_EVENT_BUSOFF:
		if (net_ratelimit())
			netdev_dbg(netdev, "Event CAN BUS OFF\n");
		if (cf)
			cf->can_id |= CAN_ERR_BUSOFF;
		can_stats->bus_off++;
		netif_stop_queue(netdev);
		if (can->state != CAN_STATE_BUS_OFF) {
			can->state = CAN_STATE_BUS_OFF;
			can_bus_off(netdev);
			ret = can->do_set_mode(netdev, CAN_MODE_STOP);
		}
		break;

	case ES58X_EVENT_SINGLE_WIRE:
		if (net_ratelimit())
			netdev_warn(netdev,
				    "Lost connection on either CAN high or CAN low\n");
		 
		if (cf) {
			cf->data[4] |= CAN_ERR_TRX_CANH_NO_WIRE;
			cf->data[4] |= CAN_ERR_TRX_CANL_NO_WIRE;
		}
		break;

	default:
		if (net_ratelimit())
			netdev_err(netdev,
				   "%s: Unspecified event code 0x%04X\n",
				   __func__, (int)event);
		if (cf)
			cf->can_id |= CAN_ERR_CRTL;
		break;
	}

	if (cf) {
		if (cf->data[1])
			cf->can_id |= CAN_ERR_CRTL;
		if (cf->data[2] || cf->data[3]) {
			cf->can_id |= CAN_ERR_PROT;
			can_stats->bus_error++;
		}
		if (cf->data[4])
			cf->can_id |= CAN_ERR_TRX;

		es58x_set_skb_timestamp(netdev, skb, timestamp);
		netif_rx(skb);
	}

	if ((event & ES58X_EVENT_CRTL_PASSIVE) &&
	    priv->err_passive_before_rtx_success == ES58X_CONSECUTIVE_ERR_PASSIVE_MAX) {
		netdev_info(netdev,
			    "Got %d consecutive warning events with no successful RX or TX. Forcing bus-off\n",
			    priv->err_passive_before_rtx_success);
		return es58x_rx_err_msg(netdev, ES58X_ERR_OK,
					ES58X_EVENT_BUSOFF, timestamp);
	}

	return ret;
}

 
static const char *es58x_cmd_ret_desc(enum es58x_ret_type cmd_ret_type)
{
	switch (cmd_ret_type) {
	case ES58X_RET_TYPE_SET_BITTIMING:
		return "Set bittiming";
	case ES58X_RET_TYPE_ENABLE_CHANNEL:
		return "Enable channel";
	case ES58X_RET_TYPE_DISABLE_CHANNEL:
		return "Disable channel";
	case ES58X_RET_TYPE_TX_MSG:
		return "Transmit message";
	case ES58X_RET_TYPE_RESET_RX:
		return "Reset RX";
	case ES58X_RET_TYPE_RESET_TX:
		return "Reset TX";
	case ES58X_RET_TYPE_DEVICE_ERR:
		return "Device error";
	}

	return "<unknown>";
};

 
int es58x_rx_cmd_ret_u8(struct device *dev,
			enum es58x_ret_type cmd_ret_type,
			enum es58x_ret_u8 rx_cmd_ret_u8)
{
	const char *ret_desc = es58x_cmd_ret_desc(cmd_ret_type);

	switch (rx_cmd_ret_u8) {
	case ES58X_RET_U8_OK:
		dev_dbg_ratelimited(dev, "%s: OK\n", ret_desc);
		return 0;

	case ES58X_RET_U8_ERR_UNSPECIFIED_FAILURE:
		dev_err(dev, "%s: unspecified failure\n", ret_desc);
		return -EBADMSG;

	case ES58X_RET_U8_ERR_NO_MEM:
		dev_err(dev, "%s: device ran out of memory\n", ret_desc);
		return -ENOMEM;

	case ES58X_RET_U8_ERR_BAD_CRC:
		dev_err(dev, "%s: CRC of previous command is incorrect\n",
			ret_desc);
		return -EIO;

	default:
		dev_err(dev, "%s: returned unknown value: 0x%02X\n",
			ret_desc, rx_cmd_ret_u8);
		return -EBADMSG;
	}
}

 
int es58x_rx_cmd_ret_u32(struct net_device *netdev,
			 enum es58x_ret_type cmd_ret_type,
			 enum es58x_ret_u32 rx_cmd_ret_u32)
{
	struct es58x_priv *priv = es58x_priv(netdev);
	const struct es58x_operators *ops = priv->es58x_dev->ops;
	const char *ret_desc = es58x_cmd_ret_desc(cmd_ret_type);

	switch (rx_cmd_ret_u32) {
	case ES58X_RET_U32_OK:
		switch (cmd_ret_type) {
		case ES58X_RET_TYPE_ENABLE_CHANNEL:
			es58x_can_reset_echo_fifo(netdev);
			priv->can.state = CAN_STATE_ERROR_ACTIVE;
			netif_wake_queue(netdev);
			netdev_info(netdev,
				    "%s: %s (Serial Number %s): CAN%d channel becomes ready\n",
				    ret_desc, priv->es58x_dev->udev->product,
				    priv->es58x_dev->udev->serial,
				    priv->channel_idx + 1);
			break;

		case ES58X_RET_TYPE_TX_MSG:
			if (IS_ENABLED(CONFIG_VERBOSE_DEBUG) && net_ratelimit())
				netdev_vdbg(netdev, "%s: OK\n", ret_desc);
			break;

		default:
			netdev_dbg(netdev, "%s: OK\n", ret_desc);
			break;
		}
		return 0;

	case ES58X_RET_U32_ERR_UNSPECIFIED_FAILURE:
		if (cmd_ret_type == ES58X_RET_TYPE_ENABLE_CHANNEL) {
			int ret;

			netdev_warn(netdev,
				    "%s: channel is already opened, closing and re-opening it to reflect new configuration\n",
				    ret_desc);
			ret = ops->disable_channel(es58x_priv(netdev));
			if (ret)
				return ret;
			return ops->enable_channel(es58x_priv(netdev));
		}
		if (cmd_ret_type == ES58X_RET_TYPE_DISABLE_CHANNEL) {
			netdev_info(netdev,
				    "%s: channel is already closed\n", ret_desc);
			return 0;
		}
		netdev_err(netdev,
			   "%s: unspecified failure\n", ret_desc);
		return -EBADMSG;

	case ES58X_RET_U32_ERR_NO_MEM:
		netdev_err(netdev, "%s: device ran out of memory\n", ret_desc);
		return -ENOMEM;

	case ES58X_RET_U32_WARN_PARAM_ADJUSTED:
		netdev_warn(netdev,
			    "%s: some incompatible parameters have been adjusted\n",
			    ret_desc);
		return 0;

	case ES58X_RET_U32_WARN_TX_MAYBE_REORDER:
		netdev_warn(netdev,
			    "%s: TX messages might have been reordered\n",
			    ret_desc);
		return 0;

	case ES58X_RET_U32_ERR_TIMEDOUT:
		netdev_err(netdev, "%s: command timed out\n", ret_desc);
		return -ETIMEDOUT;

	case ES58X_RET_U32_ERR_FIFO_FULL:
		netdev_warn(netdev, "%s: fifo is full\n", ret_desc);
		return 0;

	case ES58X_RET_U32_ERR_BAD_CONFIG:
		netdev_err(netdev, "%s: bad configuration\n", ret_desc);
		return -EINVAL;

	case ES58X_RET_U32_ERR_NO_RESOURCE:
		netdev_err(netdev, "%s: no resource available\n", ret_desc);
		return -EBUSY;

	default:
		netdev_err(netdev, "%s returned unknown value: 0x%08X\n",
			   ret_desc, rx_cmd_ret_u32);
		return -EBADMSG;
	}
}

 
static void es58x_increment_rx_errors(struct es58x_device *es58x_dev)
{
	int i;

	for (i = 0; i < es58x_dev->num_can_ch; i++)
		if (es58x_dev->netdev[i])
			es58x_dev->netdev[i]->stats.rx_errors++;
}

 
static void es58x_handle_urb_cmd(struct es58x_device *es58x_dev,
				 const union es58x_urb_cmd *urb_cmd)
{
	const struct es58x_operators *ops = es58x_dev->ops;
	size_t cmd_len;
	int i, ret;

	ret = ops->handle_urb_cmd(es58x_dev, urb_cmd);
	switch (ret) {
	case 0:		 
		return;

	case -ENODEV:
		dev_err_ratelimited(es58x_dev->dev, "Device is not ready\n");
		break;

	case -EINVAL:
	case -EMSGSIZE:
	case -EBADRQC:
	case -EBADMSG:
	case -ECHRNG:
	case -ETIMEDOUT:
		cmd_len = es58x_get_urb_cmd_len(es58x_dev,
						ops->get_msg_len(urb_cmd));
		dev_err(es58x_dev->dev,
			"ops->handle_urb_cmd() returned error %pe",
			ERR_PTR(ret));
		es58x_print_hex_dump(urb_cmd, cmd_len);
		break;

	case -EFAULT:
	case -ENOMEM:
	case -EIO:
	default:
		dev_crit(es58x_dev->dev,
			 "ops->handle_urb_cmd() returned error %pe, detaching all network devices\n",
			 ERR_PTR(ret));
		for (i = 0; i < es58x_dev->num_can_ch; i++)
			if (es58x_dev->netdev[i])
				netif_device_detach(es58x_dev->netdev[i]);
		if (es58x_dev->ops->reset_device)
			es58x_dev->ops->reset_device(es58x_dev);
		break;
	}

	 
	es58x_increment_rx_errors(es58x_dev);
}

 
static signed int es58x_check_rx_urb(struct es58x_device *es58x_dev,
				     const union es58x_urb_cmd *urb_cmd,
				     u32 urb_actual_len)
{
	const struct device *dev = es58x_dev->dev;
	const struct es58x_parameters *param = es58x_dev->param;
	u16 sof, msg_len;
	signed int urb_cmd_len, ret;

	if (urb_actual_len < param->urb_cmd_header_len) {
		dev_vdbg(dev,
			 "%s: Received %d bytes [%*ph]: header incomplete\n",
			 __func__, urb_actual_len, urb_actual_len,
			 urb_cmd->raw_cmd);
		return -ENODATA;
	}

	sof = get_unaligned_le16(&urb_cmd->sof);
	if (sof != param->rx_start_of_frame) {
		dev_err_ratelimited(es58x_dev->dev,
				    "%s: Expected sequence 0x%04X for start of frame but got 0x%04X.\n",
				    __func__, param->rx_start_of_frame, sof);
		return -EBADRQC;
	}

	msg_len = es58x_dev->ops->get_msg_len(urb_cmd);
	urb_cmd_len = es58x_get_urb_cmd_len(es58x_dev, msg_len);
	if (urb_cmd_len > param->rx_urb_cmd_max_len) {
		dev_err_ratelimited(es58x_dev->dev,
				    "%s: Biggest expected size for rx urb_cmd is %u but receive a command of size %d\n",
				    __func__,
				    param->rx_urb_cmd_max_len, urb_cmd_len);
		return -EOVERFLOW;
	} else if (urb_actual_len < urb_cmd_len) {
		dev_vdbg(dev, "%s: Received %02d/%02d bytes\n",
			 __func__, urb_actual_len, urb_cmd_len);
		return -ENODATA;
	}

	ret = es58x_check_crc(es58x_dev, urb_cmd, urb_cmd_len);
	if (ret)
		return ret;

	return urb_cmd_len;
}

 
static int es58x_copy_to_cmd_buf(struct es58x_device *es58x_dev,
				 u8 *raw_cmd, int raw_cmd_len)
{
	if (es58x_dev->rx_cmd_buf_len + raw_cmd_len >
	    es58x_dev->param->rx_urb_cmd_max_len)
		return -EMSGSIZE;

	memcpy(&es58x_dev->rx_cmd_buf.raw_cmd[es58x_dev->rx_cmd_buf_len],
	       raw_cmd, raw_cmd_len);
	es58x_dev->rx_cmd_buf_len += raw_cmd_len;

	return 0;
}

 
static int es58x_split_urb_try_recovery(struct es58x_device *es58x_dev,
					u8 *raw_cmd, size_t raw_cmd_len)
{
	union es58x_urb_cmd *urb_cmd;
	signed int urb_cmd_len;
	u16 sof;
	int dropped_bytes = 0;

	es58x_increment_rx_errors(es58x_dev);

	while (raw_cmd_len > sizeof(sof)) {
		urb_cmd = (union es58x_urb_cmd *)raw_cmd;
		sof = get_unaligned_le16(&urb_cmd->sof);

		if (sof == es58x_dev->param->rx_start_of_frame) {
			urb_cmd_len = es58x_check_rx_urb(es58x_dev,
							 urb_cmd, raw_cmd_len);
			if ((urb_cmd_len == -ENODATA) || urb_cmd_len > 0) {
				dev_info_ratelimited(es58x_dev->dev,
						     "Recovery successful! Dropped %d bytes (urb_cmd_len: %d)\n",
						     dropped_bytes,
						     urb_cmd_len);
				return dropped_bytes;
			}
		}
		raw_cmd++;
		raw_cmd_len--;
		dropped_bytes++;
	}

	dev_warn_ratelimited(es58x_dev->dev, "%s: Recovery failed\n", __func__);
	return -EBADMSG;
}

 
static signed int es58x_handle_incomplete_cmd(struct es58x_device *es58x_dev,
					      struct urb *urb)
{
	size_t cpy_len;
	signed int urb_cmd_len, tmp_cmd_buf_len, ret;

	tmp_cmd_buf_len = es58x_dev->rx_cmd_buf_len;
	cpy_len = min_t(int, es58x_dev->param->rx_urb_cmd_max_len -
			es58x_dev->rx_cmd_buf_len, urb->actual_length);
	ret = es58x_copy_to_cmd_buf(es58x_dev, urb->transfer_buffer, cpy_len);
	if (ret < 0)
		return ret;

	urb_cmd_len = es58x_check_rx_urb(es58x_dev, &es58x_dev->rx_cmd_buf,
					 es58x_dev->rx_cmd_buf_len);
	if (urb_cmd_len == -ENODATA) {
		return -ENODATA;
	} else if (urb_cmd_len < 0) {
		dev_err_ratelimited(es58x_dev->dev,
				    "Could not reconstitute incomplete command from previous URB, dropping %d bytes\n",
				    tmp_cmd_buf_len + urb->actual_length);
		dev_err_ratelimited(es58x_dev->dev,
				    "Error code: %pe, es58x_dev->rx_cmd_buf_len: %d, urb->actual_length: %u\n",
				    ERR_PTR(urb_cmd_len),
				    tmp_cmd_buf_len, urb->actual_length);
		es58x_print_hex_dump(&es58x_dev->rx_cmd_buf, tmp_cmd_buf_len);
		es58x_print_hex_dump(urb->transfer_buffer, urb->actual_length);
		return urb->actual_length;
	}

	es58x_handle_urb_cmd(es58x_dev, &es58x_dev->rx_cmd_buf);
	return urb_cmd_len - tmp_cmd_buf_len;	 
}

 
static signed int es58x_split_urb(struct es58x_device *es58x_dev,
				  struct urb *urb)
{
	union es58x_urb_cmd *urb_cmd;
	u8 *raw_cmd = urb->transfer_buffer;
	s32 raw_cmd_len = urb->actual_length;
	int ret;

	if (es58x_dev->rx_cmd_buf_len != 0) {
		ret = es58x_handle_incomplete_cmd(es58x_dev, urb);
		if (ret != -ENODATA)
			es58x_dev->rx_cmd_buf_len = 0;
		if (ret < 0)
			return ret;

		raw_cmd += ret;
		raw_cmd_len -= ret;
	}

	while (raw_cmd_len > 0) {
		if (raw_cmd[0] == ES58X_HEARTBEAT) {
			raw_cmd++;
			raw_cmd_len--;
			continue;
		}
		urb_cmd = (union es58x_urb_cmd *)raw_cmd;
		ret = es58x_check_rx_urb(es58x_dev, urb_cmd, raw_cmd_len);
		if (ret > 0) {
			es58x_handle_urb_cmd(es58x_dev, urb_cmd);
		} else if (ret == -ENODATA) {
			es58x_copy_to_cmd_buf(es58x_dev, raw_cmd, raw_cmd_len);
			return -ENODATA;
		} else if (ret < 0) {
			ret = es58x_split_urb_try_recovery(es58x_dev, raw_cmd,
							   raw_cmd_len);
			if (ret < 0)
				return ret;
		}
		raw_cmd += ret;
		raw_cmd_len -= ret;
	}

	return 0;
}

 
static void es58x_read_bulk_callback(struct urb *urb)
{
	struct es58x_device *es58x_dev = urb->context;
	const struct device *dev = es58x_dev->dev;
	int i, ret;

	switch (urb->status) {
	case 0:		 
		break;

	case -EOVERFLOW:
		dev_err_ratelimited(dev, "%s: error %pe\n",
				    __func__, ERR_PTR(urb->status));
		es58x_print_hex_dump_debug(urb->transfer_buffer,
					   urb->transfer_buffer_length);
		goto resubmit_urb;

	case -EPROTO:
		dev_warn_ratelimited(dev, "%s: error %pe. Device unplugged?\n",
				     __func__, ERR_PTR(urb->status));
		goto free_urb;

	case -ENOENT:
	case -EPIPE:
		dev_err_ratelimited(dev, "%s: error %pe\n",
				    __func__, ERR_PTR(urb->status));
		goto free_urb;

	case -ESHUTDOWN:
		dev_dbg_ratelimited(dev, "%s: error %pe\n",
				    __func__, ERR_PTR(urb->status));
		goto free_urb;

	default:
		dev_err_ratelimited(dev, "%s: error %pe\n",
				    __func__, ERR_PTR(urb->status));
		goto resubmit_urb;
	}

	ret = es58x_split_urb(es58x_dev, urb);
	if ((ret != -ENODATA) && ret < 0) {
		dev_err(es58x_dev->dev, "es58x_split_urb() returned error %pe",
			ERR_PTR(ret));
		es58x_print_hex_dump_debug(urb->transfer_buffer,
					   urb->actual_length);

		 
		es58x_increment_rx_errors(es58x_dev);
	}

 resubmit_urb:
	ret = usb_submit_urb(urb, GFP_ATOMIC);
	if (ret == -ENODEV) {
		for (i = 0; i < es58x_dev->num_can_ch; i++)
			if (es58x_dev->netdev[i])
				netif_device_detach(es58x_dev->netdev[i]);
	} else if (ret)
		dev_err_ratelimited(dev,
				    "Failed resubmitting read bulk urb: %pe\n",
				    ERR_PTR(ret));
	return;

 free_urb:
	usb_free_coherent(urb->dev, urb->transfer_buffer_length,
			  urb->transfer_buffer, urb->transfer_dma);
}

 
static void es58x_write_bulk_callback(struct urb *urb)
{
	struct net_device *netdev = urb->context;
	struct es58x_device *es58x_dev = es58x_priv(netdev)->es58x_dev;

	switch (urb->status) {
	case 0:		 
		break;

	case -EOVERFLOW:
		if (net_ratelimit())
			netdev_err(netdev, "%s: error %pe\n",
				   __func__, ERR_PTR(urb->status));
		es58x_print_hex_dump(urb->transfer_buffer,
				     urb->transfer_buffer_length);
		break;

	case -ENOENT:
		if (net_ratelimit())
			netdev_dbg(netdev, "%s: error %pe\n",
				   __func__, ERR_PTR(urb->status));
		usb_free_coherent(urb->dev,
				  es58x_dev->param->tx_urb_cmd_max_len,
				  urb->transfer_buffer, urb->transfer_dma);
		return;

	default:
		if (net_ratelimit())
			netdev_info(netdev, "%s: error %pe\n",
				    __func__, ERR_PTR(urb->status));
		break;
	}

	usb_anchor_urb(urb, &es58x_dev->tx_urbs_idle);
	atomic_inc(&es58x_dev->tx_urbs_idle_cnt);
}

 
static int es58x_alloc_urb(struct es58x_device *es58x_dev, struct urb **urb,
			   u8 **buf, size_t buf_len, gfp_t mem_flags)
{
	*urb = usb_alloc_urb(0, mem_flags);
	if (!*urb) {
		dev_err(es58x_dev->dev, "No memory left for URBs\n");
		return -ENOMEM;
	}

	*buf = usb_alloc_coherent(es58x_dev->udev, buf_len,
				  mem_flags, &(*urb)->transfer_dma);
	if (!*buf) {
		dev_err(es58x_dev->dev, "No memory left for USB buffer\n");
		usb_free_urb(*urb);
		return -ENOMEM;
	}

	(*urb)->transfer_flags |= URB_NO_TRANSFER_DMA_MAP;

	return 0;
}

 
static struct urb *es58x_get_tx_urb(struct es58x_device *es58x_dev)
{
	atomic_t *idle_cnt = &es58x_dev->tx_urbs_idle_cnt;
	struct urb *urb = usb_get_from_anchor(&es58x_dev->tx_urbs_idle);

	if (!urb) {
		size_t tx_buf_len;
		u8 *buf;

		tx_buf_len = es58x_dev->param->tx_urb_cmd_max_len;
		if (es58x_alloc_urb(es58x_dev, &urb, &buf, tx_buf_len,
				    GFP_ATOMIC))
			return NULL;

		usb_fill_bulk_urb(urb, es58x_dev->udev, es58x_dev->tx_pipe,
				  buf, tx_buf_len, es58x_write_bulk_callback,
				  NULL);
		return urb;
	}

	while (atomic_dec_return(idle_cnt) > ES58X_TX_URBS_MAX) {
		 
		struct urb *tmp = usb_get_from_anchor(&es58x_dev->tx_urbs_idle);

		if (!tmp)
			break;
		usb_free_coherent(tmp->dev,
				  es58x_dev->param->tx_urb_cmd_max_len,
				  tmp->transfer_buffer, tmp->transfer_dma);
		usb_free_urb(tmp);
	}

	return urb;
}

 
static int es58x_submit_urb(struct es58x_device *es58x_dev, struct urb *urb,
			    struct net_device *netdev)
{
	int ret;

	es58x_set_crc(urb->transfer_buffer, urb->transfer_buffer_length);
	urb->context = netdev;
	usb_anchor_urb(urb, &es58x_dev->tx_urbs_busy);
	ret = usb_submit_urb(urb, GFP_ATOMIC);
	if (ret) {
		netdev_err(netdev, "%s: USB send urb failure: %pe\n",
			   __func__, ERR_PTR(ret));
		usb_unanchor_urb(urb);
		usb_free_coherent(urb->dev,
				  es58x_dev->param->tx_urb_cmd_max_len,
				  urb->transfer_buffer, urb->transfer_dma);
	}
	usb_free_urb(urb);

	return ret;
}

 
int es58x_send_msg(struct es58x_device *es58x_dev, u8 cmd_type, u8 cmd_id,
		   const void *msg, u16 msg_len, int channel_idx)
{
	struct net_device *netdev;
	union es58x_urb_cmd *urb_cmd;
	struct urb *urb;
	int urb_cmd_len;

	if (channel_idx == ES58X_CHANNEL_IDX_NA)
		netdev = es58x_dev->netdev[0];	 
	else
		netdev = es58x_dev->netdev[channel_idx];

	urb_cmd_len = es58x_get_urb_cmd_len(es58x_dev, msg_len);
	if (urb_cmd_len > es58x_dev->param->tx_urb_cmd_max_len)
		return -EOVERFLOW;

	urb = es58x_get_tx_urb(es58x_dev);
	if (!urb)
		return -ENOMEM;

	urb_cmd = urb->transfer_buffer;
	es58x_dev->ops->fill_urb_header(urb_cmd, cmd_type, cmd_id,
					channel_idx, msg_len);
	memcpy(&urb_cmd->raw_cmd[es58x_dev->param->urb_cmd_header_len],
	       msg, msg_len);
	urb->transfer_buffer_length = urb_cmd_len;

	return es58x_submit_urb(es58x_dev, urb, netdev);
}

 
static int es58x_alloc_rx_urbs(struct es58x_device *es58x_dev)
{
	const struct device *dev = es58x_dev->dev;
	const struct es58x_parameters *param = es58x_dev->param;
	u16 rx_buf_len = usb_maxpacket(es58x_dev->udev, es58x_dev->rx_pipe);
	struct urb *urb;
	u8 *buf;
	int i;
	int ret = -EINVAL;

	for (i = 0; i < param->rx_urb_max; i++) {
		ret = es58x_alloc_urb(es58x_dev, &urb, &buf, rx_buf_len,
				      GFP_KERNEL);
		if (ret)
			break;

		usb_fill_bulk_urb(urb, es58x_dev->udev, es58x_dev->rx_pipe,
				  buf, rx_buf_len, es58x_read_bulk_callback,
				  es58x_dev);
		usb_anchor_urb(urb, &es58x_dev->rx_urbs);

		ret = usb_submit_urb(urb, GFP_KERNEL);
		if (ret) {
			usb_unanchor_urb(urb);
			usb_free_coherent(es58x_dev->udev, rx_buf_len,
					  buf, urb->transfer_dma);
			usb_free_urb(urb);
			break;
		}
		usb_free_urb(urb);
	}

	if (i == 0) {
		dev_err(dev, "%s: Could not setup any rx URBs\n", __func__);
		return ret;
	}
	dev_dbg(dev, "%s: Allocated %d rx URBs each of size %u\n",
		__func__, i, rx_buf_len);

	return ret;
}

 
static void es58x_free_urbs(struct es58x_device *es58x_dev)
{
	struct urb *urb;

	if (!usb_wait_anchor_empty_timeout(&es58x_dev->tx_urbs_busy, 1000)) {
		dev_err(es58x_dev->dev, "%s: Timeout, some TX urbs still remain\n",
			__func__);
		usb_kill_anchored_urbs(&es58x_dev->tx_urbs_busy);
	}

	while ((urb = usb_get_from_anchor(&es58x_dev->tx_urbs_idle)) != NULL) {
		usb_free_coherent(urb->dev, es58x_dev->param->tx_urb_cmd_max_len,
				  urb->transfer_buffer, urb->transfer_dma);
		usb_free_urb(urb);
		atomic_dec(&es58x_dev->tx_urbs_idle_cnt);
	}
	if (atomic_read(&es58x_dev->tx_urbs_idle_cnt))
		dev_err(es58x_dev->dev,
			"All idle urbs were freed but tx_urb_idle_cnt is %d\n",
			atomic_read(&es58x_dev->tx_urbs_idle_cnt));

	usb_kill_anchored_urbs(&es58x_dev->rx_urbs);
}

 
static int es58x_open(struct net_device *netdev)
{
	struct es58x_device *es58x_dev = es58x_priv(netdev)->es58x_dev;
	int ret;

	if (!es58x_dev->opened_channel_cnt) {
		ret = es58x_alloc_rx_urbs(es58x_dev);
		if (ret)
			return ret;

		ret = es58x_set_realtime_diff_ns(es58x_dev);
		if (ret)
			goto free_urbs;
	}

	ret = open_candev(netdev);
	if (ret)
		goto free_urbs;

	ret = es58x_dev->ops->enable_channel(es58x_priv(netdev));
	if (ret)
		goto free_urbs;

	es58x_dev->opened_channel_cnt++;
	netif_start_queue(netdev);

	return ret;

 free_urbs:
	if (!es58x_dev->opened_channel_cnt)
		es58x_free_urbs(es58x_dev);
	netdev_err(netdev, "%s: Could not open the network device: %pe\n",
		   __func__, ERR_PTR(ret));

	return ret;
}

 
static int es58x_stop(struct net_device *netdev)
{
	struct es58x_priv *priv = es58x_priv(netdev);
	struct es58x_device *es58x_dev = priv->es58x_dev;
	int ret;

	netif_stop_queue(netdev);
	ret = es58x_dev->ops->disable_channel(priv);
	if (ret)
		return ret;

	priv->can.state = CAN_STATE_STOPPED;
	es58x_can_reset_echo_fifo(netdev);
	close_candev(netdev);

	es58x_flush_pending_tx_msg(netdev);

	es58x_dev->opened_channel_cnt--;
	if (!es58x_dev->opened_channel_cnt)
		es58x_free_urbs(es58x_dev);

	return 0;
}

 
static int es58x_xmit_commit(struct net_device *netdev)
{
	struct es58x_priv *priv = es58x_priv(netdev);
	int ret;

	if (!es58x_is_can_state_active(netdev))
		return -ENETDOWN;

	if (es58x_is_echo_skb_threshold_reached(priv))
		netif_stop_queue(netdev);

	ret = es58x_submit_urb(priv->es58x_dev, priv->tx_urb, netdev);
	if (ret == 0)
		priv->tx_urb = NULL;

	return ret;
}

 
static bool es58x_xmit_more(struct es58x_priv *priv)
{
	unsigned int free_slots =
	    priv->can.echo_skb_max - (priv->tx_head - priv->tx_tail);

	return netdev_xmit_more() && free_slots > 0 &&
		priv->tx_can_msg_cnt < priv->es58x_dev->param->tx_bulk_max;
}

 
static netdev_tx_t es58x_start_xmit(struct sk_buff *skb,
				    struct net_device *netdev)
{
	struct es58x_priv *priv = es58x_priv(netdev);
	struct es58x_device *es58x_dev = priv->es58x_dev;
	unsigned int frame_len;
	int ret;

	if (can_dev_dropped_skb(netdev, skb)) {
		if (priv->tx_urb)
			goto xmit_commit;
		return NETDEV_TX_OK;
	}

	if (priv->tx_urb && priv->tx_can_msg_is_fd != can_is_canfd_skb(skb)) {
		 
		ret = es58x_xmit_commit(netdev);
		if (ret)
			goto drop_skb;
	}

	if (!priv->tx_urb) {
		priv->tx_urb = es58x_get_tx_urb(es58x_dev);
		if (!priv->tx_urb) {
			ret = -ENOMEM;
			goto drop_skb;
		}
		priv->tx_can_msg_cnt = 0;
		priv->tx_can_msg_is_fd = can_is_canfd_skb(skb);
	}

	ret = es58x_dev->ops->tx_can_msg(priv, skb);
	if (ret)
		goto drop_skb;

	frame_len = can_skb_get_frame_len(skb);
	ret = can_put_echo_skb(skb, netdev,
			       priv->tx_head & es58x_dev->param->fifo_mask,
			       frame_len);
	if (ret)
		goto xmit_failure;
	netdev_sent_queue(netdev, frame_len);

	priv->tx_head++;
	priv->tx_can_msg_cnt++;

 xmit_commit:
	if (!es58x_xmit_more(priv)) {
		ret = es58x_xmit_commit(netdev);
		if (ret)
			goto xmit_failure;
	}

	return NETDEV_TX_OK;

 drop_skb:
	dev_kfree_skb(skb);
	netdev->stats.tx_dropped++;
 xmit_failure:
	netdev_warn(netdev, "%s: send message failure: %pe\n",
		    __func__, ERR_PTR(ret));
	netdev->stats.tx_errors++;
	es58x_flush_pending_tx_msg(netdev);
	return NETDEV_TX_OK;
}

static const struct net_device_ops es58x_netdev_ops = {
	.ndo_open = es58x_open,
	.ndo_stop = es58x_stop,
	.ndo_start_xmit = es58x_start_xmit,
	.ndo_eth_ioctl = can_eth_ioctl_hwts,
};

static const struct ethtool_ops es58x_ethtool_ops = {
	.get_ts_info = can_ethtool_op_get_ts_info_hwts,
};

 
static int es58x_set_mode(struct net_device *netdev, enum can_mode mode)
{
	struct es58x_priv *priv = es58x_priv(netdev);

	switch (mode) {
	case CAN_MODE_START:
		switch (priv->can.state) {
		case CAN_STATE_BUS_OFF:
			return priv->es58x_dev->ops->enable_channel(priv);

		case CAN_STATE_STOPPED:
			return es58x_open(netdev);

		case CAN_STATE_ERROR_ACTIVE:
		case CAN_STATE_ERROR_WARNING:
		case CAN_STATE_ERROR_PASSIVE:
		default:
			return 0;
		}

	case CAN_MODE_STOP:
		switch (priv->can.state) {
		case CAN_STATE_STOPPED:
			return 0;

		case CAN_STATE_ERROR_ACTIVE:
		case CAN_STATE_ERROR_WARNING:
		case CAN_STATE_ERROR_PASSIVE:
		case CAN_STATE_BUS_OFF:
		default:
			return priv->es58x_dev->ops->disable_channel(priv);
		}

	case CAN_MODE_SLEEP:
	default:
		return -EOPNOTSUPP;
	}
}

 
static int es58x_init_priv(struct es58x_device *es58x_dev,
			   struct es58x_priv *priv, int channel_idx)
{
	struct devlink_port_attrs attrs = {
		.flavour = DEVLINK_PORT_FLAVOUR_PHYSICAL,
	};
	const struct es58x_parameters *param = es58x_dev->param;
	struct can_priv *can = &priv->can;

	priv->es58x_dev = es58x_dev;
	priv->channel_idx = channel_idx;
	priv->tx_urb = NULL;
	priv->tx_can_msg_cnt = 0;

	can->bittiming_const = param->bittiming_const;
	if (param->ctrlmode_supported & CAN_CTRLMODE_FD) {
		can->data_bittiming_const = param->data_bittiming_const;
		can->tdc_const = param->tdc_const;
	}
	can->bitrate_max = param->bitrate_max;
	can->clock = param->clock;
	can->state = CAN_STATE_STOPPED;
	can->ctrlmode_supported = param->ctrlmode_supported;
	can->do_set_mode = es58x_set_mode;

	devlink_port_attrs_set(&priv->devlink_port, &attrs);
	return devlink_port_register(priv_to_devlink(es58x_dev),
				     &priv->devlink_port, channel_idx);
}

 
static int es58x_init_netdev(struct es58x_device *es58x_dev, int channel_idx)
{
	struct net_device *netdev;
	struct device *dev = es58x_dev->dev;
	int ret;

	netdev = alloc_candev(sizeof(struct es58x_priv),
			      es58x_dev->param->fifo_mask + 1);
	if (!netdev) {
		dev_err(dev, "Could not allocate candev\n");
		return -ENOMEM;
	}
	SET_NETDEV_DEV(netdev, dev);
	es58x_dev->netdev[channel_idx] = netdev;
	ret = es58x_init_priv(es58x_dev, es58x_priv(netdev), channel_idx);
	if (ret)
		goto free_candev;
	SET_NETDEV_DEVLINK_PORT(netdev, &es58x_priv(netdev)->devlink_port);

	netdev->netdev_ops = &es58x_netdev_ops;
	netdev->ethtool_ops = &es58x_ethtool_ops;
	netdev->flags |= IFF_ECHO;	 
	netdev->dev_port = channel_idx;

	ret = register_candev(netdev);
	if (ret)
		goto devlink_port_unregister;

	netdev_queue_set_dql_min_limit(netdev_get_tx_queue(netdev, 0),
				       es58x_dev->param->dql_min_limit);

	return ret;

 devlink_port_unregister:
	devlink_port_unregister(&es58x_priv(netdev)->devlink_port);
 free_candev:
	es58x_dev->netdev[channel_idx] = NULL;
	free_candev(netdev);
	return ret;
}

 
static void es58x_free_netdevs(struct es58x_device *es58x_dev)
{
	int i;

	for (i = 0; i < es58x_dev->num_can_ch; i++) {
		struct net_device *netdev = es58x_dev->netdev[i];

		if (!netdev)
			continue;
		unregister_candev(netdev);
		devlink_port_unregister(&es58x_priv(netdev)->devlink_port);
		es58x_dev->netdev[i] = NULL;
		free_candev(netdev);
	}
}

 
static struct es58x_device *es58x_init_es58x_dev(struct usb_interface *intf,
						 kernel_ulong_t driver_info)
{
	struct device *dev = &intf->dev;
	struct es58x_device *es58x_dev;
	struct devlink *devlink;
	const struct es58x_parameters *param;
	const struct es58x_operators *ops;
	struct usb_device *udev = interface_to_usbdev(intf);
	struct usb_endpoint_descriptor *ep_in, *ep_out;
	int ret;

	dev_info(dev, "Starting %s %s (Serial Number %s)\n",
		 udev->manufacturer, udev->product, udev->serial);

	ret = usb_find_common_endpoints(intf->cur_altsetting, &ep_in, &ep_out,
					NULL, NULL);
	if (ret)
		return ERR_PTR(ret);

	if (driver_info & ES58X_FD_FAMILY) {
		param = &es58x_fd_param;
		ops = &es58x_fd_ops;
	} else {
		param = &es581_4_param;
		ops = &es581_4_ops;
	}

	devlink = devlink_alloc(&es58x_dl_ops, es58x_sizeof_es58x_device(param),
				dev);
	if (!devlink)
		return ERR_PTR(-ENOMEM);

	es58x_dev = devlink_priv(devlink);
	es58x_dev->param = param;
	es58x_dev->ops = ops;
	es58x_dev->dev = dev;
	es58x_dev->udev = udev;

	if (driver_info & ES58X_DUAL_CHANNEL)
		es58x_dev->num_can_ch = 2;
	else
		es58x_dev->num_can_ch = 1;

	init_usb_anchor(&es58x_dev->rx_urbs);
	init_usb_anchor(&es58x_dev->tx_urbs_idle);
	init_usb_anchor(&es58x_dev->tx_urbs_busy);
	atomic_set(&es58x_dev->tx_urbs_idle_cnt, 0);
	usb_set_intfdata(intf, es58x_dev);

	es58x_dev->rx_pipe = usb_rcvbulkpipe(es58x_dev->udev,
					     ep_in->bEndpointAddress);
	es58x_dev->tx_pipe = usb_sndbulkpipe(es58x_dev->udev,
					     ep_out->bEndpointAddress);

	return es58x_dev;
}

 
static int es58x_probe(struct usb_interface *intf,
		       const struct usb_device_id *id)
{
	struct es58x_device *es58x_dev;
	int ch_idx;

	es58x_dev = es58x_init_es58x_dev(intf, id->driver_info);
	if (IS_ERR(es58x_dev))
		return PTR_ERR(es58x_dev);

	es58x_parse_product_info(es58x_dev);
	devlink_register(priv_to_devlink(es58x_dev));

	for (ch_idx = 0; ch_idx < es58x_dev->num_can_ch; ch_idx++) {
		int ret = es58x_init_netdev(es58x_dev, ch_idx);

		if (ret) {
			es58x_free_netdevs(es58x_dev);
			return ret;
		}
	}

	return 0;
}

 
static void es58x_disconnect(struct usb_interface *intf)
{
	struct es58x_device *es58x_dev = usb_get_intfdata(intf);

	dev_info(&intf->dev, "Disconnecting %s %s\n",
		 es58x_dev->udev->manufacturer, es58x_dev->udev->product);

	devlink_unregister(priv_to_devlink(es58x_dev));
	es58x_free_netdevs(es58x_dev);
	es58x_free_urbs(es58x_dev);
	devlink_free(priv_to_devlink(es58x_dev));
	usb_set_intfdata(intf, NULL);
}

static struct usb_driver es58x_driver = {
	.name = KBUILD_MODNAME,
	.probe = es58x_probe,
	.disconnect = es58x_disconnect,
	.id_table = es58x_id_table
};

module_usb_driver(es58x_driver);
