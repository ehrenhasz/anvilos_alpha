 
 

#ifndef _NET_BATMAN_ADV_TYPES_H_
#define _NET_BATMAN_ADV_TYPES_H_

#ifndef _NET_BATMAN_ADV_MAIN_H_
#error only "main.h" can be included directly
#endif

#include <linux/average.h>
#include <linux/bitops.h>
#include <linux/compiler.h>
#include <linux/if.h>
#include <linux/if_ether.h>
#include <linux/kref.h>
#include <linux/mutex.h>
#include <linux/netdevice.h>
#include <linux/netlink.h>
#include <linux/sched.h>  
#include <linux/skbuff.h>
#include <linux/spinlock.h>
#include <linux/timer.h>
#include <linux/types.h>
#include <linux/wait.h>
#include <linux/workqueue.h>
#include <uapi/linux/batadv_packet.h>
#include <uapi/linux/batman_adv.h>

#ifdef CONFIG_BATMAN_ADV_DAT

 
typedef u16 batadv_dat_addr_t;

#endif  

 
enum batadv_dhcp_recipient {
	 
	BATADV_DHCP_NO = 0,

	 
	BATADV_DHCP_TO_SERVER,

	 
	BATADV_DHCP_TO_CLIENT,
};

 
#define BATADV_TT_REMOTE_MASK	0x00FF

 
#define BATADV_TT_SYNC_MASK	0x00F0

 
struct batadv_hard_iface_bat_iv {
	 
	unsigned char *ogm_buff;

	 
	int ogm_buff_len;

	 
	atomic_t ogm_seqno;

	 
	struct mutex ogm_buff_mutex;
};

 
enum batadv_v_hard_iface_flags {
	 
	BATADV_FULL_DUPLEX	= BIT(0),

	 
	BATADV_WARNING_DEFAULT	= BIT(1),
};

 
struct batadv_hard_iface_bat_v {
	 
	atomic_t elp_interval;

	 
	atomic_t elp_seqno;

	 
	struct sk_buff *elp_skb;

	 
	struct delayed_work elp_wq;

	 
	struct delayed_work aggr_wq;

	 
	struct sk_buff_head aggr_list;

	 
	unsigned int aggr_len;

	 
	atomic_t throughput_override;

	 
	u8 flags;
};

 
enum batadv_hard_iface_wifi_flags {
	 
	BATADV_HARDIF_WIFI_WEXT_DIRECT = BIT(0),

	 
	BATADV_HARDIF_WIFI_CFG80211_DIRECT = BIT(1),

	 
	BATADV_HARDIF_WIFI_WEXT_INDIRECT = BIT(2),

	 
	BATADV_HARDIF_WIFI_CFG80211_INDIRECT = BIT(3),
};

 
struct batadv_hard_iface {
	 
	struct list_head list;

	 
	char if_status;

	 
	u8 num_bcasts;

	 
	u32 wifi_flags;

	 
	struct net_device *net_dev;

	 
	struct kref refcount;

	 
	struct packet_type batman_adv_ptype;

	 
	struct net_device *soft_iface;

	 
	struct rcu_head rcu;

	 
	atomic_t hop_penalty;

	 
	struct batadv_hard_iface_bat_iv bat_iv;

#ifdef CONFIG_BATMAN_ADV_BATMAN_V
	 
	struct batadv_hard_iface_bat_v bat_v;
#endif

	 
	struct hlist_head neigh_list;

	 
	spinlock_t neigh_list_lock;
};

 
struct batadv_orig_ifinfo_bat_iv {
	 
	DECLARE_BITMAP(bcast_own, BATADV_TQ_LOCAL_WINDOW_SIZE);

	 
	u8 bcast_own_sum;
};

 
struct batadv_orig_ifinfo {
	 
