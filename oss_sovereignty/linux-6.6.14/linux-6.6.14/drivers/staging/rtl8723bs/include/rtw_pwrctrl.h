#ifndef __RTW_PWRCTRL_H_
#define __RTW_PWRCTRL_H_
#include <linux/mutex.h>
#define FW_PWR0	0
#define FW_PWR1		1
#define FW_PWR2		2
#define FW_PWR3		3
#define HW_PWR0	7
#define HW_PWR1		6
#define HW_PWR2		2
#define HW_PWR3	0
#define HW_PWR4	8
#define FW_PWRMSK	0x7
#define XMIT_ALIVE	BIT(0)
#define RECV_ALIVE	BIT(1)
#define CMD_ALIVE	BIT(2)
#define EVT_ALIVE	BIT(3)
#define BTCOEX_ALIVE	BIT(4)
enum {
	PS_MODE_ACTIVE	= 0,
	PS_MODE_MIN,
	PS_MODE_MAX,
	PS_MODE_DTIM,	 
	PS_MODE_VOIP,
	PS_MODE_UAPSD_WMM,
	PS_MODE_UAPSD,
	PS_MODE_IBSS,
	PS_MODE_WWLAN,
	PM_Radio_Off,
	PM_Card_Disable,
	PS_MODE_NUM,
};
#define PS_DPS				BIT(0)
#define PS_LCLK				(PS_DPS)
#define PS_RF_OFF			BIT(1)
#define PS_ALL_ON			BIT(2)
#define PS_ST_ACTIVE		BIT(3)
#define PS_ISR_ENABLE		BIT(4)
#define PS_IMR_ENABLE		BIT(5)
#define PS_ACK				BIT(6)
#define PS_TOGGLE			BIT(7)
#define PS_STATE_MASK		(0x0F)
#define PS_STATE_HW_MASK	(0x07)
#define PS_SEQ_MASK			(0xc0)
#define PS_STATE(x)		(PS_STATE_MASK & (x))
#define PS_STATE_HW(x)	(PS_STATE_HW_MASK & (x))
#define PS_SEQ(x)		(PS_SEQ_MASK & (x))
#define PS_STATE_S0		(PS_DPS)
#define PS_STATE_S1		(PS_LCLK)
#define PS_STATE_S2		(PS_RF_OFF)
#define PS_STATE_S3		(PS_ALL_ON)
#define PS_STATE_S4		((PS_ST_ACTIVE) | (PS_ALL_ON))
#define PS_IS_RF_ON(x)	((x) & (PS_ALL_ON))
#define PS_IS_ACTIVE(x)	((x) & (PS_ST_ACTIVE))
#define CLR_PS_STATE(x)	((x) = ((x) & (0xF0)))
struct reportpwrstate_parm {
	unsigned char mode;
	unsigned char state;  
	unsigned short rsvd;
};
#define LPS_DELAY_TIME	(1 * HZ)  
#define EXE_PWR_NONE	0x01
#define EXE_PWR_IPS		0x02
#define EXE_PWR_LPS		0x04
enum rt_rf_power_state {
	rf_on,		 
	rf_sleep,	 
	rf_off,		 
	rf_max
};
#define	RT_RF_OFF_LEVL_ASPM			BIT(0)	 
#define	RT_RF_OFF_LEVL_CLK_REQ		BIT(1)	 
#define	RT_RF_OFF_LEVL_PCI_D3			BIT(2)	 
#define	RT_RF_OFF_LEVL_HALT_NIC		BIT(3)	 
#define	RT_RF_OFF_LEVL_FREE_FW		BIT(4)	 
#define	RT_RF_OFF_LEVL_FW_32K		BIT(5)	 
#define	RT_RF_PS_LEVEL_ALWAYS_ASPM	BIT(6)	 
#define	RT_RF_LPS_DISALBE_2R			BIT(30)	 
#define	RT_RF_LPS_LEVEL_ASPM			BIT(31)	 
#define	RT_IN_PS_LEVEL(ppsc, _PS_FLAG)		((ppsc->cur_ps_level & _PS_FLAG) ? true : false)
#define	RT_CLEAR_PS_LEVEL(ppsc, _PS_FLAG)	(ppsc->cur_ps_level &= (~(_PS_FLAG)))
#define	RT_SET_PS_LEVEL(ppsc, _PS_FLAG)		(ppsc->cur_ps_level |= _PS_FLAG)
#define	RT_PCI_ASPM_OSC_IGNORE		0	  
#define	RT_PCI_ASPM_OSC_ENABLE		BIT0  
#define	RT_PCI_ASPM_OSC_DISABLE		BIT1  
enum {
	PSBBREG_RF0 = 0,
	PSBBREG_RF1,
	PSBBREG_RF2,
	PSBBREG_AFE0,
	PSBBREG_TOTALCNT
};
enum {  
	IPS_NONE = 0,
	IPS_NORMAL,
	IPS_LEVEL_2,
	IPS_NUM
};
enum ps_deny_reason {
	PS_DENY_DRV_INITIAL = 0,
	PS_DENY_SCAN,
	PS_DENY_JOIN,
	PS_DENY_DISCONNECT,
	PS_DENY_SUSPEND,
	PS_DENY_IOCTL,
	PS_DENY_MGNT_TX,
	PS_DENY_DRV_REMOVE = 30,
	PS_DENY_OTHERS = 31
};
struct pwrctrl_priv {
	struct mutex lock;
	volatile u8 rpwm;  
	volatile u8 cpwm;  
	volatile u8 tog;  
	volatile u8 cpwm_tog;  
	u8 pwr_mode;
	u8 smart_ps;
	u8 bcn_ant_mode;
	u8 dtim;
	u32 alives;
	struct work_struct cpwm_event;
	u8 brpwmtimeout;
	struct work_struct rpwmtimeoutwi;
	struct timer_list pwr_rpwm_timer;
	u8 bpower_saving;  
	u8 b_hw_radio_off;
	u8 reg_rfoff;
	u8 reg_pdnmode;  
	u32 rfoff_reason;
	u32 cur_ps_level;
	u32 reg_rfps_level;
	uint	ips_enter_cnts;
	uint	ips_leave_cnts;
	u8 ips_mode;
	u8 ips_org_mode;
	u8 ips_mode_req;  
	bool bips_processing;
	unsigned long ips_deny_time;  
	u8 pre_ips_type; 
	u32 ps_deny;
	u8 ps_processing;  
	u8 fw_psmode_iface_id;
	u8 bLeisurePs;
	u8 LpsIdleCount;
	u8 power_mgnt;
	u8 org_power_mgnt;
	bool fw_current_in_ps_mode;
	unsigned long	DelayLPSLastTimeStamp;
	s32		pnp_current_pwr_state;
	u8 pnp_bstop_trx;
	u8 bInternalAutoSuspend;
	u8 bInSuspend;
	u8 bAutoResume;
	u8 autopm_cnt;
	u8 bSupportRemoteWakeup;
	u8 wowlan_wake_reason;
	u8 wowlan_ap_mode;
	u8 wowlan_mode;
	struct timer_list	pwr_state_check_timer;
	struct adapter *adapter;
	int		pwr_state_check_interval;
	u8 pwr_state_check_cnts;
	int		ps_flag;  
	enum rt_rf_power_state	rf_pwrstate; 
	enum rt_rf_power_state	change_rfpwrstate;
	u8 bHWPowerdown;  
	u8 bHWPwrPindetect;  
	u8 bkeepfwalive;
	u8 brfoffbyhw;
	unsigned long PS_BBRegBackup[PSBBREG_TOTALCNT];
};
#define rtw_ips_mode_req(pwrctl, ips_mode) \
	((pwrctl)->ips_mode_req = (ips_mode))
