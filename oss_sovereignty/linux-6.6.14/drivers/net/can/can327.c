
 

#define pr_fmt(fmt) KBUILD_MODNAME ": " fmt

#include <linux/init.h>
#include <linux/module.h>

#include <linux/bitops.h>
#include <linux/ctype.h>
#include <linux/errno.h>
#include <linux/kernel.h>
#include <linux/list.h>
#include <linux/lockdep.h>
#include <linux/netdevice.h>
#include <linux/skbuff.h>
#include <linux/spinlock.h>
#include <linux/string.h>
#include <linux/tty.h>
#include <linux/tty_ldisc.h>
#include <linux/workqueue.h>

#include <uapi/linux/tty.h>

#include <linux/can.h>
#include <linux/can/dev.h>
#include <linux/can/error.h>
#include <linux/can/rx-offload.h>

#define CAN327_NAPI_WEIGHT 4

#define CAN327_SIZE_TXBUF 32
#define CAN327_SIZE_RXBUF 1024

#define CAN327_CAN_CONFIG_SEND_SFF 0x8000
#define CAN327_CAN_CONFIG_VARIABLE_DLC 0x4000
#define CAN327_CAN_CONFIG_RECV_BOTH_SFF_EFF 0x2000
#define CAN327_CAN_CONFIG_BAUDRATE_MULT_8_7 0x1000

#define CAN327_DUMMY_CHAR 'y'
#define CAN327_DUMMY_STRING "y"
#define CAN327_READY_CHAR '>'

 
enum can327_tx_do {
	CAN327_TX_DO_CAN_DATA = 0,
	CAN327_TX_DO_CANID_11BIT,
	CAN327_TX_DO_CANID_29BIT_LOW,
	CAN327_TX_DO_CANID_29BIT_HIGH,
	CAN327_TX_DO_CAN_CONFIG_PART2,
	CAN327_TX_DO_CAN_CONFIG,
	CAN327_TX_DO_RESPONSES,
	CAN327_TX_DO_SILENT_MONITOR,
	CAN327_TX_DO_INIT,
};

struct can327 {
	 
	struct can_priv can;

	struct can_rx_offload offload;

	 
	u8 txbuf[CAN327_SIZE_TXBUF];
	u8 rxbuf[CAN327_SIZE_RXBUF];

	 
	spinlock_t lock;

	 
	struct tty_struct *tty;
	struct net_device *dev;

	 
	struct work_struct tx_work;	 
	u8 *txhead;			 
	size_t txleft;			 
	int rxfill;			 

	 
	enum {
		CAN327_STATE_NOTINIT = 0,
		CAN327_STATE_GETDUMMYCHAR,
		CAN327_STATE_GETPROMPT,
		CAN327_STATE_RECEIVING,
	} state;

	 
	char **next_init_cmd;
	unsigned long cmds_todo;

	 
	struct can_frame can_frame_to_send;
	u16 can_config;
	u8 can_bitrate_divisor;

	 
	bool drop_next_line;

	 
	bool uart_side_failure;
};

static inline void can327_uart_side_failure(struct can327 *elm);

