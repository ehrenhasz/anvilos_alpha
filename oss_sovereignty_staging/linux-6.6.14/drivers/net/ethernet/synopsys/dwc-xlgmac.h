 

#ifndef __DWC_XLGMAC_H__
#define __DWC_XLGMAC_H__

#include <linux/dma-mapping.h>
#include <linux/netdevice.h>
#include <linux/workqueue.h>
#include <linux/phy.h>
#include <linux/if_vlan.h>
#include <linux/bitops.h>
#include <linux/timecounter.h>

#define XLGMAC_DRV_NAME			"dwc-xlgmac"
#define XLGMAC_DRV_VERSION		"1.0.0"
#define XLGMAC_DRV_DESC			"Synopsys DWC XLGMAC Driver"

 
#define XLGMAC_TX_DESC_CNT		1024
#define XLGMAC_TX_DESC_MIN_FREE		(XLGMAC_TX_DESC_CNT >> 3)
#define XLGMAC_TX_DESC_MAX_PROC		(XLGMAC_TX_DESC_CNT >> 1)
#define XLGMAC_RX_DESC_CNT		1024
#define XLGMAC_RX_DESC_MAX_DIRTY	(XLGMAC_RX_DESC_CNT >> 3)

 
#define XLGMAC_TX_MAX_SPLIT	\
	((GSO_LEGACY_MAX_SIZE / XLGMAC_TX_MAX_BUF_SIZE) + 1)

 
#define XLGMAC_TX_MAX_DESC_NR	(MAX_SKB_FRAGS + XLGMAC_TX_MAX_SPLIT + 2)

#define XLGMAC_TX_MAX_BUF_SIZE	(0x3fff & ~(64 - 1))
#define XLGMAC_RX_MIN_BUF_SIZE	(ETH_FRAME_LEN + ETH_FCS_LEN + VLAN_HLEN)
#define XLGMAC_RX_BUF_ALIGN	64

 
#define XLGMAC_SPH_HDSMS_SIZE		3
#define XLGMAC_SKB_ALLOC_SIZE		512

#define XLGMAC_MAX_FIFO			81920

#define XLGMAC_MAX_DMA_CHANNELS		16
#define XLGMAC_DMA_STOP_TIMEOUT		5
#define XLGMAC_DMA_INTERRUPT_MASK	0x31c7

 
#define XLGMAC_INIT_DMA_TX_USECS	1000
#define XLGMAC_INIT_DMA_TX_FRAMES	25
#define XLGMAC_INIT_DMA_RX_USECS	30
#define XLGMAC_INIT_DMA_RX_FRAMES	25
#define XLGMAC_MAX_DMA_RIWT		0xff
#define XLGMAC_MIN_DMA_RIWT		0x01

 
#define XLGMAC_MAX_FLOW_CONTROL_QUEUES	8

 
#define XLGMAC_SYSCLOCK			125000000

 
#define XLGMAC_MAC_HASH_TABLE_SIZE	8

 
#define XLGMAC_RSS_HASH_KEY_SIZE	40
#define XLGMAC_RSS_MAX_TABLE_SIZE	256
#define XLGMAC_RSS_LOOKUP_TABLE_TYPE	0
#define XLGMAC_RSS_HASH_KEY_TYPE	1

#define XLGMAC_STD_PACKET_MTU		1500
#define XLGMAC_JUMBO_PACKET_MTU		9000

 
#define XLGMAC_GET_DESC_DATA(ring, idx) ({				\
	typeof(ring) _ring = (ring);					\
	((_ring)->desc_data_head +					\
	 ((idx) & ((_ring)->dma_desc_count - 1)));			\
})

#define XLGMAC_GET_REG_BITS(var, pos, len) ({				\
	typeof(pos) _pos = (pos);					\
	typeof(len) _len = (len);					\
	((var) & GENMASK(_pos + _len - 1, _pos)) >> (_pos);		\
})

#define XLGMAC_GET_REG_BITS_LE(var, pos, len) ({			\
	typeof(pos) _pos = (pos);					\
	typeof(len) _len = (len);					\
	typeof(var) _var = le32_to_cpu((var));				\
	((_var) & GENMASK(_pos + _len - 1, _pos)) >> (_pos);		\
})

