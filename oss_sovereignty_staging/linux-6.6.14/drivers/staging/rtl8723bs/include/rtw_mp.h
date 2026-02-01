 
 
#ifndef _RTW_MP_H_
#define _RTW_MP_H_

#define MAX_MP_XMITBUF_SZ	2048
#define NR_MP_XMITFRAME		8

struct mp_xmit_frame {
	struct list_head	list;

	struct pkt_attrib attrib;

	struct sk_buff *pkt;

	int frame_tag;

	struct adapter *padapter;

	uint mem[(MAX_MP_XMITBUF_SZ >> 2)];
};

struct mp_wiparam {
	u32 bcompleted;
	u32 act_type;
	u32 io_offset;
	u32 io_value;
};

struct mp_tx {
	u8 stop;
	u32 count, sended;
	u8 payload;
	struct pkt_attrib attrib;
	 
	 
	u8 desc[TXDESC_SIZE];
	u8 *pallocated_buf;
	u8 *buf;
	u32 buf_size, write_size;
	void *PktTxThread;
};

#define MP_MAX_LINES		1000
#define MP_MAX_LINES_BYTES	256

typedef void (*MPT_WORK_ITEM_HANDLER)(void *Adapter);
struct mpt_context {
	 
	bool			bMassProdTest;

	 
	bool			bMptDrvUnload;

	struct timer_list			MPh2c_timeout_timer;
 

	bool		MptH2cRspEvent;
	bool		MptBtC2hEvent;
	bool		bMPh2c_timeout;

	 
	 
	 
 
	 
 
	 
 
	 
	bool			bMptWorkItemInProgress;
	 
	MPT_WORK_ITEM_HANDLER	CurrMptAct;

	 
	u32 		MptTestStart;
	 
	u32 		MptTestItem;
	 
	u32 		MptActType;	 
	 
	u32 		MptIoOffset;
	 
	u32 		MptIoValue;
	 
	u32 		MptRfPath;

	enum wireless_mode		MptWirelessModeToSw;	 
	u8 	MptChannelToSw;		 
	u8 	MptInitGainToSet;	 
	u32 		MptBandWidth;		 
	u32 		MptRateIndex;		 
	 
	u8 	btMpCckTxPower;
	 
	u8 	btMpOfdmTxPower;
	 
	u8 	TxPwrLevel[2];	 
	u32 		RegTxPwrLimit;
	 
	u32 		MptRCR;
	 
	bool			bMptFilterPattern;
	 
	u32 		MptRxOkCnt;
	 
	u32 		MptRxCrcErrCnt;

	bool			bCckContTx;	 
	bool			bOfdmContTx;	 
	bool			bStartContTx;	 
	 
	bool			bSingleCarrier;
	 
	bool			bCarrierSuppression;
	 
	bool			bSingleTone;

	 
	bool			bMptEnableAckCounter;
	u32 		MptAckCounter;

	 
	 
	 
	 

	u8 APK_bound[2];	 
	bool		bMptIndexEven;

	u8 backup0xc50;
	u8 backup0xc58;
	u8 backup0xc30;
	u8 backup0x52_RF_A;
	u8 backup0x52_RF_B;

	u32 		backup0x58_RF_A;
	u32 		backup0x58_RF_B;

	u8 	h2cReqNum;
	u8 	c2hBuf[32];

    u8          btInBuf[100];
	u32 		mptOutLen;
    u8          mptOutBuf[100];

};
 

 
#define EFUSE_MAP_SIZE		512

#define EFUSE_MAX_SIZE		512
 

 
enum {
	WRITE_REG = 1,
	READ_REG,
	WRITE_RF,
	READ_RF,
	MP_START,
	MP_STOP,
	MP_RATE,
	MP_CHANNEL,
	MP_BANDWIDTH,
	MP_TXPOWER,
	MP_ANT_TX,
	MP_ANT_RX,
	MP_CTX,
	MP_QUERY,
	MP_ARX,
	MP_PSD,
	MP_PWRTRK,
	MP_THER,
	MP_IOCTL,
	EFUSE_GET,
	EFUSE_SET,
	MP_RESET_STATS,
	MP_DUMP,
	MP_PHYPARA,
	MP_SetRFPathSwh,
	MP_QueryDrvStats,
	MP_SetBT,
	CTA_TEST,
	MP_DISABLE_BT_COEXIST,
	MP_PwrCtlDM,
	MP_NULL,
	MP_GET_TXPOWER_INX,
};

