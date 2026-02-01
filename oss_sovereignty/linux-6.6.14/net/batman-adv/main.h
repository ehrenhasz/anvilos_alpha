 
 

#ifndef _NET_BATMAN_ADV_MAIN_H_
#define _NET_BATMAN_ADV_MAIN_H_

#define BATADV_DRIVER_AUTHOR "Marek Lindner <mareklindner@neomailbox.ch>, " \
			     "Simon Wunderlich <sw@simonwunderlich.de>"
#define BATADV_DRIVER_DESC   "B.A.T.M.A.N. advanced"
#define BATADV_DRIVER_DEVICE "batman-adv"

#ifndef BATADV_SOURCE_VERSION
#define BATADV_SOURCE_VERSION "2023.3"
#endif

 

#define BATADV_TQ_MAX_VALUE 255
#define BATADV_THROUGHPUT_MAX_VALUE 0xFFFFFFFF
#define BATADV_JITTER 20

 
#define BATADV_TTL 50

 
#define BATADV_BCAST_MAX_AGE 64

 
#define BATADV_PURGE_TIMEOUT 200000  
#define BATADV_TT_LOCAL_TIMEOUT 600000  
#define BATADV_TT_CLIENT_ROAM_TIMEOUT 600000  
#define BATADV_TT_CLIENT_TEMP_TIMEOUT 600000  
#define BATADV_TT_WORK_PERIOD 5000  
#define BATADV_ORIG_WORK_PERIOD 1000  
#define BATADV_MCAST_WORK_PERIOD 500  
#define BATADV_DAT_ENTRY_TIMEOUT (5 * 60000)  
 
#define BATADV_TQ_LOCAL_WINDOW_SIZE 64
 
#define BATADV_TT_REQUEST_TIMEOUT 3000

#define BATADV_TQ_GLOBAL_WINDOW_SIZE 5
#define BATADV_TQ_LOCAL_BIDRECT_SEND_MINIMUM 1
#define BATADV_TQ_LOCAL_BIDRECT_RECV_MINIMUM 1
#define BATADV_TQ_TOTAL_BIDRECT_LIMIT 1

 
#define BATADV_THROUGHPUT_DEFAULT_VALUE 10  
#define BATADV_ELP_PROBES_PER_NODE 2
#define BATADV_ELP_MIN_PROBE_SIZE 200  
#define BATADV_ELP_PROBE_MAX_TX_DIFF 100  
#define BATADV_ELP_MAX_AGE 64
#define BATADV_OGM_MAX_ORIGDIFF 5
#define BATADV_OGM_MAX_AGE 64

 
#define BATADV_TT_OGM_APPEND_MAX 3

 
#define BATADV_ROAMING_MAX_TIME 20000
#define BATADV_ROAMING_MAX_COUNT 5

#define BATADV_NO_FLAGS 0

#define BATADV_NULL_IFINDEX 0  

#define BATADV_NO_MARK 0

 
#define BATADV_IF_DEFAULT	((struct batadv_hard_iface *)NULL)

#define BATADV_NUM_WORDS BITS_TO_LONGS(BATADV_TQ_LOCAL_WINDOW_SIZE)

#define BATADV_LOG_BUF_LEN 8192	   

 
#define BATADV_NUM_BCASTS_DEFAULT 1
#define BATADV_NUM_BCASTS_WIRELESS 3

 
#define BATADV_TP_PACKET_LEN ETH_DATA_LEN

 
#define ARP_REQ_DELAY 250
 
#define BATADV_DAT_CANDIDATES_NUM 3

 
#define BATADV_TQ_SIMILARITY_THRESHOLD 50

 
#define BATADV_MAX_AGGREGATION_BYTES 512
#define BATADV_MAX_AGGREGATION_MS 100

#define BATADV_BLA_PERIOD_LENGTH	10000	 
#define BATADV_BLA_BACKBONE_TIMEOUT	(BATADV_BLA_PERIOD_LENGTH * 6)
#define BATADV_BLA_CLAIM_TIMEOUT	(BATADV_BLA_PERIOD_LENGTH * 10)
#define BATADV_BLA_WAIT_PERIODS		3
#define BATADV_BLA_LOOPDETECT_PERIODS	6
#define BATADV_BLA_LOOPDETECT_TIMEOUT	3000	 

#define BATADV_DUPLIST_SIZE		16
#define BATADV_DUPLIST_TIMEOUT		500	 
 
#define BATADV_RESET_PROTECTION_MS 30000
#define BATADV_EXPECTED_SEQNO_RANGE	65536

#define BATADV_NC_NODE_TIMEOUT 10000  

 
#define BATADV_TP_MAX_NUM 5

 
enum batadv_mesh_state {
	 
