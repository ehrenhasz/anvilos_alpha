#ifndef __RTW_CMD_H_
#define __RTW_CMD_H_
#include <linux/completion.h>
#define C2H_MEM_SZ (16*1024)
	#define FREE_CMDOBJ_SZ	128
	#define MAX_CMDSZ	1024
	#define MAX_RSPSZ	512
	#define MAX_EVTSZ	1024
	#define CMDBUFF_ALIGN_SZ 512
	struct cmd_obj {
		struct adapter *padapter;
		u16 cmdcode;
		u8 res;
		u8 *parmbuf;
		u32 cmdsz;
		u8 *rsp;
		u32 rspsz;
		struct submit_ctx *sctx;
		struct list_head	list;
	};
	enum {
		RTW_CMDF_DIRECTLY = BIT0,
		RTW_CMDF_WAIT_ACK = BIT1,
	};
	struct cmd_priv {
		struct completion cmd_queue_comp;
		struct completion terminate_cmdthread_comp;
		struct __queue	cmd_queue;
		u8 cmd_seq;
		u8 *cmd_buf;	 
		u8 *cmd_allocated_buf;
		u8 *rsp_buf;	 
		u8 *rsp_allocated_buf;
		u32 cmd_issued_cnt;
		u32 cmd_done_cnt;
		u32 rsp_cnt;
		atomic_t cmdthd_running;
		u8 stop_req;
		struct adapter *padapter;
		struct mutex sctx_mutex;
	};
	struct	evt_priv {
		struct work_struct c2h_wk;
		bool c2h_wk_alive;
		struct rtw_cbuf *c2h_queue;
		#define C2H_QUEUE_MAX_LEN 10
		atomic_t event_seq;
		u8 *evt_buf;	 
		u8 *evt_allocated_buf;
		u32 evt_done_cnt;
		u8 *c2h_mem;
		u8 *allocated_c2h_mem;
	};