	struct hlist_node list;

	 
	struct batadv_hard_iface *if_outgoing;

	 
	struct batadv_neigh_node __rcu *router;

	 
	u32 last_real_seqno;

	 
	u8 last_ttl;

	 
	u32 last_seqno_forwarded;

	 
	unsigned long batman_seqno_reset;

	 
	struct batadv_orig_ifinfo_bat_iv bat_iv;

	 
	struct kref refcount;

	 
	struct rcu_head rcu;
};

 
struct batadv_frag_table_entry {
	 
	struct hlist_head fragment_list;

	 
	spinlock_t lock;

	 
	unsigned long timestamp;

	 
	u16 seqno;

	 
	u16 size;

	 
	u16 total_size;
};

 
struct batadv_frag_list_entry {
	 
	struct hlist_node list;

	 
	struct sk_buff *skb;

	 
	u8 no;
};

 
struct batadv_vlan_tt {
	 
	u32 crc;

	 
	atomic_t num_entries;
};

 
struct batadv_orig_node_vlan {
	 
	unsigned short vid;

	 
	struct batadv_vlan_tt tt;

	 
	struct hlist_node list;

	 
	struct kref refcount;

	 
	struct rcu_head rcu;
};

 
struct batadv_orig_bat_iv {
	 
	spinlock_t ogm_cnt_lock;
};

 
struct batadv_orig_node {
	 
	u8 orig[ETH_ALEN];

	 
	struct hlist_head ifinfo_list;

	 
	struct batadv_orig_ifinfo *last_bonding_candidate;

#ifdef CONFIG_BATMAN_ADV_DAT
	 
	batadv_dat_addr_t dat_addr;
#endif

	 
	unsigned long last_seen;

	 
	unsigned long bcast_seqno_reset;

#ifdef CONFIG_BATMAN_ADV_MCAST
	 
	spinlock_t mcast_handler_lock;

	 
	u8 mcast_flags;

	 
	struct hlist_node mcast_want_all_unsnoopables_node;

	 
	struct hlist_node mcast_want_all_ipv4_node;
	 
	struct hlist_node mcast_want_all_ipv6_node;

	 
	struct hlist_node mcast_want_all_rtr4_node;
	 
	struct hlist_node mcast_want_all_rtr6_node;
#endif

	 
	unsigned long capabilities;

	 
	unsigned long capa_initialized;

	 
	atomic_t last_ttvn;

	 
	unsigned char *tt_buff;

	 
	s16 tt_buff_len;

	 
	spinlock_t tt_buff_lock;

	 
	spinlock_t tt_lock;

	 
	DECLARE_BITMAP(bcast_bits, BATADV_TQ_LOCAL_WINDOW_SIZE);

	 
	u32 last_bcast_seqno;

	 
	struct hlist_head neigh_list;

	 
	spinlock_t neigh_list_lock;

	 
	struct hlist_node hash_entry;

	 
	struct batadv_priv *bat_priv;

	 
	spinlock_t bcast_seqno_lock;

	 
	struct kref refcount;

	 
	struct rcu_head rcu;

#ifdef CONFIG_BATMAN_ADV_NC
	 
	struct list_head in_coding_list;

	 
	struct list_head out_coding_list;

	 
	spinlock_t in_coding_list_lock;

	 
	spinlock_t out_coding_list_lock;
#endif

	 
	struct batadv_frag_table_entry fragments[BATADV_FRAG_BUFFER_COUNT];

	 
	struct hlist_head vlan_list;

	 
	spinlock_t vlan_list_lock;

	 
	struct batadv_orig_bat_iv bat_iv;
};

 
enum batadv_orig_capabilities {
	 
	BATADV_ORIG_CAPA_HAS_DAT,

	 
	BATADV_ORIG_CAPA_HAS_NC,

	 
	BATADV_ORIG_CAPA_HAS_TT,

	 
	BATADV_ORIG_CAPA_HAS_MCAST,
};

 
struct batadv_gw_node {
	 