static void can327_send(struct can327 *elm, const void *buf, size_t len)
{
	int written;

	lockdep_assert_held(&elm->lock);

	if (elm->uart_side_failure)
		return;

	memcpy(elm->txbuf, buf, len);

	 
	set_bit(TTY_DO_WRITE_WAKEUP, &elm->tty->flags);
	written = elm->tty->ops->write(elm->tty, elm->txbuf, len);
	if (written < 0) {
		netdev_err(elm->dev, "Failed to write to tty %s.\n",
			   elm->tty->name);
		can327_uart_side_failure(elm);
		return;
	}

	elm->txleft = len - written;
	elm->txhead = elm->txbuf + written;
}

 
static void can327_kick_into_cmd_mode(struct can327 *elm)
{
	lockdep_assert_held(&elm->lock);

	if (elm->state != CAN327_STATE_GETDUMMYCHAR &&
	    elm->state != CAN327_STATE_GETPROMPT) {
		can327_send(elm, CAN327_DUMMY_STRING, 1);

		elm->state = CAN327_STATE_GETDUMMYCHAR;
	}
}

 
static void can327_send_frame(struct can327 *elm, struct can_frame *frame)
{
	lockdep_assert_held(&elm->lock);

	 
	if (elm->can_frame_to_send.can_id != frame->can_id) {
		 
		if ((frame->can_id ^ elm->can_frame_to_send.can_id)
		    & CAN_EFF_FLAG) {
			elm->can_config =
				(frame->can_id & CAN_EFF_FLAG ? 0 : CAN327_CAN_CONFIG_SEND_SFF) |
				CAN327_CAN_CONFIG_VARIABLE_DLC |
				CAN327_CAN_CONFIG_RECV_BOTH_SFF_EFF |
				elm->can_bitrate_divisor;

			set_bit(CAN327_TX_DO_CAN_CONFIG, &elm->cmds_todo);
		}

		if (frame->can_id & CAN_EFF_FLAG) {
			clear_bit(CAN327_TX_DO_CANID_11BIT, &elm->cmds_todo);
			set_bit(CAN327_TX_DO_CANID_29BIT_LOW, &elm->cmds_todo);
			set_bit(CAN327_TX_DO_CANID_29BIT_HIGH, &elm->cmds_todo);
		} else {
			set_bit(CAN327_TX_DO_CANID_11BIT, &elm->cmds_todo);
			clear_bit(CAN327_TX_DO_CANID_29BIT_LOW,
				  &elm->cmds_todo);
			clear_bit(CAN327_TX_DO_CANID_29BIT_HIGH,
				  &elm->cmds_todo);
		}
	}

	 
	elm->can_frame_to_send = *frame;
	set_bit(CAN327_TX_DO_CAN_DATA, &elm->cmds_todo);

	can327_kick_into_cmd_mode(elm);
}

 
static char *can327_init_script[] = {
	"AT WS\r",         
	"AT PP FF OFF\r",  
	"AT M0\r",         
	"AT AL\r",         
	"AT BI\r",         
	"AT CAF0\r",       
	"AT CFC0\r",       
	"AT CF 000\r",     
	"AT CM 000\r",     
	"AT E1\r",         
	"AT H1\r",         
	"AT L0\r",         
	"AT SH 7DF\r",     
	"AT ST FF\r",      
	"AT AT0\r",        
	"AT D1\r",         
	"AT S1\r",         
	"AT TP B\r",       
	NULL
};

static void can327_init_device(struct can327 *elm)
{
	lockdep_assert_held(&elm->lock);

	elm->state = CAN327_STATE_NOTINIT;
	elm->can_frame_to_send.can_id = 0x7df;  
	elm->rxfill = 0;
	elm->drop_next_line = 0;

	 
	elm->can_bitrate_divisor = 500000 / elm->can.bittiming.bitrate;
	elm->can_config =
		CAN327_CAN_CONFIG_SEND_SFF | CAN327_CAN_CONFIG_VARIABLE_DLC |
		CAN327_CAN_CONFIG_RECV_BOTH_SFF_EFF | elm->can_bitrate_divisor;

	 
	elm->next_init_cmd = &can327_init_script[0];
	set_bit(CAN327_TX_DO_INIT, &elm->cmds_todo);
	set_bit(CAN327_TX_DO_SILENT_MONITOR, &elm->cmds_todo);
	set_bit(CAN327_TX_DO_RESPONSES, &elm->cmds_todo);
	set_bit(CAN327_TX_DO_CAN_CONFIG, &elm->cmds_todo);

	can327_kick_into_cmd_mode(elm);
}