#define XLGMAC_SET_REG_BITS(var, pos, len, val) ({			\
	typeof(var) _var = (var);					\
	typeof(pos) _pos = (pos);					\
	typeof(len) _len = (len);					\
	typeof(val) _val = (val);					\
	_val = (_val << _pos) & GENMASK(_pos + _len - 1, _pos);		\
	_var = (_var & ~GENMASK(_pos + _len - 1, _pos)) | _val;		\
})

#define XLGMAC_SET_REG_BITS_LE(var, pos, len, val) ({			\
	typeof(var) _var = (var);					\
	typeof(pos) _pos = (pos);					\
	typeof(len) _len = (len);					\
	typeof(val) _val = (val);					\
	_val = (_val << _pos) & GENMASK(_pos + _len - 1, _pos);		\
	_var = (_var & ~GENMASK(_pos + _len - 1, _pos)) | _val;		\
	cpu_to_le32(_var);						\
})

struct xlgmac_pdata;

enum xlgmac_int {
	XLGMAC_INT_DMA_CH_SR_TI,
	XLGMAC_INT_DMA_CH_SR_TPS,
	XLGMAC_INT_DMA_CH_SR_TBU,
	XLGMAC_INT_DMA_CH_SR_RI,
	XLGMAC_INT_DMA_CH_SR_RBU,
	XLGMAC_INT_DMA_CH_SR_RPS,
	XLGMAC_INT_DMA_CH_SR_TI_RI,
	XLGMAC_INT_DMA_CH_SR_FBE,
	XLGMAC_INT_DMA_ALL,
};

struct xlgmac_stats {
	 
	u64 txoctetcount_gb;
	u64 txframecount_gb;
	u64 txbroadcastframes_g;
	u64 txmulticastframes_g;
	u64 tx64octets_gb;
	u64 tx65to127octets_gb;
	u64 tx128to255octets_gb;
	u64 tx256to511octets_gb;
	u64 tx512to1023octets_gb;
	u64 tx1024tomaxoctets_gb;
	u64 txunicastframes_gb;
	u64 txmulticastframes_gb;
	u64 txbroadcastframes_gb;
	u64 txunderflowerror;
	u64 txoctetcount_g;
	u64 txframecount_g;
	u64 txpauseframes;
	u64 txvlanframes_g;

	 
	u64 rxframecount_gb;
	u64 rxoctetcount_gb;
	u64 rxoctetcount_g;
	u64 rxbroadcastframes_g;
	u64 rxmulticastframes_g;
	u64 rxcrcerror;
	u64 rxrunterror;
	u64 rxjabbererror;
	u64 rxundersize_g;
	u64 rxoversize_g;
	u64 rx64octets_gb;
	u64 rx65to127octets_gb;
	u64 rx128to255octets_gb;
	u64 rx256to511octets_gb;
	u64 rx512to1023octets_gb;
	u64 rx1024tomaxoctets_gb;
	u64 rxunicastframes_g;
	u64 rxlengtherror;
	u64 rxoutofrangetype;
	u64 rxpauseframes;
	u64 rxfifooverflow;
	u64 rxvlanframes_gb;
	u64 rxwatchdogerror;

	 
	u64 tx_tso_packets;
	u64 rx_split_header_packets;
	u64 tx_process_stopped;
	u64 rx_process_stopped;
	u64 tx_buffer_unavailable;
	u64 rx_buffer_unavailable;
	u64 fatal_bus_error;
	u64 tx_vlan_packets;
	u64 rx_vlan_packets;
	u64 napi_poll_isr;
	u64 napi_poll_txtimer;
};

struct xlgmac_ring_buf {
	struct sk_buff *skb;
	dma_addr_t skb_dma;
	unsigned int skb_len;
};

 
struct xlgmac_dma_desc {
	__le32 desc0;
	__le32 desc1;
	__le32 desc2;
	__le32 desc3;
};

 
struct xlgmac_page_alloc {
	struct page *pages;
	unsigned int pages_len;
	unsigned int pages_offset;

	dma_addr_t pages_dma;
};

 
struct xlgmac_buffer_data {
	struct xlgmac_page_alloc pa;
	struct xlgmac_page_alloc pa_unmap;

	dma_addr_t dma_base;
	unsigned long dma_off;
	unsigned int dma_len;
};

 
struct xlgmac_tx_desc_data {
	unsigned int packets;		 
	unsigned int bytes;		 
};

 
struct xlgmac_rx_desc_data {
	struct xlgmac_buffer_data hdr;	 
	struct xlgmac_buffer_data buf;	 