	struct hlist_node list;

	 
	struct batadv_orig_node *orig_node;

	 
	u32 bandwidth_down;

	 
	u32 bandwidth_up;

	 
	struct kref refcount;

	 
	struct rcu_head rcu;
};

DECLARE_EWMA(throughput, 10, 8)

 
struct batadv_hardif_neigh_node_bat_v {
	 
	struct ewma_throughput throughput;

	 
	u32 elp_interval;

	 
	u32 elp_latest_seqno;

	 
	unsigned long last_unicast_tx;

	 
	struct work_struct metric_work;
};

 
struct batadv_hardif_neigh_node {
	 
	struct hlist_node list;

	 
	u8 addr[ETH_ALEN];

	 
	u8 orig[ETH_ALEN];

	 
	struct batadv_hard_iface *if_incoming;

	 
	unsigned long last_seen;

#ifdef CONFIG_BATMAN_ADV_BATMAN_V
	 
	struct batadv_hardif_neigh_node_bat_v bat_v;
#endif

	 
	struct kref refcount;

	 
	struct rcu_head rcu;
};

 
struct batadv_neigh_node {
	 
	struct hlist_node list;

	 
	struct batadv_orig_node *orig_node;

	 
	u8 addr[ETH_ALEN];

	 
	struct hlist_head ifinfo_list;

	 
	spinlock_t ifinfo_lock;

	 
	struct batadv_hard_iface *if_incoming;

	 
	unsigned long last_seen;

	 
	struct batadv_hardif_neigh_node *hardif_neigh;

	 
	struct kref refcount;

	 
	struct rcu_head rcu;
};

 
struct batadv_neigh_ifinfo_bat_iv {
	 
	u8 tq_recv[BATADV_TQ_GLOBAL_WINDOW_SIZE];

	 
	u8 tq_index;

	 
	u8 tq_avg;

	 
	DECLARE_BITMAP(real_bits, BATADV_TQ_LOCAL_WINDOW_SIZE);

	 
	u8 real_packet_count;
};

 
struct batadv_neigh_ifinfo_bat_v {
	 
	u32 throughput;

	 
	u32 last_seqno;
};

 
struct batadv_neigh_ifinfo {
	 
	struct hlist_node list;

	 
	struct batadv_hard_iface *if_outgoing;

	 
	struct batadv_neigh_ifinfo_bat_iv bat_iv;

#ifdef CONFIG_BATMAN_ADV_BATMAN_V
	 
	struct batadv_neigh_ifinfo_bat_v bat_v;
#endif

	 
	u8 last_ttl;

	 
	struct kref refcount;

	 
	struct rcu_head rcu;
};

#ifdef CONFIG_BATMAN_ADV_BLA

 
struct batadv_bcast_duplist_entry {
	 
	u8 orig[ETH_ALEN];

	 
	__be32 crc;

	 
	unsigned long entrytime;
};
#endif

 
enum batadv_counters {
	 
	BATADV_CNT_TX,

	 
	BATADV_CNT_TX_BYTES,

	 
	BATADV_CNT_TX_DROPPED,

	 
	BATADV_CNT_RX,

	 
	BATADV_CNT_RX_BYTES,

	 
	BATADV_CNT_FORWARD,

	 
	BATADV_CNT_FORWARD_BYTES,

	 
	BATADV_CNT_MGMT_TX,

	 
	BATADV_CNT_MGMT_TX_BYTES,

	 
	BATADV_CNT_MGMT_RX,

	 
	BATADV_CNT_MGMT_RX_BYTES,

	 
	BATADV_CNT_FRAG_TX,

	 
	BATADV_CNT_FRAG_TX_BYTES,

	 
	BATADV_CNT_FRAG_RX,

	 
	BATADV_CNT_FRAG_RX_BYTES,

	 
	BATADV_CNT_FRAG_FWD,

	 
	BATADV_CNT_FRAG_FWD_BYTES,

	 
	BATADV_CNT_TT_REQUEST_TX,

	 
	BATADV_CNT_TT_REQUEST_RX,

	 
	BATADV_CNT_TT_RESPONSE_TX,

	 
	BATADV_CNT_TT_RESPONSE_RX,

	 
	BATADV_CNT_TT_ROAM_ADV_TX,

	 
	BATADV_CNT_TT_ROAM_ADV_RX,

#ifdef CONFIG_BATMAN_ADV_DAT
	 
