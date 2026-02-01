 
 
#ifndef __RTW_EEPROM_H__
#define __RTW_EEPROM_H__


#define	RTL8712_EEPROM_ID			0x8712
 

#define	HWSET_MAX_SIZE_128		128
#define	HWSET_MAX_SIZE_256		256
#define	HWSET_MAX_SIZE_512		512

#define	EEPROM_MAX_SIZE			HWSET_MAX_SIZE_512

#define	CLOCK_RATE					50			 

 
#define EEPROM_READ_OPCODE		06
#define EEPROM_WRITE_OPCODE		05
#define EEPROM_ERASE_OPCODE		07
#define EEPROM_EWEN_OPCODE		19       
#define EEPROM_EWDS_OPCODE		16       

 
#define USA							0x555320
#define EUROPE						0x1  
#define JAPAN						0x2  

#define eeprom_cis0_sz	17
#define eeprom_cis1_sz	50

 
 
 
 
 
 
 
 
enum {
	RT_CID_DEFAULT = 0,
	RT_CID_8187_ALPHA0 = 1,
	RT_CID_8187_SERCOMM_PS = 2,
	RT_CID_8187_HW_LED = 3,
	RT_CID_8187_NETGEAR = 4,
	RT_CID_WHQL = 5,
	RT_CID_819x_CAMEO  = 6,
	RT_CID_819x_RUNTOP = 7,
	RT_CID_819x_Senao = 8,
	RT_CID_TOSHIBA = 9,	 
	RT_CID_819x_Netcore = 10,
	RT_CID_Nettronix = 11,
	RT_CID_DLINK = 12,
	RT_CID_PRONET = 13,
	RT_CID_COREGA = 14,
	RT_CID_CHINA_MOBILE = 15,
	RT_CID_819x_ALPHA = 16,
	RT_CID_819x_Sitecom = 17,
	RT_CID_CCX = 18,  
	RT_CID_819x_Lenovo = 19,
	RT_CID_819x_QMI = 20,
	RT_CID_819x_Edimax_Belkin = 21,
	RT_CID_819x_Sercomm_Belkin = 22,
	RT_CID_819x_CAMEO1 = 23,
	RT_CID_819x_MSI = 24,
	RT_CID_819x_Acer = 25,
	RT_CID_819x_AzWave_ASUS = 26,
	RT_CID_819x_AzWave = 27,  
	RT_CID_819x_HP = 28,
	RT_CID_819x_WNC_COREGA = 29,
	RT_CID_819x_Arcadyan_Belkin = 30,
	RT_CID_819x_SAMSUNG = 31,
	RT_CID_819x_CLEVO = 32,
	RT_CID_819x_DELL = 33,
	RT_CID_819x_PRONETS = 34,
	RT_CID_819x_Edimax_ASUS = 35,
	RT_CID_NETGEAR = 36,
	RT_CID_PLANEX = 37,
	RT_CID_CC_C = 38,
	RT_CID_819x_Xavi = 39,
	RT_CID_LENOVO_CHINA = 40,
	RT_CID_INTEL_CHINA = 41,
	RT_CID_TPLINK_HPWR = 42,
	RT_CID_819x_Sercomm_Netgear = 43,
	RT_CID_819x_ALPHA_Dlink = 44, 
	RT_CID_WNC_NEC = 45, 
	RT_CID_DNI_BUFFALO = 46, 
};

struct eeprom_priv {
	u8 bautoload_fail_flag;
	u8 bloadfile_fail_flag;
	u8 bloadmac_fail_flag;
	u8 EepromOrEfuse;

	u8 mac_addr[6];	 

	u16 	channel_plan;
	u16 	CustomerID;

	u8 efuse_eeprom_data[EEPROM_MAX_SIZE];  
	u8 adjuseVoltageVal;

	u8 EEPROMRFGainOffset;
	u8 EEPROMRFGainVal;

	u8 sdio_setting;
	u32 	ocr;
	u8 cis0[eeprom_cis0_sz];
	u8 cis1[eeprom_cis1_sz];
};

#endif   
