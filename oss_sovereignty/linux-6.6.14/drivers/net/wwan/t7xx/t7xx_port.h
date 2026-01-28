#ifndef __T7XX_PORT_H__
#define __T7XX_PORT_H__
#include <linux/bits.h>
#include <linux/device.h>
#include <linux/mutex.h>
#include <linux/sched.h>
#include <linux/skbuff.h>
#include <linux/spinlock.h>
#include <linux/types.h>
#include <linux/wait.h>
#include <linux/wwan.h>
#include "t7xx_hif_cldma.h"
#include "t7xx_pci.h"
#define PORT_CH_ID_MASK		GENMASK(7, 0)
enum port_ch {
	PORT_CH_AP_CONTROL_RX = 0x1000,
	PORT_CH_AP_CONTROL_TX = 0x1001,
	PORT_CH_CONTROL_RX = 0x2000,
	PORT_CH_CONTROL_TX = 0x2001,
	PORT_CH_UART1_RX = 0x2006,	 
	PORT_CH_UART1_TX = 0x2008,
	PORT_CH_UART2_RX = 0x200a,	 
	PORT_CH_UART2_TX = 0x200c,
	PORT_CH_MD_LOG_RX = 0x202a,	 
	PORT_CH_MD_LOG_TX = 0x202b,
	PORT_CH_LB_IT_RX = 0x203e,	 
	PORT_CH_LB_IT_TX = 0x203f,
	PORT_CH_STATUS_RX = 0x2043,	 
	PORT_CH_MIPC_RX = 0x20ce,	 
	PORT_CH_MIPC_TX = 0x20cf,
	PORT_CH_MBIM_RX = 0x20d0,
	PORT_CH_MBIM_TX = 0x20d1,
	PORT_CH_DSS0_RX = 0x20d2,
	PORT_CH_DSS0_TX = 0x20d3,
	PORT_CH_DSS1_RX = 0x20d4,
	PORT_CH_DSS1_TX = 0x20d5,
	PORT_CH_DSS2_RX = 0x20d6,
	PORT_CH_DSS2_TX = 0x20d7,
	PORT_CH_DSS3_RX = 0x20d8,
	PORT_CH_DSS3_TX = 0x20d9,
	PORT_CH_DSS4_RX = 0x20da,
	PORT_CH_DSS4_TX = 0x20db,
	PORT_CH_DSS5_RX = 0x20dc,
	PORT_CH_DSS5_TX = 0x20dd,
	PORT_CH_DSS6_RX = 0x20de,
	PORT_CH_DSS6_TX = 0x20df,
	PORT_CH_DSS7_RX = 0x20e0,
	PORT_CH_DSS7_TX = 0x20e1,
};
struct t7xx_port;
struct port_ops {
	int (*init)(struct t7xx_port *port);
	int (*recv_skb)(struct t7xx_port *port, struct sk_buff *skb);
	void (*md_state_notify)(struct t7xx_port *port, unsigned int md_state);
	void (*uninit)(struct t7xx_port *port);
	int (*enable_chl)(struct t7xx_port *port);
	int (*disable_chl)(struct t7xx_port *port);
};
struct t7xx_port_conf {
	enum port_ch		tx_ch;
	enum port_ch		rx_ch;
	unsigned char		txq_index;
	unsigned char		rxq_index;
	unsigned char		txq_exp_index;
	unsigned char		rxq_exp_index;
	enum cldma_id		path_id;
	struct port_ops		*ops;
	char			*name;
	enum wwan_port_type	port_type;
};
struct t7xx_port {
	const struct t7xx_port_conf	*port_conf;
	struct t7xx_pci_dev		*t7xx_dev;
	struct device			*dev;
	u16				seq_nums[2];	 
	atomic_t			usage_cnt;
	struct				list_head entry;
	struct				list_head queue_entry;
	struct sk_buff_head		rx_skb_list;
	spinlock_t			port_update_lock;  
	wait_queue_head_t		rx_wq;
	int				rx_length_th;
	bool				chan_enable;
	struct task_struct		*thread;
	union {
		struct {
			struct wwan_port		*wwan_port;
		} wwan;
		struct {
			struct rchan			*relaych;
		} log;
	};
};
struct sk_buff *t7xx_port_alloc_skb(int payload);
struct sk_buff *t7xx_ctrl_alloc_skb(int payload);
int t7xx_port_enqueue_skb(struct t7xx_port *port, struct sk_buff *skb);
int t7xx_port_send_skb(struct t7xx_port *port, struct sk_buff *skb, unsigned int pkt_header,
		       unsigned int ex_msg);
int t7xx_port_send_ctl_skb(struct t7xx_port *port, struct sk_buff *skb, unsigned int msg,
			   unsigned int ex_msg);
#endif  