	BATADV_CNT_DAT_GET_TX,

	 
	BATADV_CNT_DAT_GET_RX,

	 
	BATADV_CNT_DAT_PUT_TX,

	 
	BATADV_CNT_DAT_PUT_RX,

	 
	BATADV_CNT_DAT_CACHED_REPLY_TX,
#endif

#ifdef CONFIG_BATMAN_ADV_NC
	 
	BATADV_CNT_NC_CODE,

	 
	BATADV_CNT_NC_CODE_BYTES,

	 
	BATADV_CNT_NC_RECODE,

	 
	BATADV_CNT_NC_RECODE_BYTES,

	 
	BATADV_CNT_NC_BUFFER,

	 
	BATADV_CNT_NC_DECODE,

	 
	BATADV_CNT_NC_DECODE_BYTES,

	 
	BATADV_CNT_NC_DECODE_FAILED,

	 
	BATADV_CNT_NC_SNIFFED,
#endif

	 
	BATADV_CNT_NUM,
};

 
struct batadv_priv_tt {
	 
	atomic_t vn;

	 
	atomic_t ogm_append_cnt;

	 
	atomic_t local_changes;

	 
	struct list_head changes_list;

	 
	struct batadv_hashtable *local_hash;

	 
	struct batadv_hashtable *global_hash;

	 
	struct hlist_head req_list;

	 
	struct list_head roam_list;

	 
	spinlock_t changes_list_lock;

	 
	spinlock_t req_list_lock;

	 
	spinlock_t roam_list_lock;

	 
	unsigned char *last_changeset;

	 
	s16 last_changeset_len;

	 
	spinlock_t last_changeset_lock;

	 
	spinlock_t commit_lock;

	 
	struct delayed_work work;
};

#ifdef CONFIG_BATMAN_ADV_BLA

 
struct batadv_priv_bla {
	 
	atomic_t num_requests;

	 
	struct batadv_hashtable *claim_hash;

	 
	struct batadv_hashtable *backbone_hash;

	 
	u8 loopdetect_addr[ETH_ALEN];

	 
	unsigned long loopdetect_lasttime;

	 
	atomic_t loopdetect_next;

	 
	struct batadv_bcast_duplist_entry bcast_duplist[BATADV_DUPLIST_SIZE];

	 
	int bcast_duplist_curr;

	 
	spinlock_t bcast_duplist_lock;

	 
	struct batadv_bla_claim_dst claim_dest;

	 
	struct delayed_work work;
};
#endif

#ifdef CONFIG_BATMAN_ADV_DEBUG

 
struct batadv_priv_debug_log {
	 
	char log_buff[BATADV_LOG_BUF_LEN];

	 
	unsigned long log_start;

	 
	unsigned long log_end;

	 
	spinlock_t lock;

	 
	wait_queue_head_t queue_wait;
};
#endif

 
struct batadv_priv_gw {
	 
	struct hlist_head gateway_list;

	 
	spinlock_t list_lock;

	 
	struct batadv_gw_node __rcu *curr_gw;

	 
	unsigned int generation;

	 
	atomic_t mode;

	 
	atomic_t sel_class;

	 
	atomic_t bandwidth_down;

	 
	atomic_t bandwidth_up;

	 
	atomic_t reselect;
};

 
struct batadv_priv_tvlv {
	 
	struct hlist_head container_list;

	 
	struct hlist_head handler_list;

	 
	spinlock_t container_list_lock;

	 
	spinlock_t handler_list_lock;
};