#define RTW_PWR_STATE_CHK_INTERVAL 2000
#define _rtw_set_pwr_state_check_timer(pwrctl, ms) \
	do { \
		_set_timer(&(pwrctl)->pwr_state_check_timer, (ms)); \
	} while (0)
#define rtw_set_pwr_state_check_timer(pwrctl) \
	_rtw_set_pwr_state_check_timer((pwrctl), (pwrctl)->pwr_state_check_interval)
extern void rtw_init_pwrctrl_priv(struct adapter *adapter);
extern void rtw_free_pwrctrl_priv(struct adapter *adapter);
s32 rtw_register_task_alive(struct adapter *, u32 task);
void rtw_unregister_task_alive(struct adapter *, u32 task);
extern s32 rtw_register_tx_alive(struct adapter *padapter);
extern void rtw_unregister_tx_alive(struct adapter *padapter);
extern s32 rtw_register_cmd_alive(struct adapter *padapter);
extern void rtw_unregister_cmd_alive(struct adapter *padapter);
extern void cpwm_int_hdl(struct adapter *padapter, struct reportpwrstate_parm *preportpwrstate);
extern void LPS_Leave_check(struct adapter *padapter);
extern void LeaveAllPowerSaveMode(struct adapter *Adapter);
extern void LeaveAllPowerSaveModeDirect(struct adapter *Adapter);
void _ips_enter(struct adapter *padapter);
void ips_enter(struct adapter *padapter);
int _ips_leave(struct adapter *padapter);
int ips_leave(struct adapter *padapter);
void rtw_ps_processor(struct adapter *padapter);
s32 LPS_RF_ON_check(struct adapter *padapter, u32 delay_ms);
void LPS_Enter(struct adapter *padapter, const char *msg);
void LPS_Leave(struct adapter *padapter, const char *msg);
void traffic_check_for_leave_lps(struct adapter *padapter, u8 tx, u32 tx_packets);
void rtw_set_ps_mode(struct adapter *padapter, u8 ps_mode, u8 smart_ps, u8 bcn_ant_mode, const char *msg);
void rtw_set_rpwm(struct adapter *padapter, u8 val8);
void rtw_set_ips_deny(struct adapter *padapter, u32 ms);
int _rtw_pwr_wakeup(struct adapter *padapter, u32 ips_deffer_ms, const char *caller);
#define rtw_pwr_wakeup(adapter) _rtw_pwr_wakeup(adapter, RTW_PWR_STATE_CHK_INTERVAL, __func__)
#define rtw_pwr_wakeup_ex(adapter, ips_deffer_ms) _rtw_pwr_wakeup(adapter, ips_deffer_ms, __func__)
int rtw_pm_set_ips(struct adapter *padapter, u8 mode);
int rtw_pm_set_lps(struct adapter *padapter, u8 mode);
void rtw_ps_deny(struct adapter *padapter, enum ps_deny_reason reason);
void rtw_ps_deny_cancel(struct adapter *padapter, enum ps_deny_reason reason);
u32 rtw_ps_deny_get(struct adapter *padapter);
#endif   