static void can327_feed_frame_to_netdev(struct can327 *elm, struct sk_buff *skb)
{
	lockdep_assert_held(&elm->lock);

	if (!netif_running(elm->dev)) {
		kfree_skb(skb);
		return;
	}

	 
	if (can_rx_offload_queue_tail(&elm->offload, skb))
		elm->dev->stats.rx_fifo_errors++;

	 
	can_rx_offload_irq_finish(&elm->offload);
}

 
static inline void can327_uart_side_failure(struct can327 *elm)
{
	struct can_frame *frame;
	struct sk_buff *skb;

	lockdep_assert_held(&elm->lock);

	elm->uart_side_failure = true;

	clear_bit(TTY_DO_WRITE_WAKEUP, &elm->tty->flags);

	elm->can.can_stats.bus_off++;
	netif_stop_queue(elm->dev);
	elm->can.state = CAN_STATE_BUS_OFF;
	can_bus_off(elm->dev);

	netdev_err(elm->dev,
		   "ELM327 misbehaved. Blocking further communication.\n");

	skb = alloc_can_err_skb(elm->dev, &frame);
	if (!skb)
		return;

	frame->can_id |= CAN_ERR_BUSOFF;
	can327_feed_frame_to_netdev(elm, skb);
}

 
static inline bool can327_rxbuf_cmp(const u8 *buf, size_t nbytes,
				    const char *reference)
{
	size_t ref_len = strlen(reference);

	return (nbytes == ref_len) && !memcmp(buf, reference, ref_len);
}