#ifdef CONFIG_BATMAN_ADV_DAT

 
struct batadv_priv_dat {
	 
	batadv_dat_addr_t addr;

	 
	struct batadv_hashtable *hash;

	 
	struct delayed_work work;
};
#endif

#ifdef CONFIG_BATMAN_ADV_MCAST
 
struct batadv_mcast_querier_state {
	 
	unsigned char exists:1;

	 
	unsigned char shadowing:1;
};

 
struct batadv_mcast_mla_flags {
	 
	struct batadv_mcast_querier_state querier_ipv4;

	 
	struct batadv_mcast_querier_state querier_ipv6;

	 
	unsigned char enabled:1;

	 
	unsigned char bridged:1;

	 
	u8 tvlv_flags;
};

 
struct batadv_priv_mcast {
	 
	struct hlist_head mla_list;  

	 
	struct hlist_head want_all_unsnoopables_list;

	 
	struct hlist_head want_all_ipv4_list;

	 
	struct hlist_head want_all_ipv6_list;

	 
	struct hlist_head want_all_rtr4_list;

	 
	struct hlist_head want_all_rtr6_list;

	 
	struct batadv_mcast_mla_flags mla_flags;

	 
	spinlock_t mla_lock;

	 
	atomic_t num_want_all_unsnoopables;

	 
	atomic_t num_want_all_ipv4;

	 
	atomic_t num_want_all_ipv6;

	 
	atomic_t num_want_all_rtr4;

	 
	atomic_t num_want_all_rtr6;

	 
	spinlock_t want_lists_lock;

	 
	struct delayed_work work;
};
#endif

 
struct batadv_priv_nc {
	 
	struct delayed_work work;

	 
	u8 min_tq;

	 
	u32 max_fwd_delay;

	 
	u32 max_buffer_time;

	 
	unsigned long timestamp_fwd_flush;

	 
	unsigned long timestamp_sniffed_purge;

	 
	struct batadv_hashtable *coding_hash;

	 
	struct batadv_hashtable *decoding_hash;
};

 
struct batadv_tp_unacked {
	 
	u32 seqno;

	 
	u16 len;

	 
	struct list_head list;
};

 
enum batadv_tp_meter_role {
	 
	BATADV_TP_RECEIVER,

	 
	BATADV_TP_SENDER
};

 
struct batadv_tp_vars {
	 
	struct hlist_node list;

	 
	struct timer_list timer;

	 
	struct batadv_priv *bat_priv;

	 
	unsigned long start_time;

	 
	u8 other_end[ETH_ALEN];

	 
	enum batadv_tp_meter_role role;

	 
	atomic_t sending;

	 
	enum batadv_tp_meter_reason reason;

	 
	struct delayed_work finish_work;

	 
	u32 test_length;

	 
	u8 session[2];

	 
	u8 icmp_uid;

	 

	 
	u16 dec_cwnd;

	 
	u32 cwnd;

	 
	spinlock_t cwnd_lock;

	 
	u32 ss_threshold;

	 
	atomic_t last_acked;

	 
	u32 last_sent;

	 
	atomic64_t tot_sent;

	 
	atomic_t dup_acks;

	 
	unsigned char fast_recovery:1;

	 
	u32 recover;

	 
	u32 rto;

	 
	u32 srtt;

	 
	u32 rttvar;

	 
	wait_queue_head_t more_bytes;

	 
	u32 prerandom_offset;

	 
	spinlock_t prerandom_lock;

	 

	 
	u32 last_recv;

	 
	struct list_head unacked_list;

	 
	spinlock_t unacked_lock;

	 
	unsigned long last_recv_time;

	 
	struct kref refcount;

	 
	struct rcu_head rcu;
};

 
struct batadv_softif_vlan {
	 
