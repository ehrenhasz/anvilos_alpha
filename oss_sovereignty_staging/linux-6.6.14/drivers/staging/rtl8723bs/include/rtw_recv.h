 
 
#ifndef _RTW_RECV_H_
#define _RTW_RECV_H_

#define NR_RECVBUFF (8)

#define NR_PREALLOC_RECV_SKB (8)

#define NR_RECVFRAME 256

#define RXFRAME_ALIGN	8
#define RXFRAME_ALIGN_SZ	(1<<RXFRAME_ALIGN)

#define DRVINFO_SZ	4  

#define MAX_RXFRAME_CNT	512
#define MAX_RX_NUMBLKS		(32)
#define RECVFRAME_HDR_ALIGN 128


#define PHY_RSSI_SLID_WIN_MAX				100
#define PHY_LINKQUALITY_SLID_WIN_MAX		20


#define SNAP_SIZE sizeof(struct ieee80211_snap_hdr)

#define RX_MPDU_QUEUE				0
#define RX_CMD_QUEUE				1
#define RX_MAX_QUEUE				2

#define MAX_SUBFRAME_COUNT	64

#define LLC_HEADER_LENGTH	6

 
struct recv_reorder_ctrl {
	struct adapter	*padapter;
	u8 enable;
	u16 indicate_seq; 
	u16 wend_b;
	u8 wsize_b;
	struct __queue pending_recvframe_queue;
	struct timer_list reordering_ctrl_timer;
};

struct	stainfo_rxcache	{
	u16 tid_rxseq[16];
 
};


struct signal_stat {
	u8 update_req;		 
	u8 avg_val;		 
	u32 total_num;		 
	u32 total_val;		 
};

struct phy_info {
	u8 rx_pwd_ba11;

	u8 SignalQuality;	  
	s8		rx_mimo_signal_quality[4];	 
	u8 RxMIMOEVMdbm[4];		 

	u8 rx_mimo_signal_strength[4]; 

	u16 	Cfo_short[4];			 
	u16 	Cfo_tail[4];			 

	s8		RxPower;  
	s8		RecvSignalPower; 
	u8 bt_rx_rssi_percentage;
	u8 SignalStrength;  

	s8		RxPwr[4];				 
	u8 RxSNR[4];				 
	u8 BandWidth;
	u8 btCoexPwrAdjust;
};

#ifdef DBG_RX_SIGNAL_DISPLAY_RAW_DATA
struct rx_raw_rssi {
	u8 data_rate;
	u8 pwdball;
	s8 pwr_all;

	u8 mimo_signal_strength[4]; 
	u8 mimo_signal_quality[4];

	s8 ofdm_pwr[4];
	u8 ofdm_snr[4];

};
#endif

struct rx_pkt_attrib	{
	u16 pkt_len;
	u8 physt;
	u8 drvinfo_sz;
	u8 shift_sz;
	u8 hdrlen;  
	u8 to_fr_ds;
	u8 amsdu;
	u8 qos;
	u8 priority;
	u8 pw_save;
	u8 mdata;
	u16 seq_num;
	u8 frag_num;
	u8 mfrag;
	u8 order;
	u8 privacy;  
	u8 bdecrypted;
	u8 encrypt;  
	u8 iv_len;
	u8 icv_len;
	u8 crc_err;
	u8 icv_err;

	u16 eth_type;

	u8 dst[ETH_ALEN];
	u8 src[ETH_ALEN];
	u8 ta[ETH_ALEN];
	u8 ra[ETH_ALEN];
	u8 bssid[ETH_ALEN];

	u8 ack_policy;

	u8 key_index;

	u8 data_rate;
	u8 sgi;
	u8 pkt_rpt_type;
	u32 MacIDValidEntry[2];	 

 
	struct phy_info phy_info;
};


 
#define SN_LESS(a, b)		(((a - b) & 0x800) != 0)
#define SN_EQUAL(a, b)	(a == b)
 
 
#define REORDER_WAIT_TIME	(50)  