static void can327_parse_error(struct can327 *elm, size_t len)
{
	struct can_frame *frame;
	struct sk_buff *skb;

	lockdep_assert_held(&elm->lock);

	skb = alloc_can_err_skb(elm->dev, &frame);
	if (!skb)
		 
		return;

	 
	if (can327_rxbuf_cmp(elm->rxbuf, len, "UNABLE TO CONNECT")) {
		netdev_err(elm->dev,
			   "ELM327 reported UNABLE TO CONNECT. Please check your setup.\n");
	} else if (can327_rxbuf_cmp(elm->rxbuf, len, "BUFFER FULL")) {
		 
		frame->can_id |= CAN_ERR_CRTL;
		frame->data[1] = CAN_ERR_CRTL_RX_OVERFLOW;
	} else if (can327_rxbuf_cmp(elm->rxbuf, len, "BUS ERROR")) {
		frame->can_id |= CAN_ERR_BUSERROR;
	} else if (can327_rxbuf_cmp(elm->rxbuf, len, "CAN ERROR")) {
		frame->can_id |= CAN_ERR_PROT;
	} else if (can327_rxbuf_cmp(elm->rxbuf, len, "<RX ERROR")) {
		frame->can_id |= CAN_ERR_PROT;
	} else if (can327_rxbuf_cmp(elm->rxbuf, len, "BUS BUSY")) {
		frame->can_id |= CAN_ERR_PROT;
		frame->data[2] = CAN_ERR_PROT_OVERLOAD;
	} else if (can327_rxbuf_cmp(elm->rxbuf, len, "FB ERROR")) {
		frame->can_id |= CAN_ERR_PROT;
		frame->data[2] = CAN_ERR_PROT_TX;
	} else if (len == 5 && !memcmp(elm->rxbuf, "ERR", 3)) {
		 
		netdev_err(elm->dev, "ELM327 reported an ERR%c%c. Please power it off and on again.\n",
			   elm->rxbuf[3], elm->rxbuf[4]);
		frame->can_id |= CAN_ERR_CRTL;
	} else {
		 
	}

	can327_feed_frame_to_netdev(elm, skb);
}

 
static int can327_parse_frame(struct can327 *elm, size_t len)
{
	struct can_frame *frame;
	struct sk_buff *skb;
	int hexlen;
	int datastart;
	int i;

	lockdep_assert_held(&elm->lock);

	skb = alloc_can_skb(elm->dev, &frame);
	if (!skb)
		return -ENOMEM;

	 
	for (hexlen = 0; hexlen <= len; hexlen++) {
		if (hex_to_bin(elm->rxbuf[hexlen]) < 0 &&
		    elm->rxbuf[hexlen] != ' ') {
			break;
		}
	}

	 
	if (hexlen < len && !isdigit(elm->rxbuf[hexlen]) &&
	    !isupper(elm->rxbuf[hexlen]) && '<' != elm->rxbuf[hexlen] &&
	    ' ' != elm->rxbuf[hexlen]) {
		 
		kfree_skb(skb);
		return -ENODATA;
	}

	 
	if (elm->rxbuf[2] == ' ' && elm->rxbuf[5] == ' ' &&
	    elm->rxbuf[8] == ' ' && elm->rxbuf[11] == ' ' &&
	    elm->rxbuf[13] == ' ') {
		frame->can_id = CAN_EFF_FLAG;
		datastart = 14;
	} else if (elm->rxbuf[3] == ' ' && elm->rxbuf[5] == ' ') {
		datastart = 6;
	} else {
		 
		kfree_skb(skb);
		return -ENODATA;
	}

	if (hexlen < datastart) {
		 
		kfree_skb(skb);
		return -ENODATA;
	}

	 

	 
	frame->len = (hex_to_bin(elm->rxbuf[datastart - 2]) << 0);

	 
	if (frame->can_id & CAN_EFF_FLAG) {
		frame->can_id |= (hex_to_bin(elm->rxbuf[0]) << 28) |
				 (hex_to_bin(elm->rxbuf[1]) << 24) |
				 (hex_to_bin(elm->rxbuf[3]) << 20) |
				 (hex_to_bin(elm->rxbuf[4]) << 16) |
				 (hex_to_bin(elm->rxbuf[6]) << 12) |
				 (hex_to_bin(elm->rxbuf[7]) << 8) |
				 (hex_to_bin(elm->rxbuf[9]) << 4) |
				 (hex_to_bin(elm->rxbuf[10]) << 0);
	} else {
		frame->can_id |= (hex_to_bin(elm->rxbuf[0]) << 8) |
				 (hex_to_bin(elm->rxbuf[1]) << 4) |
				 (hex_to_bin(elm->rxbuf[2]) << 0);
	}

	 
	if (elm->rxfill >= hexlen + 3 &&
	    !memcmp(&elm->rxbuf[hexlen], "RTR", 3)) {
		frame->can_id |= CAN_RTR_FLAG;
	}

	 
	if (!(frame->can_id & CAN_RTR_FLAG) &&
	    (hexlen < frame->len * 3 + datastart)) {
		 
		frame->can_id = CAN_ERR_FLAG | CAN_ERR_CRTL;
		frame->len = CAN_ERR_DLC;
		frame->data[1] = CAN_ERR_CRTL_RX_OVERFLOW;
		can327_feed_frame_to_netdev(elm, skb);

		 
		return -ENODATA;
	}

	 
	for (i = 0; i < frame->len; i++) {
		frame->data[i] =
			(hex_to_bin(elm->rxbuf[datastart + 3 * i]) << 4) |
			(hex_to_bin(elm->rxbuf[datastart + 3 * i + 1]));
	}

	 
	can327_feed_frame_to_netdev(elm, skb);

	return 0;
}

static void can327_parse_line(struct can327 *elm, size_t len)
{
	lockdep_assert_held(&elm->lock);

	 
	if (!len)
		return;

	 
	if (elm->drop_next_line) {
		elm->drop_next_line = 0;
		return;
	} else if (!memcmp(elm->rxbuf, "AT", 2)) {
		return;
	}

	 
	if (elm->state == CAN327_STATE_RECEIVING &&
	    can327_parse_frame(elm, len)) {
		 
		can327_parse_error(elm, len);

		 
		can327_kick_into_cmd_mode(elm);
	}
}