	struct batadv_priv *bat_priv;

	 
	unsigned short vid;

	 
	atomic_t ap_isolation;		 

	 
	struct batadv_vlan_tt tt;

	 
	struct hlist_node list;

	 
	struct kref refcount;

	 
	struct rcu_head rcu;
};

 
struct batadv_priv_bat_v {
	 
	unsigned char *ogm_buff;

	 
	int ogm_buff_len;

	 
	atomic_t ogm_seqno;

	 
	struct mutex ogm_buff_mutex;

	 
	struct delayed_work ogm_wq;
};

 
struct batadv_priv {
	 
	atomic_t mesh_state;

	 
	struct net_device *soft_iface;

	 
	int mtu_set_by_user;

	 
	u64 __percpu *bat_counters;  

	 
	atomic_t aggregated_ogms;

	 
	atomic_t bonding;

	 
	atomic_t fragmentation;

	 
	atomic_t packet_size_max;

	 
	atomic_t frag_seqno;

#ifdef CONFIG_BATMAN_ADV_BLA
	 
	atomic_t bridge_loop_avoidance;
#endif

#ifdef CONFIG_BATMAN_ADV_DAT
	 
	atomic_t distributed_arp_table;
#endif

#ifdef CONFIG_BATMAN_ADV_MCAST
	 
	atomic_t multicast_mode;

	 
	atomic_t multicast_fanout;
#endif

	 
	atomic_t orig_interval;

	 
	atomic_t hop_penalty;

#ifdef CONFIG_BATMAN_ADV_DEBUG
	 
	atomic_t log_level;
#endif

	 
	u32 isolation_mark;

	 
	u32 isolation_mark_mask;

	 
	atomic_t bcast_seqno;

	 
	atomic_t bcast_queue_left;

	 
	atomic_t batman_queue_left;

	 
	struct hlist_head forw_bat_list;

	 
	struct hlist_head forw_bcast_list;

	 
	struct hlist_head tp_list;

	 
	struct batadv_hashtable *orig_hash;

	 
	spinlock_t forw_bat_list_lock;

	 
	spinlock_t forw_bcast_list_lock;

	 
	spinlock_t tp_list_lock;

	 
	atomic_t tp_num;

	 
	struct delayed_work orig_work;

	 
	struct batadv_hard_iface __rcu *primary_if;   

	 
	struct batadv_algo_ops *algo_ops;

	 
	struct hlist_head softif_vlan_list;

	 
	spinlock_t softif_vlan_list_lock;

#ifdef CONFIG_BATMAN_ADV_BLA
	 
	struct batadv_priv_bla bla;
#endif

#ifdef CONFIG_BATMAN_ADV_DEBUG
	 
	struct batadv_priv_debug_log *debug_log;
#endif

	 
	struct batadv_priv_gw gw;

	 
	struct batadv_priv_tt tt;

	 
	struct batadv_priv_tvlv tvlv;

#ifdef CONFIG_BATMAN_ADV_DAT
	 
	struct batadv_priv_dat dat;
#endif

#ifdef CONFIG_BATMAN_ADV_MCAST
	 
	struct batadv_priv_mcast mcast;
#endif

#ifdef CONFIG_BATMAN_ADV_NC
	 
	atomic_t network_coding;

	 
	struct batadv_priv_nc nc;
#endif  

#ifdef CONFIG_BATMAN_ADV_BATMAN_V
	 
	struct batadv_priv_bat_v bat_v;
#endif
};

#ifdef CONFIG_BATMAN_ADV_BLA

 
struct batadv_bla_backbone_gw {
	 
	u8 orig[ETH_ALEN];

	 
	unsigned short vid;

	 
	struct hlist_node hash_entry;

	 
	struct batadv_priv *bat_priv;

	 
	unsigned long lasttime;

	 
	atomic_t wait_periods;

	 
	atomic_t request_sent;

	 
	u16 crc;

	 
	spinlock_t crc_lock;

	 
	struct work_struct report_work;

	 
	struct kref refcount;

	 
	struct rcu_head rcu;
};

 
struct batadv_bla_claim {
	 