	unsigned short hdr_len;		 
	unsigned short len;		 
};

struct xlgmac_pkt_info {
	struct sk_buff *skb;

	unsigned int attributes;

	unsigned int errors;

	 
	unsigned int desc_count;
	unsigned int length;

	unsigned int tx_packets;
	unsigned int tx_bytes;

	unsigned int header_len;
	unsigned int tcp_header_len;
	unsigned int tcp_payload_len;
	unsigned short mss;

	unsigned short vlan_ctag;

	u64 rx_tstamp;

	u32 rss_hash;
	enum pkt_hash_types rss_hash_type;
};

struct xlgmac_desc_data {
	 
	struct xlgmac_dma_desc *dma_desc;
	dma_addr_t dma_desc_addr;

	 
	struct sk_buff *skb;
	dma_addr_t skb_dma;
	unsigned int skb_dma_len;

	 
	struct xlgmac_tx_desc_data tx;
	struct xlgmac_rx_desc_data rx;

	unsigned int mapped_as_page;

	 
	unsigned int state_saved;
	struct {
		struct sk_buff *skb;
		unsigned int len;
		unsigned int error;
	} state;
};

struct xlgmac_ring {
	 
	struct xlgmac_pkt_info pkt_info;

	 
	struct xlgmac_dma_desc *dma_desc_head;
	dma_addr_t dma_desc_head_addr;
	unsigned int dma_desc_count;

	 
	struct xlgmac_desc_data *desc_data_head;

	 
	struct xlgmac_page_alloc rx_hdr_pa;
	struct xlgmac_page_alloc rx_buf_pa;

	 
	unsigned int cur;
	unsigned int dirty;

	 
	unsigned int coalesce_count;

	union {
		struct {
			unsigned int xmit_more;
			unsigned int queue_stopped;
			unsigned short cur_mss;
			unsigned short cur_vlan_ctag;
		} tx;
	};
} ____cacheline_aligned;

struct xlgmac_channel {
	char name[16];

	 
	struct xlgmac_pdata *pdata;

	 
	unsigned int queue_index;
	void __iomem *dma_regs;

	 
	int dma_irq;
	char dma_irq_name[IFNAMSIZ + 32];

	 
	struct napi_struct napi;

	unsigned int saved_ier;

	unsigned int tx_timer_active;
	struct timer_list tx_timer;

	struct xlgmac_ring *tx_ring;
	struct xlgmac_ring *rx_ring;
} ____cacheline_aligned;

struct xlgmac_desc_ops {
	int (*alloc_channels_and_rings)(struct xlgmac_pdata *pdata);
	void (*free_channels_and_rings)(struct xlgmac_pdata *pdata);
	int (*map_tx_skb)(struct xlgmac_channel *channel,
			  struct sk_buff *skb);
	int (*map_rx_buffer)(struct xlgmac_pdata *pdata,
			     struct xlgmac_ring *ring,
			struct xlgmac_desc_data *desc_data);
	void (*unmap_desc_data)(struct xlgmac_pdata *pdata,
				struct xlgmac_desc_data *desc_data);
	void (*tx_desc_init)(struct xlgmac_pdata *pdata);
	void (*rx_desc_init)(struct xlgmac_pdata *pdata);
};

struct xlgmac_hw_ops {
	int (*init)(struct xlgmac_pdata *pdata);
	int (*exit)(struct xlgmac_pdata *pdata);

	int (*tx_complete)(struct xlgmac_dma_desc *dma_desc);

	void (*enable_tx)(struct xlgmac_pdata *pdata);
	void (*disable_tx)(struct xlgmac_pdata *pdata);
	void (*enable_rx)(struct xlgmac_pdata *pdata);
	void (*disable_rx)(struct xlgmac_pdata *pdata);

	int (*enable_int)(struct xlgmac_channel *channel,
			  enum xlgmac_int int_id);
	int (*disable_int)(struct xlgmac_channel *channel,
			   enum xlgmac_int int_id);
	void (*dev_xmit)(struct xlgmac_channel *channel);
	int (*dev_read)(struct xlgmac_channel *channel);