	BATADV_MESH_INACTIVE,

	 
	BATADV_MESH_ACTIVE,

	 
	BATADV_MESH_DEACTIVATING,
};

#define BATADV_BCAST_QUEUE_LEN		256
#define BATADV_BATMAN_QUEUE_LEN	256

 
enum batadv_uev_action {
	 
	BATADV_UEV_ADD = 0,

	 
	BATADV_UEV_DEL,

	 
	BATADV_UEV_CHANGE,

	 
	BATADV_UEV_LOOPDETECT,
};

 
enum batadv_uev_type {
	 
	BATADV_UEV_GW = 0,

	 
	BATADV_UEV_BLA,
};

#define BATADV_GW_THRESHOLD	50

 
#define BATADV_FRAG_BUFFER_COUNT 8
 
#define BATADV_FRAG_MAX_FRAGMENTS 16
 
#define BATADV_FRAG_MAX_FRAG_SIZE 1280
 
#define BATADV_FRAG_TIMEOUT 10000

#define BATADV_DAT_CANDIDATE_NOT_FOUND	0
#define BATADV_DAT_CANDIDATE_ORIG	1

 
#ifdef pr_fmt
#undef pr_fmt
#endif
 
#define pr_fmt(fmt) KBUILD_MODNAME ": " fmt

 

#include <linux/atomic.h>
#include <linux/compiler.h>
#include <linux/etherdevice.h>
#include <linux/if_vlan.h>
#include <linux/jiffies.h>
#include <linux/netdevice.h>
#include <linux/percpu.h>
#include <linux/skbuff.h>
#include <linux/types.h>
#include <uapi/linux/batadv_packet.h>

#include "types.h"
#include "main.h"

 
static inline int batadv_print_vid(unsigned short vid)
{
	if (vid & BATADV_VLAN_HAS_TAG)
		return (int)(vid & VLAN_VID_MASK);
	else
		return -1;
}

extern struct list_head batadv_hardif_list;
extern unsigned int batadv_hardif_generation;

extern unsigned char batadv_broadcast_addr[];
extern struct workqueue_struct *batadv_event_workqueue;

int batadv_mesh_init(struct net_device *soft_iface);
void batadv_mesh_free(struct net_device *soft_iface);
bool batadv_is_my_mac(struct batadv_priv *bat_priv, const u8 *addr);
int batadv_max_header_len(void);
void batadv_skb_set_priority(struct sk_buff *skb, int offset);
int batadv_batman_skb_recv(struct sk_buff *skb, struct net_device *dev,
			   struct packet_type *ptype,
			   struct net_device *orig_dev);
int
batadv_recv_handler_register(u8 packet_type,
			     int (*recv_handler)(struct sk_buff *,
						 struct batadv_hard_iface *));
void batadv_recv_handler_unregister(u8 packet_type);
__be32 batadv_skb_crc32(struct sk_buff *skb, u8 *payload_ptr);

 
static inline bool batadv_compare_eth(const void *data1, const void *data2)
{
	return ether_addr_equal_unaligned(data1, data2);
}

 
static inline bool batadv_has_timed_out(unsigned long timestamp,
					unsigned int timeout)
{
	return time_is_before_jiffies(timestamp + msecs_to_jiffies(timeout));
}

 
#define batadv_atomic_dec_not_zero(v)	atomic_add_unless((v), -1, 0)

 
#define batadv_smallest_signed_int(x) (1u << (7u + 8u * (sizeof(x) - 1u)))

 
#define batadv_seq_before(x, y) ({ \
	typeof(x)_d1 = (x); \
	typeof(y)_d2 = (y); \
	typeof(x)_dummy = (_d1 - _d2); \
	(void)(&_d1 == &_d2); \
	_dummy > batadv_smallest_signed_int(_dummy); \
})

 
#define batadv_seq_after(x, y) batadv_seq_before(y, x)

 
static inline void batadv_add_counter(struct batadv_priv *bat_priv, size_t idx,
				      size_t count)
{
	this_cpu_add(bat_priv->bat_counters[idx], count);
}

 
#define batadv_inc_counter(b, i) batadv_add_counter(b, i, 1)

 
#define BATADV_SKB_CB(__skb)       ((struct batadv_skb_cb *)&((__skb)->cb[0]))

unsigned short batadv_get_vid(struct sk_buff *skb, size_t header_len);
bool batadv_vlan_ap_isola_get(struct batadv_priv *bat_priv, unsigned short vid);
int batadv_throw_uevent(struct batadv_priv *bat_priv, enum batadv_uev_type type,
			enum batadv_uev_action action, const char *data);

#endif  
