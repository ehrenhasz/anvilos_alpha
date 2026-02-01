 
 

#ifndef __DEVICE_H__
#define __DEVICE_H__

#include <linux/module.h>
#include <linux/types.h>
#include <linux/pci.h>
#include <linux/etherdevice.h>
#include <linux/skbuff.h>
#include <linux/interrupt.h>
#include <linux/crc32.h>
#include <net/mac80211.h>

 

#include "device_cfg.h"
#include "card.h"
#include "srom.h"
#include "desc.h"
#include "key.h"
#include "mac.h"

 

#define RATE_1M		0
#define RATE_2M		1
#define RATE_5M		2
#define RATE_11M	3
#define RATE_6M		4
#define RATE_9M		5
#define RATE_12M	6
#define RATE_18M	7
#define RATE_24M	8
#define RATE_36M	9
#define RATE_48M	10
#define RATE_54M	11
#define MAX_RATE	12

#define AUTO_FB_NONE            0
#define AUTO_FB_0               1
#define AUTO_FB_1               2

#define FB_RATE0                0
#define FB_RATE1                1

 
#define ANT_A                   0
#define ANT_B                   1
#define ANT_DIVERSITY           2
#define ANT_RXD_TXA             3
#define ANT_RXD_TXB             4
#define ANT_UNKNOWN             0xFF

#define BB_VGA_LEVEL            4
#define BB_VGA_CHANGE_THRESHOLD 16

#define MAKE_BEACON_RESERVED	10   

 

#define	AVAIL_TD(p, q)	((p)->opts.tx_descs[(q)] - ((p)->iTDUsed[(q)]))

 
#define BB_TYPE_11A    0
#define BB_TYPE_11B    1
#define BB_TYPE_11G    2

 
#define PK_TYPE_11A     0
#define PK_TYPE_11B     1
#define PK_TYPE_11GB    2
#define PK_TYPE_11GA    3

#define OWNED_BY_HOST	0
#define	OWNED_BY_NIC	1

struct vnt_options {
	int rx_descs0;		 
	int rx_descs1;		 
	int tx_descs[2];	 
	int int_works;		 
	int short_retry;
	int long_retry;
	int bbp_type;
	u32 flags;
};

struct vnt_private {
	struct pci_dev *pcid;
	 
	struct ieee80211_hw *hw;
	struct ieee80211_vif *vif;
	unsigned long key_entry_inuse;
	u32 basic_rates;
	u16 current_aid;
	int mc_list_count;
	u8 mac_hw;

 
	dma_addr_t                  pool_dma;
	dma_addr_t                  rd0_pool_dma;
	dma_addr_t                  rd1_pool_dma;

	dma_addr_t                  td0_pool_dma;
	dma_addr_t                  td1_pool_dma;

	dma_addr_t                  tx_bufs_dma0;
	dma_addr_t                  tx_bufs_dma1;
	dma_addr_t                  tx_beacon_dma;

	unsigned char *tx0_bufs;
	unsigned char *tx1_bufs;
	unsigned char *tx_beacon_bufs;

	void __iomem                *port_offset;
	u32                         memaddr;
	u32                         ioaddr;

	spinlock_t                  lock;

	volatile int                iTDUsed[TYPE_MAXTD];

	struct vnt_tx_desc *apCurrTD[TYPE_MAXTD];
	struct vnt_tx_desc *apTailTD[TYPE_MAXTD];

	struct vnt_tx_desc *apTD0Rings;
	struct vnt_tx_desc *apTD1Rings;

	struct vnt_rx_desc *aRD0Ring;
	struct vnt_rx_desc *aRD1Ring;
	struct vnt_rx_desc *pCurrRD[TYPE_MAXRD];

	struct vnt_options opts;

	u32                         flags;

	u32                         rx_buf_sz;
	u8 rx_rate;

	u32                         rx_bytes;

	 
	unsigned char local_id;
	unsigned char byRFType;

	unsigned char max_pwr_level;
	unsigned char byZoneType;
	bool bZoneRegExist;
	unsigned char byOriginalZonetype;