static void can327_handle_prompt(struct can327 *elm)
{
	struct can_frame *frame = &elm->can_frame_to_send;
	 
	char local_txbuf[sizeof("0102030405060708\r")];

	lockdep_assert_held(&elm->lock);

	if (!elm->cmds_todo) {
		 
		can327_send(elm, "ATMA\r", 5);
		elm->state = CAN327_STATE_RECEIVING;

		 
		netif_wake_queue(elm->dev);

		return;
	}

	 
	if (test_bit(CAN327_TX_DO_INIT, &elm->cmds_todo)) {
		snprintf(local_txbuf, sizeof(local_txbuf), "%s",
			 *elm->next_init_cmd);

		elm->next_init_cmd++;
		if (!(*elm->next_init_cmd)) {
			clear_bit(CAN327_TX_DO_INIT, &elm->cmds_todo);
			 
		}

	} else if (test_and_clear_bit(CAN327_TX_DO_SILENT_MONITOR, &elm->cmds_todo)) {
		snprintf(local_txbuf, sizeof(local_txbuf),
			 "ATCSM%i\r",
			 !!(elm->can.ctrlmode & CAN_CTRLMODE_LISTENONLY));

	} else if (test_and_clear_bit(CAN327_TX_DO_RESPONSES, &elm->cmds_todo)) {
		snprintf(local_txbuf, sizeof(local_txbuf),
			 "ATR%i\r",
			 !(elm->can.ctrlmode & CAN_CTRLMODE_LISTENONLY));

	} else if (test_and_clear_bit(CAN327_TX_DO_CAN_CONFIG, &elm->cmds_todo)) {
		snprintf(local_txbuf, sizeof(local_txbuf),
			 "ATPC\r");
		set_bit(CAN327_TX_DO_CAN_CONFIG_PART2, &elm->cmds_todo);

	} else if (test_and_clear_bit(CAN327_TX_DO_CAN_CONFIG_PART2, &elm->cmds_todo)) {
		snprintf(local_txbuf, sizeof(local_txbuf),
			 "ATPB%04X\r",
			 elm->can_config);

	} else if (test_and_clear_bit(CAN327_TX_DO_CANID_29BIT_HIGH, &elm->cmds_todo)) {
		snprintf(local_txbuf, sizeof(local_txbuf),
			 "ATCP%02X\r",
			 (frame->can_id & CAN_EFF_MASK) >> 24);

	} else if (test_and_clear_bit(CAN327_TX_DO_CANID_29BIT_LOW, &elm->cmds_todo)) {
		snprintf(local_txbuf, sizeof(local_txbuf),
			 "ATSH%06X\r",
			 frame->can_id & CAN_EFF_MASK & ((1 << 24) - 1));

	} else if (test_and_clear_bit(CAN327_TX_DO_CANID_11BIT, &elm->cmds_todo)) {
		snprintf(local_txbuf, sizeof(local_txbuf),
			 "ATSH%03X\r",
			 frame->can_id & CAN_SFF_MASK);

	} else if (test_and_clear_bit(CAN327_TX_DO_CAN_DATA, &elm->cmds_todo)) {
		if (frame->can_id & CAN_RTR_FLAG) {
			 
			snprintf(local_txbuf, sizeof(local_txbuf), "ATRTR\r");
		} else {
			 
			int i;

			for (i = 0; i < frame->len; i++) {
				snprintf(&local_txbuf[2 * i],
					 sizeof(local_txbuf), "%02X",
					 frame->data[i]);
			}

			snprintf(&local_txbuf[2 * i], sizeof(local_txbuf),
				 "\r");
		}

		elm->drop_next_line = 1;
		elm->state = CAN327_STATE_RECEIVING;

		 
		netif_wake_queue(elm->dev);
	}

	can327_send(elm, local_txbuf, strlen(local_txbuf));
}

static bool can327_is_ready_char(char c)
{
	 
	return (c & 0x3f) == CAN327_READY_CHAR;
}

static void can327_drop_bytes(struct can327 *elm, size_t i)
{
	lockdep_assert_held(&elm->lock);

	memmove(&elm->rxbuf[0], &elm->rxbuf[i], CAN327_SIZE_RXBUF - i);
	elm->rxfill -= i;
}

