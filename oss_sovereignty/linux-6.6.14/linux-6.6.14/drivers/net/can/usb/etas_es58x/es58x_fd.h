#ifndef __ES58X_FD_H__
#define __ES58X_FD_H__
#include <linux/types.h>
#define ES582_1_NUM_CAN_CH 2
#define ES584_1_NUM_CAN_CH 1
#define ES58X_FD_NUM_CAN_CH 2
#define ES58X_FD_CHANNEL_IDX_OFFSET 0
#define ES58X_FD_TX_BULK_MAX 100
#define ES58X_FD_RX_BULK_MAX 100
#define ES58X_FD_ECHO_BULK_MAX 100
enum es58x_fd_cmd_type {
	ES58X_FD_CMD_TYPE_CAN = 0x03,
	ES58X_FD_CMD_TYPE_CANFD = 0x04,
	ES58X_FD_CMD_TYPE_DEVICE = 0xFF
};
enum es58x_fd_can_cmd_id {
	ES58X_FD_CAN_CMD_ID_ENABLE_CHANNEL = 0x01,
	ES58X_FD_CAN_CMD_ID_DISABLE_CHANNEL = 0x02,
	ES58X_FD_CAN_CMD_ID_TX_MSG = 0x05,
	ES58X_FD_CAN_CMD_ID_ECHO_MSG = 0x07,
	ES58X_FD_CAN_CMD_ID_RX_MSG = 0x10,
	ES58X_FD_CAN_CMD_ID_ERROR_OR_EVENT_MSG = 0x11,
	ES58X_FD_CAN_CMD_ID_RESET_RX = 0x20,
	ES58X_FD_CAN_CMD_ID_RESET_TX = 0x21,
	ES58X_FD_CAN_CMD_ID_TX_MSG_NO_ACK = 0x55
};
enum es58x_fd_dev_cmd_id {
	ES58X_FD_DEV_CMD_ID_GETTIMETICKS = 0x01,
	ES58X_FD_DEV_CMD_ID_TIMESTAMP = 0x02
};
enum es58x_fd_ctrlmode {
	ES58X_FD_CTRLMODE_ACTIVE = 0,
	ES58X_FD_CTRLMODE_PASSIVE = BIT(0),
	ES58X_FD_CTRLMODE_FD = BIT(4),
	ES58X_FD_CTRLMODE_FD_NON_ISO = BIT(5),
	ES58X_FD_CTRLMODE_DISABLE_PROTOCOL_EXCEPTION_HANDLING = BIT(6),
	ES58X_FD_CTRLMODE_EDGE_FILTER_DURING_BUS_INTEGRATION = BIT(7)
};
struct es58x_fd_bittiming {
	__le32 bitrate;
	__le16 tseg1;		 
	__le16 tseg2;		 
	__le16 brp;		 
	__le16 sjw;		 
} __packed;
struct es58x_fd_tx_conf_msg {
	struct es58x_fd_bittiming nominal_bittiming;
	u8 samples_per_bit;
	u8 sync_edge;
	u8 physical_layer;
	u8 echo_mode;
	u8 ctrlmode;
	u8 canfd_enabled;
	struct es58x_fd_bittiming data_bittiming;
	u8 tdc_enabled;
	__le16 tdco;
	__le16 tdcf;
} __packed;
#define ES58X_FD_CAN_CONF_LEN					\
	(offsetof(struct es58x_fd_tx_conf_msg, canfd_enabled))
#define ES58X_FD_CANFD_CONF_LEN (sizeof(struct es58x_fd_tx_conf_msg))
struct es58x_fd_tx_can_msg {
	u8 packet_idx;
	__le32 can_id;
	u8 flags;
	union {
		u8 dlc;		 
		u8 len;		 
	} __packed;
	u8 data[CANFD_MAX_DLEN];
} __packed;
#define ES58X_FD_CAN_TX_LEN						\
	(offsetof(struct es58x_fd_tx_can_msg, data[CAN_MAX_DLEN]))
#define ES58X_FD_CANFD_TX_LEN (sizeof(struct es58x_fd_tx_can_msg))
struct es58x_fd_rx_can_msg {
	__le64 timestamp;
	__le32 can_id;
	u8 flags;
	union {
		u8 dlc;		 
		u8 len;		 
	} __packed;
	u8 data[CANFD_MAX_DLEN];
} __packed;
#define ES58X_FD_CAN_RX_LEN						\
	(offsetof(struct es58x_fd_rx_can_msg, data[CAN_MAX_DLEN]))
#define ES58X_FD_CANFD_RX_LEN (sizeof(struct es58x_fd_rx_can_msg))
struct es58x_fd_echo_msg {
	__le64 timestamp;
	u8 packet_idx;
} __packed;
struct es58x_fd_rx_event_msg {
	__le64 timestamp;
	__le32 can_id;
	u8 flags;		 
	u8 error_type;		 
	u8 error_code;
	u8 event_code;
} __packed;
struct es58x_fd_tx_ack_msg {
	__le32 rx_cmd_ret_le32;	 
	__le16 tx_free_entries;	 
} __packed;
struct es58x_fd_urb_cmd {
	__le16 SOF;
	u8 cmd_type;
	u8 cmd_id;
	u8 channel_idx;
	__le16 msg_len;
	union {
		struct es58x_fd_tx_conf_msg tx_conf_msg;
		u8 tx_can_msg_buf[ES58X_FD_TX_BULK_MAX * ES58X_FD_CANFD_TX_LEN];
		u8 rx_can_msg_buf[ES58X_FD_RX_BULK_MAX * ES58X_FD_CANFD_RX_LEN];
		struct es58x_fd_echo_msg echo_msg[ES58X_FD_ECHO_BULK_MAX];
		struct es58x_fd_rx_event_msg rx_event_msg;
		struct es58x_fd_tx_ack_msg tx_ack_msg;
		__le64 timestamp;
		__le32 rx_cmd_ret_le32;
		DECLARE_FLEX_ARRAY(u8, raw_msg);
	} __packed;
	__le16 reserved_for_crc16_do_not_use;
} __packed;
#define ES58X_FD_URB_CMD_HEADER_LEN (offsetof(struct es58x_fd_urb_cmd, raw_msg))
#define ES58X_FD_TX_URB_CMD_MAX_LEN					\
	ES58X_SIZEOF_URB_CMD(struct es58x_fd_urb_cmd, tx_can_msg_buf)
#define ES58X_FD_RX_URB_CMD_MAX_LEN					\
	ES58X_SIZEOF_URB_CMD(struct es58x_fd_urb_cmd, rx_can_msg_buf)
#endif  