	int (*set_mac_address)(struct xlgmac_pdata *pdata, const u8 *addr);
	int (*config_rx_mode)(struct xlgmac_pdata *pdata);
	int (*enable_rx_csum)(struct xlgmac_pdata *pdata);
	int (*disable_rx_csum)(struct xlgmac_pdata *pdata);

	 
	int (*set_xlgmii_25000_speed)(struct xlgmac_pdata *pdata);
	int (*set_xlgmii_40000_speed)(struct xlgmac_pdata *pdata);
	int (*set_xlgmii_50000_speed)(struct xlgmac_pdata *pdata);
	int (*set_xlgmii_100000_speed)(struct xlgmac_pdata *pdata);

	 
	void (*tx_desc_init)(struct xlgmac_channel *channel);
	void (*rx_desc_init)(struct xlgmac_channel *channel);
	void (*tx_desc_reset)(struct xlgmac_desc_data *desc_data);
	void (*rx_desc_reset)(struct xlgmac_pdata *pdata,
			      struct xlgmac_desc_data *desc_data,
			unsigned int index);
	int (*is_last_desc)(struct xlgmac_dma_desc *dma_desc);
	int (*is_context_desc)(struct xlgmac_dma_desc *dma_desc);
	void (*tx_start_xmit)(struct xlgmac_channel *channel,
			      struct xlgmac_ring *ring);

	 
	int (*config_tx_flow_control)(struct xlgmac_pdata *pdata);
	int (*config_rx_flow_control)(struct xlgmac_pdata *pdata);

	 
	int (*enable_rx_vlan_stripping)(struct xlgmac_pdata *pdata);
	int (*disable_rx_vlan_stripping)(struct xlgmac_pdata *pdata);
	int (*enable_rx_vlan_filtering)(struct xlgmac_pdata *pdata);
	int (*disable_rx_vlan_filtering)(struct xlgmac_pdata *pdata);
	int (*update_vlan_hash_table)(struct xlgmac_pdata *pdata);

	 
	int (*config_rx_coalesce)(struct xlgmac_pdata *pdata);
	int (*config_tx_coalesce)(struct xlgmac_pdata *pdata);
	unsigned int (*usec_to_riwt)(struct xlgmac_pdata *pdata,
				     unsigned int usec);
	unsigned int (*riwt_to_usec)(struct xlgmac_pdata *pdata,
				     unsigned int riwt);

	 
	int (*config_rx_threshold)(struct xlgmac_pdata *pdata,
				   unsigned int val);
	int (*config_tx_threshold)(struct xlgmac_pdata *pdata,
				   unsigned int val);

	 
	int (*config_rsf_mode)(struct xlgmac_pdata *pdata,
			       unsigned int val);
	int (*config_tsf_mode)(struct xlgmac_pdata *pdata,
			       unsigned int val);

	 
	int (*config_osp_mode)(struct xlgmac_pdata *pdata);

	 
	int (*config_rx_pbl_val)(struct xlgmac_pdata *pdata);
	int (*get_rx_pbl_val)(struct xlgmac_pdata *pdata);
	int (*config_tx_pbl_val)(struct xlgmac_pdata *pdata);
	int (*get_tx_pbl_val)(struct xlgmac_pdata *pdata);
	int (*config_pblx8)(struct xlgmac_pdata *pdata);

	 
	void (*rx_mmc_int)(struct xlgmac_pdata *pdata);
	void (*tx_mmc_int)(struct xlgmac_pdata *pdata);
	void (*read_mmc_stats)(struct xlgmac_pdata *pdata);

	 
	int (*enable_rss)(struct xlgmac_pdata *pdata);
	int (*disable_rss)(struct xlgmac_pdata *pdata);
	int (*set_rss_hash_key)(struct xlgmac_pdata *pdata,
				const u8 *key);
	int (*set_rss_lookup_table)(struct xlgmac_pdata *pdata,
				    const u32 *table);
};

 
struct xlgmac_hw_features {
	 
