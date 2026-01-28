#ifndef R819XUSB_CMDPKT_H
#define R819XUSB_CMDPKT_H
#define		CMPK_RX_TX_FB_SIZE		sizeof(struct cmd_pkt_tx_feedback)	 
#define		CMPK_BOTH_QUERY_CONFIG_SIZE	sizeof(struct cmd_pkt_set_configuration)	 
#define		CMPK_RX_TX_STS_SIZE		sizeof(cmpk_tx_status_t)
#define		CMPK_TX_RAHIS_SIZE		sizeof(cmpk_tx_rahis_t)
#define ISR_TX_BCN_OK		BIT(27)		 
#define ISR_TX_BCN_ERR		BIT(26)		 
#define ISR_BCN_TIMER_INTR	BIT(13)		 
struct cmd_pkt_tx_feedback {
	u8	element_id;			 
	u8	length;				 
	u8	TID:4;
	u8	fail_reason:3;
	u8	tok:1;				 
	u8	reserve1:4;
	u8	pkt_type:2;
	u8	bandwidth:1;
	u8	qos_pkt:1;
	u8	reserve2;
	u8	retry_cnt;
	u16	pkt_id;
	u16	seq_num;
	u8	s_rate;				 
	u8	f_rate;				 
	u8	s_rts_rate;
	u8	f_rts_rate;
	u16	pkt_length;
	u16	reserve3;
	u16	duration;
};
struct cmd_pkt_interrupt_status {
	u8	element_id;			 
	u8	length;				 
	u16	reserve;
	u32	interrupt_status;		 
};
struct cmd_pkt_set_configuration {
	u8	element_id;			 
	u8	length;				 
	u16	reserve1;
	u8	cfg_reserve1:3;
	u8	cfg_size:2;
	u8	cfg_type:2;
	u8	cfg_action:1;
	u8	cfg_reserve2;
	u8	cfg_page:4;
	u8	cfg_reserve3:4;
	u8	cfg_offset;
	u32	value;
	u32	mask;
};
#define		cmpk_query_cfg	cmd_pkt_set_configuration
typedef struct tag_tx_stats_feedback {
	u16	reserve1;
	u8	length;				 
	u8	element_id;			 
	u16	txfail;				 
	u16	txok;				 
	u16	txmcok;				 
	u16	txretry;			 
	u16	txucok;				 
	u16	txbcok;				 
	u16	txbcfail;
	u16	txmcfail;
	u16	reserve2;
	u16	txucfail;
	u32	txmclength;
	u32	txbclength;
	u32	txuclength;
	u16	reserve3_23;
	u8	reserve3_1;
	u8	rate;
} __packed cmpk_tx_status_t;
typedef struct tag_rx_debug_message_feedback {
	u16	reserve1;
	u8	length;				 
	u8	element_id;			 
} cmpk_rx_dbginfo_t;
typedef struct tag_tx_rate_history {
	u8	element_id;			 
	u8	length;				 
	u16	reserved1;
	u16	cck[4];
	u16	ofdm[8];
	u16	ht_mcs[4][16];
} __packed cmpk_tx_rahis_t;
typedef enum tag_command_packet_directories {
	RX_TX_FEEDBACK			= 0,
	RX_INTERRUPT_STATUS		= 1,
	TX_SET_CONFIG			= 2,
	BOTH_QUERY_CONFIG		= 3,
	RX_TX_STATUS			= 4,
	RX_DBGINFO_FEEDBACK		= 5,
	RX_TX_PER_PKT_FEEDBACK		= 6,
	RX_TX_RATE_HISTORY		= 7,
	RX_CMD_ELE_MAX
} cmpk_element_e;
typedef enum _rt_status {
	RT_STATUS_SUCCESS,
	RT_STATUS_FAILURE,
	RT_STATUS_PENDING,
	RT_STATUS_RESOURCE
} rt_status, *prt_status;
u32 cmpk_message_handle_rx(struct net_device *dev,
			   struct ieee80211_rx_stats *pstats);
rt_status SendTxCommandPacket(struct net_device *dev,
			      void *pData, u32 DataLen);
#endif