static void can327_parse_rxbuf(struct can327 *elm, size_t first_new_char_idx)
{
	size_t len, pos;

	lockdep_assert_held(&elm->lock);

	switch (elm->state) {
	case CAN327_STATE_NOTINIT:
		elm->rxfill = 0;
		break;

	case CAN327_STATE_GETDUMMYCHAR:
		 
		for (pos = 0; pos < elm->rxfill; pos++) {
			if (elm->rxbuf[pos] == CAN327_DUMMY_CHAR) {
				can327_send(elm, "\r", 1);
				elm->state = CAN327_STATE_GETPROMPT;
				pos++;
				break;
			} else if (can327_is_ready_char(elm->rxbuf[pos])) {
				can327_send(elm, CAN327_DUMMY_STRING, 1);
				pos++;
				break;
			}
		}

		can327_drop_bytes(elm, pos);
		break;

	case CAN327_STATE_GETPROMPT:
		 
		if (can327_is_ready_char(elm->rxbuf[elm->rxfill - 1]))
			can327_handle_prompt(elm);

		elm->rxfill = 0;
		break;

	case CAN327_STATE_RECEIVING:
		 
		len = first_new_char_idx;
		while (len < elm->rxfill && elm->rxbuf[len] != '\r')
			len++;

		if (len == CAN327_SIZE_RXBUF) {
			 
			netdev_err(elm->dev,
				   "RX buffer overflow. Faulty ELM327 or UART?\n");
			can327_uart_side_failure(elm);
		} else if (len == elm->rxfill) {
			if (can327_is_ready_char(elm->rxbuf[elm->rxfill - 1])) {
				 
				elm->rxfill = 0;

				can327_handle_prompt(elm);
			}

			 
		} else {
			 
			can327_parse_line(elm, len);

			 
			can327_drop_bytes(elm, len + 1);

			 
			if (elm->rxfill)
				can327_parse_rxbuf(elm, 0);
		}
	}
}

static int can327_netdev_open(struct net_device *dev)
{
	struct can327 *elm = netdev_priv(dev);
	int err;

	spin_lock_bh(&elm->lock);

	if (!elm->tty) {
		spin_unlock_bh(&elm->lock);
		return -ENODEV;
	}

	if (elm->uart_side_failure)
		netdev_warn(elm->dev,
			    "Reopening netdev after a UART side fault has been detected.\n");

	 
	elm->rxfill = 0;
	elm->txleft = 0;

	 
	err = open_candev(dev);
	if (err) {
		spin_unlock_bh(&elm->lock);
		return err;
	}

	can327_init_device(elm);
	spin_unlock_bh(&elm->lock);

	err = can_rx_offload_add_manual(dev, &elm->offload, CAN327_NAPI_WEIGHT);
	if (err) {
		close_candev(dev);
		return err;
	}

	can_rx_offload_enable(&elm->offload);

	elm->can.state = CAN_STATE_ERROR_ACTIVE;
	netif_start_queue(dev);

	return 0;
}

static int can327_netdev_close(struct net_device *dev)
{
	struct can327 *elm = netdev_priv(dev);

	 
	spin_lock_bh(&elm->lock);
	can327_send(elm, CAN327_DUMMY_STRING, 1);
	spin_unlock_bh(&elm->lock);

	netif_stop_queue(dev);

	 

	can_rx_offload_disable(&elm->offload);
	elm->can.state = CAN_STATE_STOPPED;
	can_rx_offload_del(&elm->offload);
	close_candev(dev);

	return 0;
}

 
static netdev_tx_t can327_netdev_start_xmit(struct sk_buff *skb,
					    struct net_device *dev)
{
	struct can327 *elm = netdev_priv(dev);
	struct can_frame *frame = (struct can_frame *)skb->data;

	if (can_dev_dropped_skb(dev, skb))
		return NETDEV_TX_OK;

	 
	if (elm->uart_side_failure) {
		WARN_ON_ONCE(elm->uart_side_failure);
		goto out;
	}

	netif_stop_queue(dev);

	 
	spin_lock(&elm->lock);
	can327_send_frame(elm, frame);
	spin_unlock(&elm->lock);