#define RECVBUFF_ALIGN_SZ 8

#define RXDESC_SIZE	24
#define RXDESC_OFFSET RXDESC_SIZE

struct recv_stat {
	__le32 rxdw0;
	__le32 rxdw1;
	__le32 rxdw2;
	__le32 rxdw3;
#ifndef BUF_DESC_ARCH
	__le32 rxdw4;
	__le32 rxdw5;
#endif  
};

#define EOR BIT(30)

 
struct recv_priv {
	spinlock_t	lock;
	struct __queue	free_recv_queue;
	struct __queue	recv_pending_queue;
	struct __queue	uc_swdec_pending_queue;
	u8 *pallocated_frame_buf;
	u8 *precv_frame_buf;
	uint free_recvframe_cnt;
	struct adapter	*adapter;
	u32 bIsAnyNonBEPkts;
	u64	rx_bytes;
	u64	rx_pkts;
	u64	rx_drop;
	uint  rx_icv_err;
	uint  rx_largepacket_crcerr;
	uint  rx_smallpacket_crcerr;
	uint  rx_middlepacket_crcerr;

	struct tasklet_struct irq_prepare_beacon_tasklet;
	struct tasklet_struct recv_tasklet;
	struct sk_buff_head free_recv_skb_queue;
	struct sk_buff_head rx_skb_queue;

	u8 *pallocated_recv_buf;
	u8 *precv_buf;     
	struct __queue	free_recv_buf_queue;
	u32 free_recv_buf_queue_cnt;

	struct __queue	recv_buf_pending_queue;

	 
	u8 is_signal_dbg;	 
	u8 signal_strength_dbg;	 

	u8 signal_strength;
	u8 signal_qual;
	s8 rssi;	 
	#ifdef DBG_RX_SIGNAL_DISPLAY_RAW_DATA
	struct rx_raw_rssi raw_rssi_info;
	#endif
	 
	s16 noise;
	 
	 
	 


	struct timer_list signal_stat_timer;
	u32 signal_stat_sampling_interval;
	 
	struct signal_stat signal_qual_data;
	struct signal_stat signal_strength_data;
};

#define rtw_set_signal_stat_timer(recvpriv) _set_timer(&(recvpriv)->signal_stat_timer, (recvpriv)->signal_stat_sampling_interval)

struct sta_recv_priv {

	spinlock_t	lock;
	signed int	option;

	 
	struct __queue defrag_q;	  

	struct	stainfo_rxcache rxcache;

	 
	 
	 

};


struct recv_buf {
	struct list_head list;

	spinlock_t recvbuf_lock;

	u32 ref_cnt;

	struct adapter *adapter;

	u8 *pbuf;
	u8 *pallocated_buf;

	u32 len;
	u8 *phead;
	u8 *pdata;
	u8 *ptail;
	u8 *pend;

	struct sk_buff	*pskb;
	u8 reuse;
};


 
struct recv_frame_hdr {
	struct list_head	list;
	struct sk_buff	 *pkt;
	struct sk_buff	 *pkt_newalloc;

	struct adapter  *adapter;

	u8 fragcnt;

	int frame_tag;

	struct rx_pkt_attrib attrib;

	uint  len;
	u8 *rx_head;
	u8 *rx_data;
	u8 *rx_tail;
	u8 *rx_end;

	void *precvbuf;


	 
	struct sta_info *psta;

	 
	struct recv_reorder_ctrl *preorder_ctrl;
};


union recv_frame {
	union{
		struct list_head list;
		struct recv_frame_hdr hdr;
		uint mem[RECVFRAME_HDR_ALIGN>>2];
	} u;

	 

};

enum {
	NORMAL_RX, 
	TX_REPORT1, 
	TX_REPORT2, 
	HIS_REPORT, 
	C2H_PACKET
};