#define init_h2fwcmd_w_parm_no_rsp(pcmd, pparm, code) \
do {\
	INIT_LIST_HEAD(&pcmd->list);\
	pcmd->cmdcode = code;\
	pcmd->parmbuf = (u8 *)(pparm);\
	pcmd->cmdsz = sizeof(*pparm);\
	pcmd->rsp = NULL;\
	pcmd->rspsz = 0;\
} while (0)
#define init_h2fwcmd_w_parm_no_parm_rsp(pcmd, code) \
do {\
	INIT_LIST_HEAD(&pcmd->list);\
	pcmd->cmdcode = code;\
	pcmd->parmbuf = NULL;\
	pcmd->cmdsz = 0;\
	pcmd->rsp = NULL;\
	pcmd->rspsz = 0;\
} while (0)
struct c2h_evt_hdr {
	u8 id:4;
	u8 plen:4;
	u8 seq;
	u8 payload[];
};
struct c2h_evt_hdr_88xx {
	u8 id;
	u8 seq;
	u8 payload[12];
	u8 plen;
	u8 trigger;
};
#define c2h_evt_valid(c2h_evt) ((c2h_evt)->id || (c2h_evt)->plen)
int rtw_enqueue_cmd(struct cmd_priv *pcmdpriv, struct cmd_obj *obj);
extern struct cmd_obj *rtw_dequeue_cmd(struct cmd_priv *pcmdpriv);
extern void rtw_free_cmd_obj(struct cmd_obj *pcmd);
void rtw_stop_cmd_thread(struct adapter *adapter);
int rtw_cmd_thread(void *context);
extern void rtw_free_cmd_priv(struct cmd_priv *pcmdpriv);
extern void rtw_free_evt_priv(struct evt_priv *pevtpriv);
extern void rtw_evt_notify_isr(struct evt_priv *pevtpriv);
enum {
	NONE_WK_CID,
	DYNAMIC_CHK_WK_CID,
	DM_CTRL_WK_CID,
	PBC_POLLING_WK_CID,
	POWER_SAVING_CTRL_WK_CID, 
	LPS_CTRL_WK_CID,
	ANT_SELECT_WK_CID,
	P2P_PS_WK_CID,
	P2P_PROTO_WK_CID,
	CHECK_HIQ_WK_CID, 
	INTEl_WIDI_WK_CID,
	C2H_WK_CID,
	RTP_TIMER_CFG_WK_CID,
	RESET_SECURITYPRIV,  
	FREE_ASSOC_RESOURCES,  
	DM_IN_LPS_WK_CID,
	DM_RA_MSK_WK_CID,  
	BEAMFORMING_WK_CID,
	LPS_CHANGE_DTIM_CID,
	BTINFO_WK_CID,
	MAX_WK_CID
};
enum {
	LPS_CTRL_SCAN = 0,
	LPS_CTRL_JOINBSS = 1,
	LPS_CTRL_CONNECT = 2,
	LPS_CTRL_DISCONNECT = 3,
	LPS_CTRL_SPECIAL_PACKET = 4,
	LPS_CTRL_LEAVE = 5,
	LPS_CTRL_TRAFFIC_BUSY = 6,
};
enum {
	SWSI,
	HWSI,
	HWPI,
};
struct joinbss_parm {
	struct wlan_bssid_ex network;
};
struct disconnect_parm {
	u32 deauth_timeout_ms;
};
struct createbss_parm {
	struct wlan_bssid_ex network;
};
struct	setopmode_parm {
	u8 mode;
	u8 rsvd[3];
};
#define RTW_SSID_SCAN_AMOUNT 9  
#define RTW_CHANNEL_SCAN_AMOUNT (14+37)
struct sitesurvey_parm {
	signed int scan_mode;	 
	u8 ssid_num;
	u8 ch_num;
	struct ndis_802_11_ssid ssid[RTW_SSID_SCAN_AMOUNT];
	struct rtw_ieee80211_channel ch[RTW_CHANNEL_SCAN_AMOUNT];
};
struct setauth_parm {
	u8 mode;   
	u8 _1x;    
	u8 rsvd[2];
};
struct setkey_parm {
	u8 algorithm;	 
	u8 keyid;
	u8 grpkey;		 
	u8 set_tx;		 
	u8 key[16];	 
};
struct set_stakey_parm {
	u8 addr[ETH_ALEN];
	u8 algorithm;
	u8 keyid;
	u8 key[16];
};
struct set_stakey_rsp {
	u8 addr[ETH_ALEN];
	u8 keyid;
	u8 rsvd;
};
struct set_assocsta_parm {
	u8 addr[ETH_ALEN];
};
struct set_assocsta_rsp {
	u8 cam_id;
	u8 rsvd[3];
};
struct del_assocsta_parm {
	u8 addr[ETH_ALEN];
};
struct setstapwrstate_parm {
	u8 staid;
	u8 status;
	u8 hwaddr[6];
};
struct	setbasicrate_parm {
	u8 basicrates[NumRates];
};
struct getbasicrate_parm {
	u32 rsvd;
};
struct setdatarate_parm {
	u8 mac_id;
	u8 datarates[NumRates];
};
struct getdatarate_parm {
	u32 rsvd;
};
struct	setphyinfo_parm {
	struct regulatory_class class_sets[NUM_REGULATORYS];
	u8 status;
};
struct	getphyinfo_parm {
	u32 rsvd;
};
struct	setphy_parm {
	u8 rfchannel;
	u8 modem;
};
struct	getphy_parm {
	u32 rsvd;
};
struct Tx_Beacon_param {
	struct wlan_bssid_ex network;
};
struct drvextra_cmd_parm {
	int ec_id;  
	int type;  
	int size;  
	unsigned char *pbuf;
};
struct	getcountjudge_rsp {
	u8 count_judge[MAX_RATES_LENGTH];
};
struct addBaReq_parm {
	unsigned int tid;
	u8 addr[ETH_ALEN];
};
struct set_ch_parm {
	u8 ch;
	u8 bw;
	u8 ch_offset;
};
struct SetChannelPlan_param {
	u8 channel_plan;
};
struct SetChannelSwitch_param {
	u8 new_ch_no;
};
struct TDLSoption_param {
	u8 addr[ETH_ALEN];
	u8 option;
};
struct RunInThread_param {
	void (*func)(void *);
	void *context;
};
#define GEN_CMD_CODE(cmd)	cmd ## _CMD_
#define H2C_RSP_OFFSET			512
#define H2C_SUCCESS			0x00
#define H2C_SUCCESS_RSP			0x01
#define H2C_DUPLICATED			0x02
#define H2C_DROPPED			0x03
#define H2C_PARAMETERS_ERROR		0x04
#define H2C_REJECTED			0x05
#define H2C_CMD_OVERFLOW		0x06
#define H2C_RESERVED			0x07
u8 rtw_sitesurvey_cmd(struct adapter  *padapter, struct ndis_802_11_ssid *ssid, int ssid_num, struct rtw_ieee80211_channel *ch, int ch_num);
extern u8 rtw_createbss_cmd(struct adapter  *padapter);
int rtw_startbss_cmd(struct adapter  *padapter, int flags);
struct sta_info;
extern u8 rtw_setstakey_cmd(struct adapter  *padapter, struct sta_info *sta, u8 unicast_key, bool enqueue);
extern u8 rtw_clearstakey_cmd(struct adapter *padapter, struct sta_info *sta, u8 enqueue);
extern u8 rtw_joinbss_cmd(struct adapter *padapter, struct wlan_network *pnetwork);
u8 rtw_disassoc_cmd(struct adapter *padapter, u32 deauth_timeout_ms, bool enqueue);
extern u8 rtw_setopmode_cmd(struct adapter  *padapter, enum ndis_802_11_network_infrastructure networktype, bool enqueue);
extern u8 rtw_setrfintfs_cmd(struct adapter  *padapter, u8 mode);
extern u8 rtw_gettssi_cmd(struct adapter  *padapter, u8 offset, u8 *pval);
extern u8 rtw_setfwdig_cmd(struct adapter *padapter, u8 type);
extern u8 rtw_setfwra_cmd(struct adapter *padapter, u8 type);
extern u8 rtw_addbareq_cmd(struct adapter *padapter, u8 tid, u8 *addr);
extern u8 rtw_reset_securitypriv_cmd(struct adapter *padapter);
extern u8 rtw_free_assoc_resources_cmd(struct adapter *padapter);
extern u8 rtw_dynamic_chk_wk_cmd(struct adapter *adapter);
u8 rtw_lps_ctrl_wk_cmd(struct adapter *padapter, u8 lps_ctrl_type, u8 enqueue);
u8 rtw_dm_in_lps_wk_cmd(struct adapter *padapter);
u8 rtw_dm_ra_mask_wk_cmd(struct adapter *padapter, u8 *psta);
extern u8 rtw_ps_cmd(struct adapter *padapter);
u8 rtw_chk_hi_queue_cmd(struct adapter *padapter);
extern u8 rtw_c2h_packet_wk_cmd(struct adapter *padapter, u8 *pbuf, u16 length);
extern u8 rtw_c2h_wk_cmd(struct adapter *padapter, u8 *c2h_evt);
u8 rtw_drvextra_cmd_hdl(struct adapter *padapter, unsigned char *pbuf);
extern void rtw_survey_cmd_callback(struct adapter  *padapter, struct cmd_obj *pcmd);
extern void rtw_disassoc_cmd_callback(struct adapter  *padapter, struct cmd_obj *pcmd);
extern void rtw_joinbss_cmd_callback(struct adapter  *padapter, struct cmd_obj *pcmd);
extern void rtw_createbss_cmd_callback(struct adapter  *padapter, struct cmd_obj *pcmd);
extern void rtw_getbbrfreg_cmdrsp_callback(struct adapter  *padapter, struct cmd_obj *pcmd);
extern void rtw_setstaKey_cmdrsp_callback(struct adapter  *padapter,  struct cmd_obj *pcmd);
extern void rtw_setassocsta_cmdrsp_callback(struct adapter  *padapter,  struct cmd_obj *pcmd);
extern void rtw_getrttbl_cmdrsp_callback(struct adapter  *padapter,  struct cmd_obj *pcmd);
struct _cmd_callback {
	u32 cmd_code;
	void (*callback)(struct adapter  *padapter, struct cmd_obj *cmd);
};
enum {
	GEN_CMD_CODE(_Read_MACREG),	 
	GEN_CMD_CODE(_Write_MACREG),
	GEN_CMD_CODE(_Read_BBREG),
	GEN_CMD_CODE(_Write_BBREG),
	GEN_CMD_CODE(_Read_RFREG),
	GEN_CMD_CODE(_Write_RFREG),  
	GEN_CMD_CODE(_Read_EEPROM),
	GEN_CMD_CODE(_Write_EEPROM),
	GEN_CMD_CODE(_Read_EFUSE),
	GEN_CMD_CODE(_Write_EFUSE),
	GEN_CMD_CODE(_Read_CAM),	 
	GEN_CMD_CODE(_Write_CAM),
	GEN_CMD_CODE(_setBCNITV),
	GEN_CMD_CODE(_setMBIDCFG),
	GEN_CMD_CODE(_JoinBss),    
	GEN_CMD_CODE(_DisConnect),  
	GEN_CMD_CODE(_CreateBss),
	GEN_CMD_CODE(_SetOpMode),
	GEN_CMD_CODE(_SiteSurvey),   
	GEN_CMD_CODE(_SetAuth),
	GEN_CMD_CODE(_SetKey),	 
	GEN_CMD_CODE(_SetStaKey),
	GEN_CMD_CODE(_SetAssocSta),
	GEN_CMD_CODE(_DelAssocSta),
	GEN_CMD_CODE(_SetStaPwrState),
	GEN_CMD_CODE(_SetBasicRate),  
	GEN_CMD_CODE(_GetBasicRate),
	GEN_CMD_CODE(_SetDataRate),
	GEN_CMD_CODE(_GetDataRate),
	GEN_CMD_CODE(_SetPhyInfo),
	GEN_CMD_CODE(_GetPhyInfo),	 
	GEN_CMD_CODE(_SetPhy),
	GEN_CMD_CODE(_GetPhy),
	GEN_CMD_CODE(_readRssi),
	GEN_CMD_CODE(_readGain),
	GEN_CMD_CODE(_SetAtim),  
	GEN_CMD_CODE(_SetPwrMode),
	GEN_CMD_CODE(_JoinbssRpt),
	GEN_CMD_CODE(_SetRaTable),
	GEN_CMD_CODE(_GetRaTable),
	GEN_CMD_CODE(_GetCCXReport),  
	GEN_CMD_CODE(_GetDTMReport),
	GEN_CMD_CODE(_GetTXRateStatistics),
	GEN_CMD_CODE(_SetUsbSuspend),
	GEN_CMD_CODE(_SetH2cLbk),
	GEN_CMD_CODE(_AddBAReq),  
	GEN_CMD_CODE(_SetChannel),  
	GEN_CMD_CODE(_SetTxPower),
	GEN_CMD_CODE(_SwitchAntenna),
	GEN_CMD_CODE(_SetCrystalCap),
	GEN_CMD_CODE(_SetSingleCarrierTx),  
	GEN_CMD_CODE(_SetSingleToneTx), 
	GEN_CMD_CODE(_SetCarrierSuppressionTx),
	GEN_CMD_CODE(_SetContinuousTx),
	GEN_CMD_CODE(_SwitchBandwidth),  
	GEN_CMD_CODE(_TX_Beacon),  
	GEN_CMD_CODE(_Set_MLME_EVT),  
	GEN_CMD_CODE(_Set_Drv_Extra),  
	GEN_CMD_CODE(_Set_H2C_MSG),  
	GEN_CMD_CODE(_SetChannelPlan),  
	GEN_CMD_CODE(_SetChannelSwitch),  
	GEN_CMD_CODE(_TDLS),  
	GEN_CMD_CODE(_ChkBMCSleepq),  
	GEN_CMD_CODE(_RunInThreadCMD),  
	MAX_H2CCMD
};
#define _GetBBReg_CMD_		_Read_BBREG_CMD_
#define _SetBBReg_CMD_		_Write_BBREG_CMD_
#define _GetRFReg_CMD_		_Read_RFREG_CMD_
#define _SetRFReg_CMD_		_Write_RFREG_CMD_
#endif  