struct mp_priv {
	struct adapter *papdater;

	 
	u32 mode; 

	u32 prev_fw_state;

	 
	struct mp_wiparam workparam;
 

	 
	u8 TID;
	u32 tx_pktcount;
	u32 pktInterval;
	struct mp_tx tx;

	 
	u32 rx_bssidpktcount;
	u32 rx_pktcount;
	u32 rx_pktcount_filter_out;
	u32 rx_crcerrpktcount;
	u32 rx_pktloss;
	bool  rx_bindicatePkt;
	struct recv_stat rxstat;

	 
	u8 channel;
	u8 bandwidth;
	u8 prime_channel_offset;
	u8 txpoweridx;
	u8 txpoweridx_b;
	u8 rateidx;
	u32 preamble;
 
	u32 CrystalCap;
 

	u16 antenna_tx;
	u16 antenna_rx;
 

	u8 check_mp_pkt;

	u8 bSetTxPower;
 
	u8 mp_dm;
	u8 mac_filter[ETH_ALEN];
	u8 bmac_filter;

	struct wlan_network mp_network;
	NDIS_802_11_MAC_ADDRESS network_macaddr;

	u8 *pallocated_mp_xmitframe_buf;
	u8 *pmp_xmtframe_buf;
	struct __queue free_mp_xmitqueue;
	u32 free_mp_xmitframe_cnt;
	bool bSetRxBssid;
	bool bTxBufCkFail;

	struct mpt_context MptCtx;

	u8 *TXradomBuffer;
};

#define LOWER	true
#define RAISE	false

 
#define BB_REG_BASE_ADDR		0x800

#define MAX_RF_PATH_NUMS	RF_PATH_MAX

extern u8 mpdatarate[NumRates];

#define MAX_TX_PWR_INDEX_N_MODE 64	 

#define RX_PKT_BROADCAST	1
#define RX_PKT_DEST_ADDR	2
#define RX_PKT_PHY_MATCH	3

#define Mac_OFDM_OK			0x00000000
#define Mac_OFDM_Fail			0x10000000
#define Mac_OFDM_FasleAlarm	0x20000000
#define Mac_CCK_OK				0x30000000
#define Mac_CCK_Fail			0x40000000
#define Mac_CCK_FasleAlarm		0x50000000
#define Mac_HT_OK				0x60000000
#define Mac_HT_Fail			0x70000000
#define Mac_HT_FasleAlarm		0x90000000
#define Mac_DropPacket			0xA0000000

#define		REG_RF_BB_GAIN_OFFSET	0x7f
#define		RF_GAIN_OFFSET_MASK	0xfffff

 
 
 

s32 init_mp_priv(struct adapter *padapter);
void free_mp_priv(struct mp_priv *pmp_priv);
s32 MPT_InitializeAdapter(struct adapter *padapter, u8 Channel);
void MPT_DeInitAdapter(struct adapter *padapter);
s32 mp_start_test(struct adapter *padapter);
void mp_stop_test(struct adapter *padapter);

u32 _read_rfreg(struct adapter *padapter, u8 rfpath, u32 addr, u32 bitmask);
void _write_rfreg(struct adapter *padapter, u8 rfpath, u32 addr, u32 bitmask, u32 val);

u32 read_macreg(struct adapter *padapter, u32 addr, u32 sz);
void write_macreg(struct adapter *padapter, u32 addr, u32 val, u32 sz);
u32 read_bbreg(struct adapter *padapter, u32 addr, u32 bitmask);
void write_bbreg(struct adapter *padapter, u32 addr, u32 bitmask, u32 val);
u32 read_rfreg(struct adapter *padapter, u8 rfpath, u32 addr);
void write_rfreg(struct adapter *padapter, u8 rfpath, u32 addr, u32 val);

