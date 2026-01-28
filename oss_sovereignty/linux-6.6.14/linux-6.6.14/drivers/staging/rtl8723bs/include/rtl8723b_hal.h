#ifndef __RTL8723B_HAL_H__
#define __RTL8723B_HAL_H__
#include "hal_data.h"
#include "rtl8723b_spec.h"
#include "rtl8723b_rf.h"
#include "rtl8723b_dm.h"
#include "rtl8723b_recv.h"
#include "rtl8723b_xmit.h"
#include "rtl8723b_cmd.h"
#include "rtw_mp.h"
#include "hal_pwr_seq.h"
#include "hal_phy_reg_8723b.h"
#include "hal_phy_cfg.h"
#define FW_8723B_SIZE          0x8000
#define FW_8723B_START_ADDRESS 0x1000
#define FW_8723B_END_ADDRESS   0x1FFF  
#define IS_FW_HEADER_EXIST_8723B(fw_hdr) \
	((le16_to_cpu(fw_hdr->signature) & 0xFFF0) == 0x5300)
struct rt_firmware {
	u32 fw_length;
	u8 *fw_buffer_sz;
};
struct rt_firmware_hdr {
	__le16 signature;   
	u8 category;	    
	u8 function;	    
	__le16 version;     
	__le16 subversion;  
	u8 month;   
	u8 date;    
	u8 hour;    
	u8 minute;  
	__le16 ram_code_size;  
	__le16 rsvd2;
	__le32 svn_idx;	 
	__le32 rsvd3;
	__le32 rsvd4;
	__le32 rsvd5;
};
#define DRIVER_EARLY_INT_TIME_8723B  0x05
#define BCN_DMA_ATIME_INT_TIME_8723B 0x02
#define PAGE_SIZE_TX_8723B 128
#define PAGE_SIZE_RX_8723B 8
#define RX_DMA_SIZE_8723B          0x4000  
#define RX_DMA_RESERVED_SIZE_8723B 0x80    
#define RX_DMA_BOUNDARY_8723B \
	(RX_DMA_SIZE_8723B - RX_DMA_RESERVED_SIZE_8723B - 1)
#define BCNQ_PAGE_NUM_8723B  0x08
#define BCNQ1_PAGE_NUM_8723B 0x00
#define MAX_RX_DMA_BUFFER_SIZE_8723B 0x2800  
#define WOWLAN_PAGE_NUM_8723B 0x00
#define TX_TOTAL_PAGE_NUMBER_8723B     \
	(0xFF - BCNQ_PAGE_NUM_8723B  - \
		BCNQ1_PAGE_NUM_8723B - \
		WOWLAN_PAGE_NUM_8723B)
#define TX_PAGE_BOUNDARY_8723B (TX_TOTAL_PAGE_NUMBER_8723B + 1)
#define WMM_NORMAL_TX_TOTAL_PAGE_NUMBER_8723B TX_TOTAL_PAGE_NUMBER_8723B
#define WMM_NORMAL_TX_PAGE_BOUNDARY_8723B \
	(WMM_NORMAL_TX_TOTAL_PAGE_NUMBER_8723B + 1)
#define NORMAL_PAGE_NUM_HPQ_8723B 0x0C
#define NORMAL_PAGE_NUM_LPQ_8723B 0x02
#define NORMAL_PAGE_NUM_NPQ_8723B 0x02
#define WMM_NORMAL_PAGE_NUM_HPQ_8723B 0x30
#define WMM_NORMAL_PAGE_NUM_LPQ_8723B 0x20
#define WMM_NORMAL_PAGE_NUM_NPQ_8723B 0x20
#include "HalVerDef.h"
#include "hal_com.h"
#define EFUSE_OOB_PROTECT_BYTES 15
#define HAL_EFUSE_MEMORY
#define HWSET_MAX_SIZE_8723B         512
#define EFUSE_REAL_CONTENT_LEN_8723B 512
#define EFUSE_MAP_LEN_8723B          512
#define EFUSE_MAX_SECTION_8723B      64
#define EFUSE_IC_ID_OFFSET 506  
#define AVAILABLE_EFUSE_ADDR(addr) (addr < EFUSE_REAL_CONTENT_LEN_8723B)
#define EFUSE_ACCESS_ON  0x69  
#define EFUSE_ACCESS_OFF 0x00  
#define EFUSE_BT_REAL_BANK_CONTENT_LEN 512
#define EFUSE_BT_REAL_CONTENT_LEN      1536  
#define EFUSE_BT_MAP_LEN               1024  
#define EFUSE_BT_MAX_SECTION           128   
#define EFUSE_PROTECT_BYTES_BANK 16
enum {
	C2H_DBG = 0,
	C2H_TSF = 1,
	C2H_AP_RPT_RSP = 2,
	C2H_CCX_TX_RPT = 3,  
	C2H_BT_RSSI = 4,
	C2H_BT_OP_MODE = 5,
	C2H_EXT_RA_RPT = 6,
	C2H_8723B_BT_INFO = 9,
	C2H_HW_INFO_EXCH = 10,
	C2H_8723B_BT_MP_INFO = 11,
	MAX_C2HEVENT
};
struct c2h_evt_hdr_t {
	u8 CmdID;
	u8 CmdLen;
	u8 CmdSeq;
} __attribute__((__packed__));
enum {  
	PACKAGE_DEFAULT,
	PACKAGE_QFN68,
	PACKAGE_TFBGA90,
	PACKAGE_TFBGA80,
	PACKAGE_TFBGA79
};
#define INCLUDE_MULTI_FUNC_BT(_Adapter)  \
	(GET_HAL_DATA(_Adapter)->MultiFunc & RT_MULTI_FUNC_BT)