	unsigned int version;

	 
	unsigned int phyifsel;		 
	unsigned int vlhash;		 
	unsigned int sma;		 
	unsigned int rwk;		 
	unsigned int mgk;		 
	unsigned int mmc;		 
	unsigned int aoe;		 
	unsigned int ts;		 
	unsigned int eee;		 
	unsigned int tx_coe;		 
	unsigned int rx_coe;		 
	unsigned int addn_mac;		 
	unsigned int ts_src;		 
	unsigned int sa_vlan_ins;	 

	 
	unsigned int rx_fifo_size;	 
	unsigned int tx_fifo_size;	 
	unsigned int adv_ts_hi;		 
	unsigned int dma_width;		 
	unsigned int dcb;		 
	unsigned int sph;		 
	unsigned int tso;		 
	unsigned int dma_debug;		 
	unsigned int rss;		 
	unsigned int tc_cnt;		 
	unsigned int hash_table_size;	 
	unsigned int l3l4_filter_num;	 

	 
	unsigned int rx_q_cnt;		 
	unsigned int tx_q_cnt;		 
	unsigned int rx_ch_cnt;		 
	unsigned int tx_ch_cnt;		 
	unsigned int pps_out_num;	 
	unsigned int aux_snap_num;	 
};

struct xlgmac_resources {
	void __iomem *addr;
	int irq;
};

struct xlgmac_pdata {
	struct net_device *netdev;
	struct device *dev;

	struct xlgmac_hw_ops hw_ops;
	struct xlgmac_desc_ops desc_ops;

	 
	struct xlgmac_stats stats;

	u32 msg_enable;

	 
	void __iomem *mac_regs;

	 
	struct xlgmac_hw_features hw_feat;

	struct work_struct restart_work;

	 
	struct xlgmac_channel *channel_head;
	unsigned int channel_count;
	unsigned int tx_ring_count;
	unsigned int rx_ring_count;
	unsigned int tx_desc_count;
	unsigned int rx_desc_count;
	unsigned int tx_q_count;
	unsigned int rx_q_count;

	 
	unsigned int pblx8;

	 
	unsigned int tx_sf_mode;
	unsigned int tx_threshold;
	unsigned int tx_pbl;
	unsigned int tx_osp_mode;

	 
	unsigned int rx_sf_mode;
	unsigned int rx_threshold;
	unsigned int rx_pbl;

	 
	unsigned int tx_usecs;
	unsigned int tx_frames;

	 
	unsigned int rx_riwt;
	unsigned int rx_usecs;
	unsigned int rx_frames;

	 
	unsigned int rx_buf_size;

	 
	unsigned int tx_pause;
	unsigned int rx_pause;

	 
	int dev_irq;
	unsigned int per_channel_irq;
	int channel_irq[XLGMAC_MAX_DMA_CHANNELS];

	 
	unsigned char mac_addr[ETH_ALEN];
	netdev_features_t netdev_features;
	struct napi_struct napi;

	 
	unsigned long active_vlans[BITS_TO_LONGS(VLAN_N_VID)];

	 
	unsigned long sysclk_rate;

	 
	struct mutex rss_mutex;

	 
	u8 rss_key[XLGMAC_RSS_HASH_KEY_SIZE];
	u32 rss_table[XLGMAC_RSS_MAX_TABLE_SIZE];
	u32 rss_options;

	int phy_speed;

	char drv_name[32];
	char drv_ver[32];
};

void xlgmac_init_desc_ops(struct xlgmac_desc_ops *desc_ops);
void xlgmac_init_hw_ops(struct xlgmac_hw_ops *hw_ops);
const struct net_device_ops *xlgmac_get_netdev_ops(void);
const struct ethtool_ops *xlgmac_get_ethtool_ops(void);
void xlgmac_dump_tx_desc(struct xlgmac_pdata *pdata,
			 struct xlgmac_ring *ring,
			 unsigned int idx,
			 unsigned int count,
			 unsigned int flag);
void xlgmac_dump_rx_desc(struct xlgmac_pdata *pdata,
			 struct xlgmac_ring *ring,
			 unsigned int idx);
void xlgmac_print_pkt(struct net_device *netdev,
		      struct sk_buff *skb, bool tx_rx);
void xlgmac_get_all_hw_features(struct xlgmac_pdata *pdata);
void xlgmac_print_all_hw_features(struct xlgmac_pdata *pdata);
int xlgmac_drv_probe(struct device *dev,
		     struct xlgmac_resources *res);
int xlgmac_drv_remove(struct device *dev);

 
#ifdef XLGMAC_DEBUG
#define XLGMAC_PR(fmt, args...) \
	pr_alert("[%s,%d]:" fmt, __func__, __LINE__, ## args)
#else
#define XLGMAC_PR(x...)		do { } while (0)
#endif

#endif  
