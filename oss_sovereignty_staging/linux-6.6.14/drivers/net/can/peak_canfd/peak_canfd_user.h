 
 
#ifndef PEAK_CANFD_USER_H
#define PEAK_CANFD_USER_H

#include <linux/can/dev/peak_canfd.h>

#define PCANFD_ECHO_SKB_DEF		-1

 
struct peak_canfd_priv {
	struct can_priv can;		 
	struct net_device *ndev;	 
	int index;			 

	struct can_berr_counter bec;	 

	int echo_idx;			 
	spinlock_t echo_lock;

	int cmd_len;
	void *cmd_buffer;
	int cmd_maxlen;

	int (*pre_cmd)(struct peak_canfd_priv *priv);
	int (*write_cmd)(struct peak_canfd_priv *priv);
	int (*post_cmd)(struct peak_canfd_priv *priv);

	int (*enable_tx_path)(struct peak_canfd_priv *priv);
	void *(*alloc_tx_msg)(struct peak_canfd_priv *priv, u16 msg_size,
			      int *room_left);
	int (*write_tx_msg)(struct peak_canfd_priv *priv,
			    struct pucan_tx_msg *msg);
};

struct net_device *alloc_peak_canfd_dev(int sizeof_priv, int index,
					int echo_skb_max);
int peak_canfd_handle_msg(struct peak_canfd_priv *priv,
			  struct pucan_rx_msg *msg);
int peak_canfd_handle_msgs_list(struct peak_canfd_priv *priv,
				struct pucan_rx_msg *rx_msg, int rx_count);
#endif
