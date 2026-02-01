 
 
#ifndef __RTL8712_HAL_H__
#define __RTL8712_HAL_H__

enum _HW_VERSION {
	RTL8712_FPGA,
	RTL8712_1stCUT,	 
	RTL8712_2ndCUT,	 
	RTL8712_3rdCUT,	 
};

enum _LOOPBACK_TYPE {
	RTL8712_AIR_TRX = 0,
	RTL8712_MAC_LBK,
	RTL8712_BB_LBK,
	RTL8712_MAC_FW_LBK = 4,
	RTL8712_BB_FW_LBK = 8,
};

enum RTL871X_HCI_TYPE {
	RTL8712_SDIO,
	RTL8712_USB,
};

enum RTL8712_RF_CONFIG {
	RTL8712_RF_1T1R,
	RTL8712_RF_1T2R,
	RTL8712_RF_2T2R
};

enum _RTL8712_HCI_TYPE_ {
	RTL8712_HCI_TYPE_PCIE = 0x01,
	RTL8712_HCI_TYPE_AP_PCIE = 0x81,
	RTL8712_HCI_TYPE_USB = 0x02,
	RTL8712_HCI_TYPE_92USB = 0x02,
	RTL8712_HCI_TYPE_AP_USB = 0x82,
	RTL8712_HCI_TYPE_72USB = 0x12,
	RTL8712_HCI_TYPE_SDIO = 0x04,
	RTL8712_HCI_TYPE_72SDIO = 0x14
};

struct fw_priv {    
	 
	unsigned char signature_0;   
	unsigned char signature_1;   
	unsigned char hci_sel;  
	unsigned char chip_version;  
	unsigned char customer_ID_0;  
	unsigned char customer_ID_1;  
	unsigned char rf_config;   
	unsigned char usb_ep_num;   
	 
	unsigned char regulatory_class_0;  
	unsigned char regulatory_class_1;  
	unsigned char regulatory_class_2;  
	unsigned char regulatory_class_3;  
	unsigned char rfintfs;     
	unsigned char def_nettype;
	unsigned char turbo_mode;
	unsigned char low_power_mode; 
	 
	unsigned char lbk_mode;  
	unsigned char mp_mode;  
	unsigned char vcs_type;  
	unsigned char vcs_mode;  
	unsigned char rsvd022;
	unsigned char rsvd023;
	unsigned char rsvd024;
	unsigned char rsvd025;
	 
	unsigned char qos_en;     
	unsigned char bw_40MHz_en;    
	unsigned char AMSDU2AMPDU_en;    
	unsigned char AMPDU_en;    
	unsigned char rate_control_offload;  
	unsigned char aggregation_offload;   
	unsigned char rsvd030;
	unsigned char rsvd031;
	 
	unsigned char beacon_offload;    
	unsigned char MLME_offload;    
	unsigned char hwpc_offload;    
	unsigned char tcp_checksum_offload;  
	unsigned char tcp_offload;     
	unsigned char ps_control_offload;  
	unsigned char WWLAN_offload;    
	unsigned char rsvd040;
	 
	unsigned char tcp_tx_frame_len_L;   
	unsigned char tcp_tx_frame_len_H;   
	unsigned char tcp_rx_frame_len_L;   
	unsigned char tcp_rx_frame_len_H;   
	unsigned char rsvd050;
	unsigned char rsvd051;
	unsigned char rsvd052;
	unsigned char rsvd053;
};

struct fw_hdr { 
	unsigned short	signature;
	unsigned short	version;	 
	unsigned int		dmem_size;     
	unsigned int		img_IMEM_size;  
	unsigned int		img_SRAM_size;  
	unsigned int		fw_priv_sz;  
	unsigned short	efuse_addr;
	unsigned short	h2ccnd_resp_addr;
	unsigned int		SVNRevision;
	unsigned int		release_time;  
	struct fw_priv	fwpriv;
};

struct hal_priv {
	 
	struct  net_device *pipehdls_r8712[10];
	u8 (*hal_bus_init)(struct _adapter *adapter);
};

uint	 rtl8712_hal_init(struct _adapter *padapter);
int rtl871x_load_fw(struct _adapter *padapter);

#endif