	dev->stats.tx_packets++;
	dev->stats.tx_bytes += frame->can_id & CAN_RTR_FLAG ? 0 : frame->len;

	skb_tx_timestamp(skb);

out:
	kfree_skb(skb);
	return NETDEV_TX_OK;
}

static const struct net_device_ops can327_netdev_ops = {
	.ndo_open = can327_netdev_open,
	.ndo_stop = can327_netdev_close,
	.ndo_start_xmit = can327_netdev_start_xmit,
	.ndo_change_mtu = can_change_mtu,
};

static const struct ethtool_ops can327_ethtool_ops = {
	.get_ts_info = ethtool_op_get_ts_info,
};

static bool can327_is_valid_rx_char(u8 c)
{
	static const bool lut_char_is_valid['z'] = {
		['\r'] = true,
		[' '] = true,
		['.'] = true,
		['0'] = true, true, true, true, true,
		['5'] = true, true, true, true, true,
		['<'] = true,
		[CAN327_READY_CHAR] = true,
		['?'] = true,
		['A'] = true, true, true, true, true, true, true,
		['H'] = true, true, true, true, true, true, true,
		['O'] = true, true, true, true, true, true, true,
		['V'] = true, true, true, true, true,
		['a'] = true,
		['b'] = true,
		['v'] = true,
		[CAN327_DUMMY_CHAR] = true,
	};
	BUILD_BUG_ON(CAN327_DUMMY_CHAR >= 'z');

	return (c < ARRAY_SIZE(lut_char_is_valid) && lut_char_is_valid[c]);
}

 
static void can327_ldisc_rx(struct tty_struct *tty, const u8 *cp,
			    const u8 *fp, size_t count)
{
	struct can327 *elm = tty->disc_data;
	size_t first_new_char_idx;

	if (elm->uart_side_failure)
		return;

	spin_lock_bh(&elm->lock);

	 
	first_new_char_idx = elm->rxfill;

	while (count--) {
		if (elm->rxfill >= CAN327_SIZE_RXBUF) {
			netdev_err(elm->dev,
				   "Receive buffer overflowed. Bad chip or wiring? count = %zu",
				   count);
			goto uart_failure;
		}
		if (fp && *fp++) {
			netdev_err(elm->dev,
				   "Error in received character stream. Check your wiring.");
			goto uart_failure;
		}

		 
		if (*cp) {
			 
			if (!can327_is_valid_rx_char(*cp)) {
				netdev_err(elm->dev,
					   "Received illegal character %02x.\n",
					   *cp);
				goto uart_failure;
			}

			elm->rxbuf[elm->rxfill++] = *cp;
		}

		cp++;
	}

	can327_parse_rxbuf(elm, first_new_char_idx);
	spin_unlock_bh(&elm->lock);

	return;
uart_failure:
	can327_uart_side_failure(elm);
	spin_unlock_bh(&elm->lock);
}

 
static void can327_ldisc_tx_worker(struct work_struct *work)
{
	struct can327 *elm = container_of(work, struct can327, tx_work);
	ssize_t written;

	if (elm->uart_side_failure)
		return;

	spin_lock_bh(&elm->lock);

	if (elm->txleft) {
		written = elm->tty->ops->write(elm->tty, elm->txhead,
					       elm->txleft);
		if (written < 0) {
			netdev_err(elm->dev, "Failed to write to tty %s.\n",
				   elm->tty->name);
			can327_uart_side_failure(elm);

			spin_unlock_bh(&elm->lock);
			return;
		}

		elm->txleft -= written;
		elm->txhead += written;
	}

	if (!elm->txleft)
		clear_bit(TTY_DO_WRITE_WAKEUP, &elm->tty->flags);

	spin_unlock_bh(&elm->lock);
}

 
static void can327_ldisc_tx_wakeup(struct tty_struct *tty)
{
	struct can327 *elm = tty->disc_data;

	schedule_work(&elm->tx_work);
}

 
static const u32 can327_bitrate_const[] = {
	7812,  7936,  8064,  8196,   8333,   8474,   8620,   8771,
	8928,  9090,  9259,  9433,   9615,   9803,   10000,  10204,
	10416, 10638, 10869, 11111,  11363,  11627,  11904,  12195,
	12500, 12820, 13157, 13513,  13888,  14285,  14705,  15151,
	15625, 16129, 16666, 17241,  17857,  18518,  19230,  20000,
	20833, 21739, 22727, 23809,  25000,  26315,  27777,  29411,
	31250, 33333, 35714, 38461,  41666,  45454,  50000,  55555,
	62500, 71428, 83333, 100000, 125000, 166666, 250000, 500000
};

