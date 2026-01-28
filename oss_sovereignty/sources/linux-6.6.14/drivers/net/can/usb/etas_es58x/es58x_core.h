



#ifndef __ES58X_COMMON_H__
#define __ES58X_COMMON_H__

#include <linux/can.h>
#include <linux/can/dev.h>
#include <linux/netdevice.h>
#include <linux/types.h>
#include <linux/usb.h>
#include <net/devlink.h>

#include "es581_4.h"
#include "es58x_fd.h"


#define ES58X_RX_URBS_MAX 5	
#define ES58X_TX_URBS_MAX 6	

#define ES58X_MAX(param)				\
	(ES581_4_##param > ES58X_FD_##param ?		\
		ES581_4_##param : ES58X_FD_##param)
#define ES58X_TX_BULK_MAX ES58X_MAX(TX_BULK_MAX)
#define ES58X_RX_BULK_MAX ES58X_MAX(RX_BULK_MAX)
#define ES58X_ECHO_BULK_MAX ES58X_MAX(ECHO_BULK_MAX)
#define ES58X_NUM_CAN_CH_MAX ES58X_MAX(NUM_CAN_CH)


#define ES58X_CHANNEL_IDX_NA 0xFF
#define ES58X_EMPTY_MSG NULL


#define ES58X_CONSECUTIVE_ERR_PASSIVE_MAX 254


#define ES58X_HEARTBEAT 0x11


enum es58x_driver_info {
	ES58X_DUAL_CHANNEL = BIT(0),
	ES58X_FD_FAMILY = BIT(1)
};

enum es58x_echo {
	ES58X_ECHO_OFF = 0,
	ES58X_ECHO_ON = 1
};


enum es58x_physical_layer {
	ES58X_PHYSICAL_LAYER_HIGH_SPEED = 1
};

enum es58x_samples_per_bit {
	ES58X_SAMPLES_PER_BIT_ONE = 1,
	ES58X_SAMPLES_PER_BIT_THREE = 2
};


enum es58x_sync_edge {
	ES58X_SYNC_EDGE_SINGLE = 1
};


enum es58x_flag {
	ES58X_FLAG_EFF = BIT(0),
	ES58X_FLAG_RTR = BIT(1),
	ES58X_FLAG_FD_BRS = BIT(3),
	ES58X_FLAG_FD_ESI = BIT(5),
	ES58X_FLAG_FD_DATA = BIT(6)
};


enum es58x_err {
	ES58X_ERR_OK = 0,
	ES58X_ERR_PROT_STUFF = BIT(0),
	ES58X_ERR_PROT_FORM = BIT(1),
	ES58X_ERR_ACK = BIT(2),
	ES58X_ERR_PROT_BIT = BIT(3),
	ES58X_ERR_PROT_CRC = BIT(4),
	ES58X_ERR_PROT_BIT1 = BIT(5),
	ES58X_ERR_PROT_BIT0 = BIT(6),
	ES58X_ERR_PROT_OVERLOAD = BIT(7),
	ES58X_ERR_PROT_UNSPEC = BIT(31)
};


enum es58x_event {
	ES58X_EVENT_OK = 0,
	ES58X_EVENT_CRTL_ACTIVE = BIT(0),
	ES58X_EVENT_CRTL_PASSIVE = BIT(1),
	ES58X_EVENT_CRTL_WARNING = BIT(2),
	ES58X_EVENT_BUSOFF = BIT(3),
	ES58X_EVENT_SINGLE_WIRE = BIT(4)
};


enum es58x_ret_u8 {
	ES58X_RET_U8_OK = 0x00,
	ES58X_RET_U8_ERR_UNSPECIFIED_FAILURE = 0x80,
	ES58X_RET_U8_ERR_NO_MEM = 0x81,
	ES58X_RET_U8_ERR_BAD_CRC = 0x99
};


enum es58x_ret_u32 {
	ES58X_RET_U32_OK = 0x00000000UL,
	ES58X_RET_U32_ERR_UNSPECIFIED_FAILURE = 0x80000000UL,
	ES58X_RET_U32_ERR_NO_MEM = 0x80004001UL,
	ES58X_RET_U32_WARN_PARAM_ADJUSTED = 0x40004000UL,
	ES58X_RET_U32_WARN_TX_MAYBE_REORDER = 0x40004001UL,
	ES58X_RET_U32_ERR_TIMEDOUT = 0x80000008UL,
	ES58X_RET_U32_ERR_FIFO_FULL = 0x80003002UL,
	ES58X_RET_U32_ERR_BAD_CONFIG = 0x80004000UL,
	ES58X_RET_U32_ERR_NO_RESOURCE = 0x80004002UL
};


enum es58x_ret_type {
	ES58X_RET_TYPE_SET_BITTIMING,
	ES58X_RET_TYPE_ENABLE_CHANNEL,
	ES58X_RET_TYPE_DISABLE_CHANNEL,
	ES58X_RET_TYPE_TX_MSG,
	ES58X_RET_TYPE_RESET_RX,
	ES58X_RET_TYPE_RESET_TX,
	ES58X_RET_TYPE_DEVICE_ERR
};

union es58x_urb_cmd {
	struct es581_4_urb_cmd es581_4_urb_cmd;
	struct es58x_fd_urb_cmd es58x_fd_urb_cmd;
	struct {		
		__le16 sof;
		u8 cmd_type;
		u8 cmd_id;
	} __packed;
	DECLARE_FLEX_ARRAY(u8, raw_cmd);
};


struct es58x_priv {
	struct can_priv can;
	struct devlink_port devlink_port;
	struct es58x_device *es58x_dev;
	struct urb *tx_urb;

	u32 tx_tail;
	u32 tx_head;

	u8 tx_can_msg_cnt;
	bool tx_can_msg_is_fd;

	u8 err_passive_before_rtx_success;

	u8 channel_idx;
};


struct es58x_parameters {
	const struct can_bittiming_const *bittiming_const;
	const struct can_bittiming_const *data_bittiming_const;
	const struct can_tdc_const *tdc_const;
	u32 bitrate_max;
	struct can_clock clock;
	u32 ctrlmode_supported;
	u16 tx_start_of_frame;
	u16 rx_start_of_frame;
	u16 tx_urb_cmd_max_len;
	u16 rx_urb_cmd_max_len;
	u16 fifo_mask;
	u16 dql_min_limit;
	u8 tx_bulk_max;
	u8 urb_cmd_header_len;
	u8 rx_urb_max;
	u8 tx_urb_max;
};


struct es58x_operators {
	u16 (*get_msg_len)(const union es58x_urb_cmd *urb_cmd);
	int (*handle_urb_cmd)(struct es58x_device *es58x_dev,
			      const union es58x_urb_cmd *urb_cmd);
	void (*fill_urb_header)(union es58x_urb_cmd *urb_cmd, u8 cmd_type,
				u8 cmd_id, u8 channel_idx, u16 cmd_len);
	int (*tx_can_msg)(struct es58x_priv *priv, const struct sk_buff *skb);
	int (*enable_channel)(struct es58x_priv *priv);
	int (*disable_channel)(struct es58x_priv *priv);
	int (*reset_device)(struct es58x_device *es58x_dev);
	int (*get_timestamp)(struct es58x_device *es58x_dev);
};


struct es58x_sw_version {
	u8 major;
	u8 minor;
	u8 revision;
};


struct es58x_hw_revision {
	char letter;
	u16 major;
	u16 minor;
};


struct es58x_device {
	struct device *dev;
	struct usb_device *udev;
	struct net_device *netdev[ES58X_NUM_CAN_CH_MAX];

	const struct es58x_parameters *param;
	const struct es58x_operators *ops;

	unsigned int rx_pipe;
	unsigned int tx_pipe;

	struct usb_anchor rx_urbs;
	struct usb_anchor tx_urbs_busy;
	struct usb_anchor tx_urbs_idle;
	atomic_t tx_urbs_idle_cnt;

	struct es58x_sw_version firmware_version;
	struct es58x_sw_version bootloader_version;
	struct es58x_hw_revision hardware_revision;

	u64 ktime_req_ns;
	s64 realtime_diff_ns;

	u64 timestamps[ES58X_ECHO_BULK_MAX];

	u8 num_can_ch;
	u8 opened_channel_cnt;

	u16 rx_cmd_buf_len;
	union es58x_urb_cmd rx_cmd_buf;
};


static inline size_t es58x_sizeof_es58x_device(const struct es58x_parameters
					       *es58x_dev_param)
{
	return offsetof(struct es58x_device, rx_cmd_buf) +
		es58x_dev_param->rx_urb_cmd_max_len;
}

static inline int __es58x_check_msg_len(const struct device *dev,
					const char *stringified_msg,
					size_t actual_len, size_t expected_len)
{
	if (expected_len != actual_len) {
		dev_err(dev,
			"Length of %s is %zu but received command is %zu.\n",
			stringified_msg, expected_len, actual_len);
		return -EMSGSIZE;
	}
	return 0;
}


#define es58x_check_msg_len(dev, msg, actual_len)			\
	__es58x_check_msg_len(dev, __stringify(msg),			\
			      actual_len, sizeof(msg))

static inline int __es58x_check_msg_max_len(const struct device *dev,
					    const char *stringified_msg,
					    size_t actual_len,
					    size_t expected_len)
{
	if (actual_len > expected_len) {
		dev_err(dev,
			"Maximum length for %s is %zu but received command is %zu.\n",
			stringified_msg, expected_len, actual_len);
		return -EOVERFLOW;
	}
	return 0;
}


#define es58x_check_msg_max_len(dev, msg, actual_len)			\
	__es58x_check_msg_max_len(dev, __stringify(msg),		\
				  actual_len, sizeof(msg))

static inline int __es58x_msg_num_element(const struct device *dev,
					  const char *stringified_msg,
					  size_t actual_len, size_t msg_len,
					  size_t elem_len)
{
	size_t actual_num_elem = actual_len / elem_len;
	size_t expected_num_elem = msg_len / elem_len;

	if (actual_num_elem == 0) {
		dev_err(dev,
			"Minimum length for %s is %zu but received command is %zu.\n",
			stringified_msg, elem_len, actual_len);
		return -EMSGSIZE;
	} else if ((actual_len % elem_len) != 0) {
		dev_err(dev,
			"Received command length: %zu is not a multiple of %s[0]: %zu\n",
			actual_len, stringified_msg, elem_len);
		return -EMSGSIZE;
	} else if (actual_num_elem > expected_num_elem) {
		dev_err(dev,
			"Array %s is supposed to have %zu elements each of size %zu...\n",
			stringified_msg, expected_num_elem, elem_len);
		dev_err(dev,
			"... But received command has %zu elements (total length %zu).\n",
			actual_num_elem, actual_len);
		return -EOVERFLOW;
	}
	return actual_num_elem;
}


#define es58x_msg_num_element(dev, msg, actual_len)			\
({									\
	size_t __elem_len = sizeof((msg)[0]) + __must_be_array(msg);	\
	__es58x_msg_num_element(dev, __stringify(msg), actual_len,	\
				sizeof(msg), __elem_len);		\
})


static inline struct es58x_priv *es58x_priv(struct net_device *netdev)
{
	return (struct es58x_priv *)netdev_priv(netdev);
}


#define ES58X_SIZEOF_URB_CMD(es58x_urb_cmd_type, msg_field)		\
	(offsetof(es58x_urb_cmd_type, raw_msg)				\
		+ sizeof_field(es58x_urb_cmd_type, msg_field)		\
		+ sizeof_field(es58x_urb_cmd_type,			\
			       reserved_for_crc16_do_not_use))


static inline size_t es58x_get_urb_cmd_len(struct es58x_device *es58x_dev,
					   u16 msg_len)
{
	return es58x_dev->param->urb_cmd_header_len + msg_len + sizeof(u16);
}


static inline int es58x_get_netdev(struct es58x_device *es58x_dev,
				   int channel_no, int channel_idx_offset,
				   struct net_device **netdev)
{
	int channel_idx = channel_no - channel_idx_offset;

	*netdev = NULL;
	if (channel_idx < 0 || channel_idx >= es58x_dev->num_can_ch)
		return -ECHRNG;

	*netdev = es58x_dev->netdev[channel_idx];
	if (!*netdev || !netif_device_present(*netdev))
		return -ENODEV;

	return 0;
}


static inline int es58x_get_raw_can_id(const struct can_frame *cf)
{
	if (cf->can_id & CAN_EFF_FLAG)
		return cf->can_id & CAN_EFF_MASK;
	else
		return cf->can_id & CAN_SFF_MASK;
}


static inline enum es58x_flag es58x_get_flags(const struct sk_buff *skb)
{
	struct canfd_frame *cf = (struct canfd_frame *)skb->data;
	enum es58x_flag es58x_flags = 0;

	if (cf->can_id & CAN_EFF_FLAG)
		es58x_flags |= ES58X_FLAG_EFF;

	if (can_is_canfd_skb(skb)) {
		es58x_flags |= ES58X_FLAG_FD_DATA;
		if (cf->flags & CANFD_BRS)
			es58x_flags |= ES58X_FLAG_FD_BRS;
		if (cf->flags & CANFD_ESI)
			es58x_flags |= ES58X_FLAG_FD_ESI;
	} else if (cf->can_id & CAN_RTR_FLAG)
		
		es58x_flags |= ES58X_FLAG_RTR;

	return es58x_flags;
}


int es58x_can_get_echo_skb(struct net_device *netdev, u32 packet_idx,
			   u64 *tstamps, unsigned int pkts);
int es58x_tx_ack_msg(struct net_device *netdev, u16 tx_free_entries,
		     enum es58x_ret_u32 rx_cmd_ret_u32);
int es58x_rx_can_msg(struct net_device *netdev, u64 timestamp, const u8 *data,
		     canid_t can_id, enum es58x_flag es58x_flags, u8 dlc);
int es58x_rx_err_msg(struct net_device *netdev, enum es58x_err error,
		     enum es58x_event event, u64 timestamp);
void es58x_rx_timestamp(struct es58x_device *es58x_dev, u64 timestamp);
int es58x_rx_cmd_ret_u8(struct device *dev, enum es58x_ret_type cmd_ret_type,
			enum es58x_ret_u8 rx_cmd_ret_u8);
int es58x_rx_cmd_ret_u32(struct net_device *netdev,
			 enum es58x_ret_type cmd_ret_type,
			 enum es58x_ret_u32 rx_cmd_ret_u32);
int es58x_send_msg(struct es58x_device *es58x_dev, u8 cmd_type, u8 cmd_id,
		   const void *msg, u16 cmd_len, int channel_idx);


void es58x_parse_product_info(struct es58x_device *es58x_dev);
extern const struct devlink_ops es58x_dl_ops;


extern const struct es58x_parameters es581_4_param;
extern const struct es58x_operators es581_4_ops;


extern const struct es58x_parameters es58x_fd_param;
extern const struct es58x_operators es58x_fd_ops;

#endif 