extern union recv_frame *_rtw_alloc_recvframe(struct __queue *pfree_recv_queue);   
extern union recv_frame *rtw_alloc_recvframe(struct __queue *pfree_recv_queue);   
extern int	 rtw_free_recvframe(union recv_frame *precvframe, struct __queue *pfree_recv_queue);

#define rtw_dequeue_recvframe(queue) rtw_alloc_recvframe(queue)
extern int _rtw_enqueue_recvframe(union recv_frame *precvframe, struct __queue *queue);
extern int rtw_enqueue_recvframe(union recv_frame *precvframe, struct __queue *queue);

extern void rtw_free_recvframe_queue(struct __queue *pframequeue,  struct __queue *pfree_recv_queue);
u32 rtw_free_uc_swdec_pending_queue(struct adapter *adapter);

signed int rtw_enqueue_recvbuf_to_head(struct recv_buf *precvbuf, struct __queue *queue);
signed int rtw_enqueue_recvbuf(struct recv_buf *precvbuf, struct __queue *queue);
struct recv_buf *rtw_dequeue_recvbuf(struct __queue *queue);

void rtw_reordering_ctrl_timeout_handler(struct timer_list *t);

static inline u8 *get_rxmem(union recv_frame *precvframe)
{
	 
	if (precvframe == NULL)
		return NULL;

	return precvframe->u.hdr.rx_head;
}

static inline u8 *recvframe_pull(union recv_frame *precvframe, signed int sz)
{
	 

	 


	if (precvframe == NULL)
		return NULL;


	precvframe->u.hdr.rx_data += sz;

	if (precvframe->u.hdr.rx_data > precvframe->u.hdr.rx_tail) {
		precvframe->u.hdr.rx_data -= sz;
		return NULL;
	}

	precvframe->u.hdr.len -= sz;

	return precvframe->u.hdr.rx_data;

}

static inline u8 *recvframe_put(union recv_frame *precvframe, signed int sz)
{
	 

	 
	 
	unsigned char *prev_rx_tail;

	if (precvframe == NULL)
		return NULL;

	prev_rx_tail = precvframe->u.hdr.rx_tail;

	precvframe->u.hdr.rx_tail += sz;

	if (precvframe->u.hdr.rx_tail > precvframe->u.hdr.rx_end) {
		precvframe->u.hdr.rx_tail = prev_rx_tail;
		return NULL;
	}

	precvframe->u.hdr.len += sz;

	return precvframe->u.hdr.rx_tail;

}



static inline u8 *recvframe_pull_tail(union recv_frame *precvframe, signed int sz)
{
	 

	 
	 

	if (precvframe == NULL)
		return NULL;

	precvframe->u.hdr.rx_tail -= sz;

	if (precvframe->u.hdr.rx_tail < precvframe->u.hdr.rx_data) {
		precvframe->u.hdr.rx_tail += sz;
		return NULL;
	}

	precvframe->u.hdr.len -= sz;

	return precvframe->u.hdr.rx_tail;

}

static inline union recv_frame *rxmem_to_recvframe(u8 *rxmem)
{
	 
	 
	 

	return (union recv_frame *)(((SIZE_PTR)rxmem >> RXFRAME_ALIGN) << RXFRAME_ALIGN);

}

static inline signed int get_recvframe_len(union recv_frame *precvframe)
{
	return precvframe->u.hdr.len;
}


static inline s32 translate_percentage_to_dbm(u32 SignalStrengthIndex)
{
	s32	SignalPower;  

	 
	SignalPower = (s32)((SignalStrengthIndex + 1) >> 1);
	SignalPower -= 95;

	return SignalPower;
}


struct sta_info;

extern void _rtw_init_sta_recv_priv(struct sta_recv_priv *psta_recvpriv);

extern void  mgt_dispatcher(struct adapter *padapter, union recv_frame *precv_frame);

#endif