void SetChannel(struct adapter *padapter);
void SetBandwidth(struct adapter *padapter);
int SetTxPower(struct adapter *padapter);
void SetAntennaPathPower(struct adapter *padapter);
void SetDataRate(struct adapter *padapter);

void SetAntenna(struct adapter *padapter);

s32 SetThermalMeter(struct adapter *padapter, u8 target_ther);
void GetThermalMeter(struct adapter *padapter, u8 *value);

void SetContinuousTx(struct adapter *padapter, u8 bStart);
void SetSingleCarrierTx(struct adapter *padapter, u8 bStart);
void SetSingleToneTx(struct adapter *padapter, u8 bStart);
void SetCarrierSuppressionTx(struct adapter *padapter, u8 bStart);
void PhySetTxPowerLevel(struct adapter *padapter);

void fill_txdesc_for_mp(struct adapter *padapter, u8 *ptxdesc);
void SetPacketTx(struct adapter *padapter);
void SetPacketRx(struct adapter *padapter, u8 bStartRx);

void ResetPhyRxPktCount(struct adapter *padapter);
u32 GetPhyRxPktReceived(struct adapter *padapter);
u32 GetPhyRxPktCRC32Error(struct adapter *padapter);

s32	SetPowerTracking(struct adapter *padapter, u8 enable);
void GetPowerTracking(struct adapter *padapter, u8 *enable);

u32 mp_query_psd(struct adapter *padapter, u8 *data);

void Hal_SetAntenna(struct adapter *padapter);
void Hal_SetBandwidth(struct adapter *padapter);

void Hal_SetTxPower(struct adapter *padapter);
void Hal_SetCarrierSuppressionTx(struct adapter *padapter, u8 bStart);
void Hal_SetSingleToneTx(struct adapter *padapter, u8 bStart);
void Hal_SetSingleCarrierTx(struct adapter *padapter, u8 bStart);
void Hal_SetContinuousTx(struct adapter *padapter, u8 bStart);

void Hal_SetDataRate(struct adapter *padapter);
void Hal_SetChannel(struct adapter *padapter);
void Hal_SetAntennaPathPower(struct adapter *padapter);
s32 Hal_SetThermalMeter(struct adapter *padapter, u8 target_ther);
s32 Hal_SetPowerTracking(struct adapter *padapter, u8 enable);
void Hal_GetPowerTracking(struct adapter *padapter, u8 *enable);
void Hal_GetThermalMeter(struct adapter *padapter, u8 *value);
void Hal_mpt_SwitchRfSetting(struct adapter *padapter);
void Hal_MPT_CCKTxPowerAdjust(struct adapter *Adapter, bool bInCH14);
void Hal_MPT_CCKTxPowerAdjustbyIndex(struct adapter *padapter, bool beven);
void Hal_SetCCKTxPower(struct adapter *padapter, u8 *TxPower);
void Hal_SetOFDMTxPower(struct adapter *padapter, u8 *TxPower);
void Hal_TriggerRFThermalMeter(struct adapter *padapter);
u8 Hal_ReadRFThermalMeter(struct adapter *padapter);
void Hal_SetCCKContinuousTx(struct adapter *padapter, u8 bStart);
void Hal_SetOFDMContinuousTx(struct adapter *padapter, u8 bStart);
void Hal_ProSetCrystalCap(struct adapter *padapter, u32 CrystalCapVal);
void MP_PHY_SetRFPathSwitch(struct adapter *padapter, bool bMain);
u32 mpt_ProQueryCalTxPower(struct adapter *padapter, u8 RfPath);
void MPT_PwrCtlDM(struct adapter *padapter, u32 bstart);
u8 MptToMgntRate(u32 MptRateIdx);

#endif  
