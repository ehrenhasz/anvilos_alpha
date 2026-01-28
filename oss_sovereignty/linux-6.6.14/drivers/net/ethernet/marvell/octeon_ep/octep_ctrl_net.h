#ifndef __OCTEP_CTRL_NET_H__
#define __OCTEP_CTRL_NET_H__
#include "octep_cp_version.h"
#define OCTEP_CTRL_NET_INVALID_VFID	(-1)
enum octep_ctrl_net_cmd {
	OCTEP_CTRL_NET_CMD_GET = 0,
	OCTEP_CTRL_NET_CMD_SET,
};
enum octep_ctrl_net_state {
	OCTEP_CTRL_NET_STATE_DOWN = 0,
	OCTEP_CTRL_NET_STATE_UP,
};
enum octep_ctrl_net_reply {
	OCTEP_CTRL_NET_REPLY_OK = 0,
	OCTEP_CTRL_NET_REPLY_GENERIC_FAIL,
	OCTEP_CTRL_NET_REPLY_INVALID_PARAM,
};
enum octep_ctrl_net_h2f_cmd {
	OCTEP_CTRL_NET_H2F_CMD_INVALID = 0,
	OCTEP_CTRL_NET_H2F_CMD_MTU,
	OCTEP_CTRL_NET_H2F_CMD_MAC,
	OCTEP_CTRL_NET_H2F_CMD_GET_IF_STATS,
	OCTEP_CTRL_NET_H2F_CMD_GET_XSTATS,
	OCTEP_CTRL_NET_H2F_CMD_GET_Q_STATS,
	OCTEP_CTRL_NET_H2F_CMD_LINK_STATUS,
	OCTEP_CTRL_NET_H2F_CMD_RX_STATE,
	OCTEP_CTRL_NET_H2F_CMD_LINK_INFO,
	OCTEP_CTRL_NET_H2F_CMD_MAX
};
enum octep_ctrl_net_f2h_cmd {
	OCTEP_CTRL_NET_F2H_CMD_INVALID = 0,
	OCTEP_CTRL_NET_F2H_CMD_LINK_STATUS,
	OCTEP_CTRL_NET_F2H_CMD_MAX
};
union octep_ctrl_net_req_hdr {
	u64 words[1];
	struct {
		u16 sender;
		u16 receiver;
		u16 cmd;
		u16 rsvd0;
	} s;
};
struct octep_ctrl_net_h2f_req_cmd_mtu {
	u16 cmd;
	u16 val;
};
struct octep_ctrl_net_h2f_req_cmd_mac {
	u16 cmd;
	u8 addr[ETH_ALEN];
};
struct octep_ctrl_net_h2f_req_cmd_state {
	u16 cmd;
	u16 state;
};
struct octep_ctrl_net_link_info {
	u64 supported_modes;
	u64 advertised_modes;
	u8 autoneg;
	u8 pause;
	u32 speed;
};
struct octep_ctrl_net_h2f_req_cmd_link_info {
	u16 cmd;
	struct octep_ctrl_net_link_info info;
};
struct octep_ctrl_net_h2f_req {
	union octep_ctrl_net_req_hdr hdr;
	union {
		struct octep_ctrl_net_h2f_req_cmd_mtu mtu;
		struct octep_ctrl_net_h2f_req_cmd_mac mac;
		struct octep_ctrl_net_h2f_req_cmd_state link;
		struct octep_ctrl_net_h2f_req_cmd_state rx;
		struct octep_ctrl_net_h2f_req_cmd_link_info link_info;
	};
} __packed;
union octep_ctrl_net_resp_hdr {
	u64 words[1];
	struct {
		u16 sender;
		u16 receiver;
		u16 cmd;
		u16 reply;
	} s;
};
struct octep_ctrl_net_h2f_resp_cmd_mtu {
	u16 val;
};
struct octep_ctrl_net_h2f_resp_cmd_mac {
	u8 addr[ETH_ALEN];
};
struct octep_ctrl_net_h2f_resp_cmd_get_stats {
	struct octep_iface_rx_stats rx_stats;
	struct octep_iface_tx_stats tx_stats;
};
struct octep_ctrl_net_h2f_resp_cmd_state {
	u16 state;
};
struct octep_ctrl_net_h2f_resp {
	union octep_ctrl_net_resp_hdr hdr;
	union {
		struct octep_ctrl_net_h2f_resp_cmd_mtu mtu;
		struct octep_ctrl_net_h2f_resp_cmd_mac mac;
		struct octep_ctrl_net_h2f_resp_cmd_get_stats if_stats;
		struct octep_ctrl_net_h2f_resp_cmd_state link;
		struct octep_ctrl_net_h2f_resp_cmd_state rx;
		struct octep_ctrl_net_link_info link_info;
	};
} __packed;
struct octep_ctrl_net_f2h_req_cmd_state {
	u16 state;
};
struct octep_ctrl_net_f2h_req {
	union octep_ctrl_net_req_hdr hdr;
	union {
		struct octep_ctrl_net_f2h_req_cmd_state link;
	};
};
struct octep_ctrl_net_f2h_resp {
	union octep_ctrl_net_resp_hdr hdr;
};
union octep_ctrl_net_max_data {
	struct octep_ctrl_net_h2f_req h2f_req;
	struct octep_ctrl_net_h2f_resp h2f_resp;
	struct octep_ctrl_net_f2h_req f2h_req;
	struct octep_ctrl_net_f2h_resp f2h_resp;
};
struct octep_ctrl_net_wait_data {
	struct list_head list;
	int done;
	struct octep_ctrl_mbox_msg msg;
	union {
		struct octep_ctrl_net_h2f_req req;
		struct octep_ctrl_net_h2f_resp resp;
	} data;
};
int octep_ctrl_net_init(struct octep_device *oct);
int octep_ctrl_net_get_link_status(struct octep_device *oct, int vfid);
int octep_ctrl_net_set_link_status(struct octep_device *oct, int vfid, bool up,
				   bool wait_for_response);
int octep_ctrl_net_set_rx_state(struct octep_device *oct, int vfid, bool up,
				bool wait_for_response);
int octep_ctrl_net_get_mac_addr(struct octep_device *oct, int vfid, u8 *addr);
int octep_ctrl_net_set_mac_addr(struct octep_device *oct, int vfid, u8 *addr,
				bool wait_for_response);
int octep_ctrl_net_set_mtu(struct octep_device *oct, int vfid, int mtu,
			   bool wait_for_response);
int octep_ctrl_net_get_if_stats(struct octep_device *oct, int vfid,
				struct octep_iface_rx_stats *rx_stats,
				struct octep_iface_tx_stats *tx_stats);
int octep_ctrl_net_get_link_info(struct octep_device *oct, int vfid,
				 struct octep_iface_link_info *link_info);
int octep_ctrl_net_set_link_info(struct octep_device *oct,
				 int vfid,
				 struct octep_iface_link_info *link_info,
				 bool wait_for_response);
void octep_ctrl_net_recv_fw_messages(struct octep_device *oct);
int octep_ctrl_net_uninit(struct octep_device *oct);
#endif  