static int can327_ldisc_open(struct tty_struct *tty)
{
	struct net_device *dev;
	struct can327 *elm;
	int err;

	if (!capable(CAP_NET_ADMIN))
		return -EPERM;

	if (!tty->ops->write)
		return -EOPNOTSUPP;

	dev = alloc_candev(sizeof(struct can327), 0);
	if (!dev)
		return -ENFILE;
	elm = netdev_priv(dev);

	 
	tty->receive_room = 65536;  
	spin_lock_init(&elm->lock);
	INIT_WORK(&elm->tx_work, can327_ldisc_tx_worker);

	 
	elm->can.bitrate_const = can327_bitrate_const;
	elm->can.bitrate_const_cnt = ARRAY_SIZE(can327_bitrate_const);
	elm->can.ctrlmode_supported = CAN_CTRLMODE_LISTENONLY;

	 
	elm->dev = dev;
	dev->netdev_ops = &can327_netdev_ops;
	dev->ethtool_ops = &can327_ethtool_ops;

	 
	elm->tty = tty;
	tty->disc_data = elm;

	 
	err = register_candev(elm->dev);
	if (err) {
		free_candev(elm->dev);
		return err;
	}

	netdev_info(elm->dev, "can327 on %s.\n", tty->name);

	return 0;
}

 
static void can327_ldisc_close(struct tty_struct *tty)
{
	struct can327 *elm = tty->disc_data;

	 
	unregister_candev(elm->dev);

	 
	flush_work(&elm->tx_work);

	 
	spin_lock_bh(&elm->lock);
	tty->disc_data = NULL;
	elm->tty = NULL;
	spin_unlock_bh(&elm->lock);

	netdev_info(elm->dev, "can327 off %s.\n", tty->name);

	free_candev(elm->dev);
}

static int can327_ldisc_ioctl(struct tty_struct *tty, unsigned int cmd,
			      unsigned long arg)
{
	struct can327 *elm = tty->disc_data;
	unsigned int tmp;

	switch (cmd) {
	case SIOCGIFNAME:
		tmp = strnlen(elm->dev->name, IFNAMSIZ - 1) + 1;
		if (copy_to_user((void __user *)arg, elm->dev->name, tmp))
			return -EFAULT;
		return 0;

	case SIOCSIFHWADDR:
		return -EINVAL;

	default:
		return tty_mode_ioctl(tty, cmd, arg);
	}
}

static struct tty_ldisc_ops can327_ldisc = {
	.owner = THIS_MODULE,
	.name = KBUILD_MODNAME,
	.num = N_CAN327,
	.receive_buf = can327_ldisc_rx,
	.write_wakeup = can327_ldisc_tx_wakeup,
	.open = can327_ldisc_open,
	.close = can327_ldisc_close,
	.ioctl = can327_ldisc_ioctl,
};

static int __init can327_init(void)
{
	int status;

	status = tty_register_ldisc(&can327_ldisc);
	if (status)
		pr_err("Can't register line discipline\n");

	return status;
}

static void __exit can327_exit(void)
{
	 
	tty_unregister_ldisc(&can327_ldisc);
}

module_init(can327_init);
module_exit(can327_exit);

MODULE_ALIAS_LDISC(N_CAN327);
MODULE_DESCRIPTION("ELM327 based CAN interface");
MODULE_LICENSE("GPL");
MODULE_AUTHOR("Max Staudt <max@enpas.org>");
