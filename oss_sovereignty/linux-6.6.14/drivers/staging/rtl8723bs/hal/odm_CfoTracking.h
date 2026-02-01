 
 

#ifndef	__ODMCFOTRACK_H__
#define    __ODMCFOTRACK_H__

#define		CFO_TH_XTAL_HIGH		20		 
#define		CFO_TH_XTAL_LOW			10		 
#define		CFO_TH_ATC			80		 

struct cfo_tracking {
	bool bATCStatus;
	bool largeCFOHit;
	bool bAdjust;
	u8 CrystalCap;
	u8 DefXCap;
	int CFO_tail[2];
	int CFO_ave_pre;
	u32 packetCount;
	u32 packetCount_pre;

	bool bForceXtalCap;
	bool bReset;
};

void ODM_CfoTrackingReset(void *pDM_VOID
);

void ODM_CfoTrackingInit(void *pDM_VOID);

void ODM_CfoTracking(void *pDM_VOID);

void odm_parsing_cfo(void *pDM_VOID, void *pPktinfo_VOID, s8 *pcfotail);

#endif