	u8 addr[ETH_ALEN];

	 
	unsigned short vid;

	 
	struct batadv_bla_backbone_gw *backbone_gw;

	 
	spinlock_t backbone_lock;

	 
	unsigned long lasttime;

	 
	struct hlist_node hash_entry;

	 
	struct rcu_head rcu;

	 
	struct kref refcount;
};
#endif

 
struct batadv_tt_common_entry {
	 
	u8 addr[ETH_ALEN];

	 
	unsigned short vid;

	 
	struct hlist_node hash_entry;

	 
	u16 flags;

	 
	unsigned long added_at;

	 
	struct kref refcount;

	 
	struct rcu_head rcu;
};

 
struct batadv_tt_local_entry {
	 
	struct batadv_tt_common_entry common;

	 
	unsigned long last_seen;

	 
	struct batadv_softif_vlan *vlan;
};

 
struct batadv_tt_global_entry {
	 
	struct batadv_tt_common_entry common;

	 
	struct hlist_head orig_list;

	 
	atomic_t orig_list_count;

	 
	spinlock_t list_lock;

	 
	unsigned long roam_at;
};

 
struct batadv_tt_orig_list_entry {
	 
	struct batadv_orig_node *orig_node;

	 
	u8 ttvn;

	 
	u8 flags;

	 
	struct hlist_node list;

	 
	struct kref refcount;

	 
	struct rcu_head rcu;
};

 
struct batadv_tt_change_node {
	 
	struct list_head list;

	 
	struct batadv_tvlv_tt_change change;
};

 
struct batadv_tt_req_node {
	 
	u8 addr[ETH_ALEN];

	 
	unsigned long issued_at;

	 
	struct kref refcount;

	 
	struct hlist_node list;
};

 
struct batadv_tt_roam_node {
	 
	u8 addr[ETH_ALEN];

	 
	atomic_t counter;

	 
	unsigned long first_time;

	 
	struct list_head list;
};

 
struct batadv_nc_node {
	 
	struct list_head list;

	 
	u8 addr[ETH_ALEN];

	 
	struct kref refcount;

	 
	struct rcu_head rcu;

	 
	struct batadv_orig_node *orig_node;

	 
	unsigned long last_seen;
};

 
struct batadv_nc_path {
	 
	struct hlist_node hash_entry;

	 
	struct rcu_head rcu;

	 
	struct kref refcount;

	 
	struct list_head packet_list;

	 
	spinlock_t packet_list_lock;

	 
	u8 next_hop[ETH_ALEN];

	 
	u8 prev_hop[ETH_ALEN];

	 
	unsigned long last_valid;
};

 
struct batadv_nc_packet {
	 
	struct list_head list;

	 
	__be32 packet_id;

	 
	unsigned long timestamp;

	 
	struct batadv_neigh_node *neigh_node;

	 
	struct sk_buff *skb;

	 
	struct batadv_nc_path *nc_path;
};

 
struct batadv_skb_cb {
	 
	unsigned char decoded:1;

	 
	unsigned char num_bcasts;
};

 
struct batadv_forw_packet {
	 
	struct hlist_node list;

	 
	struct hlist_node cleanup_list;

	 
	unsigned long send_time;

	 
	u8 own;

	 
	struct sk_buff *skb;

	 
	u16 packet_len;

	 
	u32 direct_link_flags;

	 
	u8 num_packets;

	 
	struct delayed_work delayed_work;

	 
	struct batadv_hard_iface *if_incoming;

	 
	struct batadv_hard_iface *if_outgoing;

	 
	atomic_t *queue_left;
};

 
struct batadv_algo_iface_ops {
	 