#define INCLUDE_MULTI_FUNC_GPS(_Adapter) \
	(GET_HAL_DATA(_Adapter)->MultiFunc & RT_MULTI_FUNC_GPS)
s32 rtl8723b_FirmwareDownload(struct adapter *padapter, bool  bUsedWoWLANFw);
void rtl8723b_FirmwareSelfReset(struct adapter *padapter);
void rtl8723b_InitializeFirmwareVars(struct adapter *padapter);
void rtl8723b_InitAntenna_Selection(struct adapter *padapter);
void rtl8723b_init_default_value(struct adapter *padapter);
s32 rtl8723b_InitLLTTable(struct adapter *padapter);
u8 GetEEPROMSize8723B(struct adapter *padapter);
void Hal_InitPGData(struct adapter *padapter, u8 *PROMContent);
void Hal_EfuseParseIDCode(struct adapter *padapter, u8 *hwinfo);
void Hal_EfuseParseTxPowerInfo_8723B(struct adapter *padapter, u8 *PROMContent,
				     bool AutoLoadFail);
void Hal_EfuseParseBTCoexistInfo_8723B(struct adapter *padapter, u8 *hwinfo,
				       bool AutoLoadFail);
void Hal_EfuseParseEEPROMVer_8723B(struct adapter *padapter, u8 *hwinfo,
				   bool AutoLoadFail);
void Hal_EfuseParseChnlPlan_8723B(struct adapter *padapter, u8 *hwinfo,
				  bool AutoLoadFail);
void Hal_EfuseParseCustomerID_8723B(struct adapter *padapter, u8 *hwinfo,
				    bool AutoLoadFail);
void Hal_EfuseParseAntennaDiversity_8723B(struct adapter *padapter, u8 *hwinfo,
					  bool AutoLoadFail);
void Hal_EfuseParseXtal_8723B(struct adapter *padapter, u8 *hwinfo,
			      bool AutoLoadFail);
void Hal_EfuseParseThermalMeter_8723B(struct adapter *padapter, u8 *hwinfo,
				      u8 AutoLoadFail);
void Hal_EfuseParsePackageType_8723B(struct adapter *padapter, u8 *hwinfo,
				     bool AutoLoadFail);
void Hal_EfuseParseVoltage_8723B(struct adapter *padapter, u8 *hwinfo,
				 bool AutoLoadFail);
void C2HPacketHandler_8723B(struct adapter *padapter, u8 *pbuffer, u16 length);
void rtl8723b_set_hal_ops(struct hal_ops *pHalFunc);
void SetHwReg8723B(struct adapter *padapter, u8 variable, u8 *val);
void GetHwReg8723B(struct adapter *padapter, u8 variable, u8 *val);
u8 SetHalDefVar8723B(struct adapter *padapter, enum hal_def_variable variable,
		     void *pval);
u8 GetHalDefVar8723B(struct adapter *padapter, enum hal_def_variable variable,
		     void *pval);
void rtl8723b_InitBeaconParameters(struct adapter *padapter);
void _InitBurstPktLen_8723BS(struct adapter *adapter);
void _8051Reset8723(struct adapter *padapter);
void rtl8723b_start_thread(struct adapter *padapter);
void rtl8723b_stop_thread(struct adapter *padapter);
int FirmwareDownloadBT(struct adapter *adapter, struct rt_firmware *firmware);
void CCX_FwC2HTxRpt_8723b(struct adapter *padapter, u8 *pdata, u8 len);
s32 c2h_id_filter_ccx_8723b(u8 *buf);
s32 c2h_handler_8723b(struct adapter *padapter, u8 *pC2hEvent);
u8 MRateToHwRate8723B(u8 rate);
u8 HwRateToMRate8723B(u8 rate);
void Hal_ReadRFGainOffset(struct adapter *padapter, u8 *hwinfo,
			  bool AutoLoadFail);
#endif