	unsigned char abyCurrentNetAddr[ETH_ALEN]; __aligned(2)
	bool bLinkPass;           

	unsigned int current_rssi;
	unsigned char byCurrSQ;

	unsigned long dwTxAntennaSel;
	unsigned long dwRxAntennaSel;
	unsigned char byAntennaCount;
	unsigned char byRxAntennaMode;
	unsigned char byTxAntennaMode;
	bool bTxRxAntInv;

	unsigned char *pbyTmpBuff;
	unsigned int	uSIFS;     
	unsigned int	uDIFS;     
	unsigned int	uEIFS;     
	unsigned int	uSlot;     
	unsigned int	uCwMin;    
	unsigned int	uCwMax;    
	 
	unsigned char bySIFS;
	unsigned char byDIFS;
	unsigned char byEIFS;
	unsigned char bySlot;
	unsigned char byCWMaxMin;

	u8		byBBType;  
	u8		byPacketType;  
	unsigned short wBasicRate;
	unsigned char byACKRate;
	unsigned char byTopOFDMBasicRate;
	unsigned char byTopCCKBasicRate;

	unsigned char byMinChannel;
	unsigned char byMaxChannel;

	unsigned char preamble_type;
	unsigned char byShortPreamble;

	unsigned short wCurrentRate;
	unsigned char byShortRetryLimit;
	unsigned char byLongRetryLimit;
	enum nl80211_iftype op_mode;
	bool bBSSIDFilter;
	unsigned short wMaxTransmitMSDULifetime;

	bool bEncryptionEnable;
	bool bLongHeader;
	bool short_slot_time;
	bool bProtectMode;
	bool bNonERPPresent;
	bool bBarkerPreambleMd;

	bool bRadioControlOff;
	bool radio_off;
	bool bEnablePSMode;
	unsigned short wListenInterval;
	bool bPWBitOn;

	 
	unsigned char byRadioCtl;
	unsigned char byGPIO;
	bool hw_radio_off;
	bool bPrvActive4RadioOFF;
	bool bGPIOBlockRead;

	 
	unsigned short wSeqCounter;
	unsigned short wBCNBufLen;
	bool bBeaconBufReady;
	bool bBeaconSent;
	bool bIsBeaconBufReadySet;
	unsigned int	cbBeaconBufReadySetCnt;
	bool bFixRate;
	u16 byCurrentCh;

	bool bAES;

	unsigned char byAutoFBCtrl;

	 
	bool bUpdateBBVGA;
	unsigned int	uBBVGADiffCount;
	unsigned char byBBVGANew;
	unsigned char byBBVGACurrent;
	unsigned char abyBBVGA[BB_VGA_LEVEL];
	long                    dbm_threshold[BB_VGA_LEVEL];

	unsigned char byBBPreEDRSSI;
	unsigned char byBBPreEDIndex;

	unsigned long dwDiagRefCount;

	 
	unsigned char byFOETuning;

	 
	unsigned char byCCKPwr;
	unsigned char byOFDMPwrG;
	unsigned char byCurPwr;
	char	 byCurPwrdBm;
	unsigned char abyCCKPwrTbl[CB_MAX_CHANNEL_24G + 1];
	unsigned char abyOFDMPwrTbl[CB_MAX_CHANNEL + 1];
	char	abyCCKDefaultPwr[CB_MAX_CHANNEL_24G + 1];
	char	abyOFDMDefaultPwr[CB_MAX_CHANNEL + 1];
	char	abyRegPwr[CB_MAX_CHANNEL + 1];
	char	abyLocalPwr[CB_MAX_CHANNEL + 1];

	 
	unsigned char byBBCR4d;
	unsigned char byBBCRc9;
	unsigned char byBBCR88;
	unsigned char byBBCR09;

	unsigned char abyEEPROM[EEP_MAX_CONTEXT_SIZE];  

	unsigned short wBeaconInterval;
	u16 wake_up_count;

	struct work_struct interrupt_work;

	struct ieee80211_low_level_stats low_stats;
};

#endif