	void (*activate)(struct batadv_hard_iface *hard_iface);

	 
	int (*enable)(struct batadv_hard_iface *hard_iface);

	 
	void (*enabled)(struct batadv_hard_iface *hard_iface);

	 
	void (*disable)(struct batadv_hard_iface *hard_iface);

	 
	void (*update_mac)(struct batadv_hard_iface *hard_iface);

	 
	void (*primary_set)(struct batadv_hard_iface *hard_iface);
};

 
struct batadv_algo_neigh_ops {
	 
	void (*hardif_init)(struct batadv_hardif_neigh_node *neigh);

	 
	int (*cmp)(struct batadv_neigh_node *neigh1,
		   struct batadv_hard_iface *if_outgoing1,
		   struct batadv_neigh_node *neigh2,
		   struct batadv_hard_iface *if_outgoing2);

	 
	bool (*is_similar_or_better)(struct batadv_neigh_node *neigh1,
				     struct batadv_hard_iface *if_outgoing1,
				     struct batadv_neigh_node *neigh2,
				     struct batadv_hard_iface *if_outgoing2);

	 
	void (*dump)(struct sk_buff *msg, struct netlink_callback *cb,
		     struct batadv_priv *priv,
		     struct batadv_hard_iface *hard_iface);
};

 
struct batadv_algo_orig_ops {
	 
	void (*dump)(struct sk_buff *msg, struct netlink_callback *cb,
		     struct batadv_priv *priv,
		     struct batadv_hard_iface *hard_iface);
};

 
struct batadv_algo_gw_ops {
	 
	void (*init_sel_class)(struct batadv_priv *bat_priv);

	 
	u32 sel_class_max;

	 
	struct batadv_gw_node *(*get_best_gw_node)
		(struct batadv_priv *bat_priv);

	 
	bool (*is_eligible)(struct batadv_priv *bat_priv,
			    struct batadv_orig_node *curr_gw_orig,
			    struct batadv_orig_node *orig_node);

	 
	void (*dump)(struct sk_buff *msg, struct netlink_callback *cb,
		     struct batadv_priv *priv);
};

 
struct batadv_algo_ops {
	 
	struct hlist_node list;

	 
	char *name;

	 
	struct batadv_algo_iface_ops iface;

	 
	struct batadv_algo_neigh_ops neigh;

	 
	struct batadv_algo_orig_ops orig;

	 
	struct batadv_algo_gw_ops gw;
};

 
struct batadv_dat_entry {
	 
	__be32 ip;

	 
	u8 mac_addr[ETH_ALEN];

	 
	unsigned short vid;

	 
	unsigned long last_update;

	 
	struct hlist_node hash_entry;

	 
	struct kref refcount;

	 
	struct rcu_head rcu;
};

 
struct batadv_hw_addr {
	 
	struct hlist_node list;

	 
	unsigned char addr[ETH_ALEN];
};

 
struct batadv_dat_candidate {
	 
	int type;

	 
	struct batadv_orig_node *orig_node;
};

 
struct batadv_tvlv_container {
	 
	struct hlist_node list;

	 
	struct batadv_tvlv_hdr tvlv_hdr;

	 
	struct kref refcount;
};

 
struct batadv_tvlv_handler {
	 
	struct hlist_node list;

	 
	void (*ogm_handler)(struct batadv_priv *bat_priv,
			    struct batadv_orig_node *orig,
			    u8 flags, void *tvlv_value, u16 tvlv_value_len);

	 
	int (*unicast_handler)(struct batadv_priv *bat_priv,
			       u8 *src, u8 *dst,
			       void *tvlv_value, u16 tvlv_value_len);

	 
	int (*mcast_handler)(struct batadv_priv *bat_priv, struct sk_buff *skb);

	 
	u8 type;

	 
	u8 version;

	 
	u8 flags;

	 
	struct kref refcount;

	 
	struct rcu_head rcu;
};

 
enum batadv_tvlv_handler_flags {
	 
	BATADV_TVLV_HANDLER_OGM_CIFNOTFND = BIT(1),

	 
	BATADV_TVLV_HANDLER_OGM_CALLED = BIT(2),
};

#endif  
