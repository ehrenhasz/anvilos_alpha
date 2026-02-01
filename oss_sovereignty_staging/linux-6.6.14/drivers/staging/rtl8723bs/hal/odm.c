
 

#include "odm_precomp.h"

 

u32 OFDMSwingTable[OFDM_TABLE_SIZE] = {
	0x7f8001fe,  
	0x788001e2,  
	0x71c001c7,  
	0x6b8001ae,  
	0x65400195,  
	0x5fc0017f,  
	0x5a400169,  
	0x55400155,  
	0x50800142,  
	0x4c000130,  
	0x47c0011f,  
	0x43c0010f,  
	0x40000100,  
	0x3c8000f2,  
	0x390000e4,  
	0x35c000d7,  
	0x32c000cb,  
	0x300000c0,  
	0x2d4000b5,  
	0x2ac000ab,  
	0x288000a2,  
	0x26000098,  
	0x24000090,  
	0x22000088,  
	0x20000080,  
	0x1e400079,  
	0x1c800072,  
	0x1b00006c,  
	0x19800066,  
	0x18000060,  
	0x16c0005b,  
	0x15800056,  
	0x14400051,  
	0x1300004c,  
	0x12000048,  
	0x11000044,  
	0x10000040,  
};

u8 CCKSwingTable_Ch1_Ch13[CCK_TABLE_SIZE][8] = {
	{0x36, 0x35, 0x2e, 0x25, 0x1c, 0x12, 0x09, 0x04},  
	{0x33, 0x32, 0x2b, 0x23, 0x1a, 0x11, 0x08, 0x04},  
	{0x30, 0x2f, 0x29, 0x21, 0x19, 0x10, 0x08, 0x03},  
	{0x2d, 0x2d, 0x27, 0x1f, 0x18, 0x0f, 0x08, 0x03},  
	{0x2b, 0x2a, 0x25, 0x1e, 0x16, 0x0e, 0x07, 0x03},  
	{0x28, 0x28, 0x22, 0x1c, 0x15, 0x0d, 0x07, 0x03},  
	{0x26, 0x25, 0x21, 0x1b, 0x14, 0x0d, 0x06, 0x03},  
	{0x24, 0x23, 0x1f, 0x19, 0x13, 0x0c, 0x06, 0x03},  
	{0x22, 0x21, 0x1d, 0x18, 0x11, 0x0b, 0x06, 0x02},  
	{0x20, 0x20, 0x1b, 0x16, 0x11, 0x08, 0x05, 0x02},  
	{0x1f, 0x1e, 0x1a, 0x15, 0x10, 0x0a, 0x05, 0x02},  
	{0x1d, 0x1c, 0x18, 0x14, 0x0f, 0x0a, 0x05, 0x02},  
	{0x1b, 0x1a, 0x17, 0x13, 0x0e, 0x09, 0x04, 0x02},  
	{0x1a, 0x19, 0x16, 0x12, 0x0d, 0x09, 0x04, 0x02},  
	{0x18, 0x17, 0x15, 0x11, 0x0c, 0x08, 0x04, 0x02},  
	{0x17, 0x16, 0x13, 0x10, 0x0c, 0x08, 0x04, 0x02},  
	{0x16, 0x15, 0x12, 0x0f, 0x0b, 0x07, 0x04, 0x01},  
	{0x14, 0x14, 0x11, 0x0e, 0x0b, 0x07, 0x03, 0x02},  
	{0x13, 0x13, 0x10, 0x0d, 0x0a, 0x06, 0x03, 0x01},  
	{0x12, 0x12, 0x0f, 0x0c, 0x09, 0x06, 0x03, 0x01},  
	{0x11, 0x11, 0x0f, 0x0c, 0x09, 0x06, 0x03, 0x01},  
	{0x10, 0x10, 0x0e, 0x0b, 0x08, 0x05, 0x03, 0x01},  
	{0x0f, 0x0f, 0x0d, 0x0b, 0x08, 0x05, 0x03, 0x01},  
	{0x0e, 0x0e, 0x0c, 0x0a, 0x08, 0x05, 0x02, 0x01},  
	{0x0d, 0x0d, 0x0c, 0x0a, 0x07, 0x05, 0x02, 0x01},  
	{0x0d, 0x0c, 0x0b, 0x09, 0x07, 0x04, 0x02, 0x01},  
	{0x0c, 0x0c, 0x0a, 0x09, 0x06, 0x04, 0x02, 0x01},  
	{0x0b, 0x0b, 0x0a, 0x08, 0x06, 0x04, 0x02, 0x01},  
	{0x0b, 0x0a, 0x09, 0x08, 0x06, 0x04, 0x02, 0x01},  
	{0x0a, 0x0a, 0x09, 0x07, 0x05, 0x03, 0x02, 0x01},  
	{0x0a, 0x09, 0x08, 0x07, 0x05, 0x03, 0x02, 0x01},  
	{0x09, 0x09, 0x08, 0x06, 0x05, 0x03, 0x01, 0x01},  
	{0x09, 0x08, 0x07, 0x06, 0x04, 0x03, 0x01, 0x01}	 
};

u8 CCKSwingTable_Ch14[CCK_TABLE_SIZE][8] = {
	{0x36, 0x35, 0x2e, 0x1b, 0x00, 0x00, 0x00, 0x00},  
	{0x33, 0x32, 0x2b, 0x19, 0x00, 0x00, 0x00, 0x00},  
	{0x30, 0x2f, 0x29, 0x18, 0x00, 0x00, 0x00, 0x00},  
	{0x2d, 0x2d, 0x17, 0x17, 0x00, 0x00, 0x00, 0x00},  
	{0x2b, 0x2a, 0x25, 0x15, 0x00, 0x00, 0x00, 0x00},  
	{0x28, 0x28, 0x24, 0x14, 0x00, 0x00, 0x00, 0x00},  
	{0x26, 0x25, 0x21, 0x13, 0x00, 0x00, 0x00, 0x00},  
	{0x24, 0x23, 0x1f, 0x12, 0x00, 0x00, 0x00, 0x00},  
	{0x22, 0x21, 0x1d, 0x11, 0x00, 0x00, 0x00, 0x00},  
	{0x20, 0x20, 0x1b, 0x10, 0x00, 0x00, 0x00, 0x00},  
	{0x1f, 0x1e, 0x1a, 0x0f, 0x00, 0x00, 0x00, 0x00},  
	{0x1d, 0x1c, 0x18, 0x0e, 0x00, 0x00, 0x00, 0x00},  
	{0x1b, 0x1a, 0x17, 0x0e, 0x00, 0x00, 0x00, 0x00},  
	{0x1a, 0x19, 0x16, 0x0d, 0x00, 0x00, 0x00, 0x00},  
	{0x18, 0x17, 0x15, 0x0c, 0x00, 0x00, 0x00, 0x00},  
	{0x17, 0x16, 0x13, 0x0b, 0x00, 0x00, 0x00, 0x00},  
	{0x16, 0x15, 0x12, 0x0b, 0x00, 0x00, 0x00, 0x00},  
	{0x14, 0x14, 0x11, 0x0a, 0x00, 0x00, 0x00, 0x00},  
	{0x13, 0x13, 0x10, 0x0a, 0x00, 0x00, 0x00, 0x00},  
	{0x12, 0x12, 0x0f, 0x09, 0x00, 0x00, 0x00, 0x00},  
	{0x11, 0x11, 0x0f, 0x09, 0x00, 0x00, 0x00, 0x00},  
	{0x10, 0x10, 0x0e, 0x08, 0x00, 0x00, 0x00, 0x00},  
	{0x0f, 0x0f, 0x0d, 0x08, 0x00, 0x00, 0x00, 0x00},  
	{0x0e, 0x0e, 0x0c, 0x07, 0x00, 0x00, 0x00, 0x00},  
	{0x0d, 0x0d, 0x0c, 0x07, 0x00, 0x00, 0x00, 0x00},  
	{0x0d, 0x0c, 0x0b, 0x06, 0x00, 0x00, 0x00, 0x00},  
	{0x0c, 0x0c, 0x0a, 0x06, 0x00, 0x00, 0x00, 0x00},  
	{0x0b, 0x0b, 0x0a, 0x06, 0x00, 0x00, 0x00, 0x00},  
	{0x0b, 0x0a, 0x09, 0x05, 0x00, 0x00, 0x00, 0x00},  
	{0x0a, 0x0a, 0x09, 0x05, 0x00, 0x00, 0x00, 0x00},  
	{0x0a, 0x09, 0x08, 0x05, 0x00, 0x00, 0x00, 0x00},  
	{0x09, 0x09, 0x08, 0x05, 0x00, 0x00, 0x00, 0x00},  
	{0x09, 0x08, 0x07, 0x04, 0x00, 0x00, 0x00, 0x00}	 
};

u32 OFDMSwingTable_New[OFDM_TABLE_SIZE] = {
	0x0b40002d,  
	0x0c000030,  
	0x0cc00033,  
	0x0d800036,  
	0x0e400039,  
	0x0f00003c,  
	0x10000040,  
	0x11000044,  
	0x12000048,  
	0x1300004c,  
	0x14400051,  
	0x15800056,  
	0x16c0005b,  
	0x18000060,  
	0x19800066,  
	0x1b00006c,  
	0x1c800072,  
	0x1e400079,  
	0x20000080,  
	0x22000088,  
	0x24000090,  
	0x26000098,  
	0x288000a2,  
	0x2ac000ab,  
	0x2d4000b5,  
	0x300000c0,  
	0x32c000cb,  
	0x35c000d7,  
	0x390000e4,  
	0x3c8000f2,  
	0x40000100,  
	0x43c0010f,  
	0x47c0011f,  
	0x4c000130,  
	0x50800142,  
	0x55400155,  
	0x5a400169,  
	0x5fc0017f,  
	0x65400195,  
	0x6b8001ae,  
	0x71c001c7,  
	0x788001e2,  
	0x7f8001fe   
};

u8 CCKSwingTable_Ch1_Ch13_New[CCK_TABLE_SIZE][8] = {
	{0x09, 0x08, 0x07, 0x06, 0x04, 0x03, 0x01, 0x01},  
	{0x09, 0x09, 0x08, 0x06, 0x05, 0x03, 0x01, 0x01},  
	{0x0a, 0x09, 0x08, 0x07, 0x05, 0x03, 0x02, 0x01},  
	{0x0a, 0x0a, 0x09, 0x07, 0x05, 0x03, 0x02, 0x01},  
	{0x0b, 0x0a, 0x09, 0x08, 0x06, 0x04, 0x02, 0x01},  
	{0x0b, 0x0b, 0x0a, 0x08, 0x06, 0x04, 0x02, 0x01},  
	{0x0c, 0x0c, 0x0a, 0x09, 0x06, 0x04, 0x02, 0x01},  
	{0x0d, 0x0c, 0x0b, 0x09, 0x07, 0x04, 0x02, 0x01},  
	{0x0d, 0x0d, 0x0c, 0x0a, 0x07, 0x05, 0x02, 0x01},  
	{0x0e, 0x0e, 0x0c, 0x0a, 0x08, 0x05, 0x02, 0x01},  
	{0x0f, 0x0f, 0x0d, 0x0b, 0x08, 0x05, 0x03, 0x01},  
	{0x10, 0x10, 0x0e, 0x0b, 0x08, 0x05, 0x03, 0x01},  
	{0x11, 0x11, 0x0f, 0x0c, 0x09, 0x06, 0x03, 0x01},  
	{0x12, 0x12, 0x0f, 0x0c, 0x09, 0x06, 0x03, 0x01},  
	{0x13, 0x13, 0x10, 0x0d, 0x0a, 0x06, 0x03, 0x01},  
	{0x14, 0x14, 0x11, 0x0e, 0x0b, 0x07, 0x03, 0x02},  
	{0x16, 0x15, 0x12, 0x0f, 0x0b, 0x07, 0x04, 0x01},  
	{0x17, 0x16, 0x13, 0x10, 0x0c, 0x08, 0x04, 0x02},  
	{0x18, 0x17, 0x15, 0x11, 0x0c, 0x08, 0x04, 0x02},  
	{0x1a, 0x19, 0x16, 0x12, 0x0d, 0x09, 0x04, 0x02},  
	{0x1b, 0x1a, 0x17, 0x13, 0x0e, 0x09, 0x04, 0x02},  
	{0x1d, 0x1c, 0x18, 0x14, 0x0f, 0x0a, 0x05, 0x02},  
	{0x1f, 0x1e, 0x1a, 0x15, 0x10, 0x0a, 0x05, 0x02},  
	{0x20, 0x20, 0x1b, 0x16, 0x11, 0x08, 0x05, 0x02},  
	{0x22, 0x21, 0x1d, 0x18, 0x11, 0x0b, 0x06, 0x02},  
	{0x24, 0x23, 0x1f, 0x19, 0x13, 0x0c, 0x06, 0x03},  
	{0x26, 0x25, 0x21, 0x1b, 0x14, 0x0d, 0x06, 0x03},  
	{0x28, 0x28, 0x22, 0x1c, 0x15, 0x0d, 0x07, 0x03},  
	{0x2b, 0x2a, 0x25, 0x1e, 0x16, 0x0e, 0x07, 0x03},  
	{0x2d, 0x2d, 0x27, 0x1f, 0x18, 0x0f, 0x08, 0x03},  
	{0x30, 0x2f, 0x29, 0x21, 0x19, 0x10, 0x08, 0x03},  
	{0x33, 0x32, 0x2b, 0x23, 0x1a, 0x11, 0x08, 0x04},  
	{0x36, 0x35, 0x2e, 0x25, 0x1c, 0x12, 0x09, 0x04}	 
};

u8 CCKSwingTable_Ch14_New[CCK_TABLE_SIZE][8] = {
	{0x09, 0x08, 0x07, 0x04, 0x00, 0x00, 0x00, 0x00},  
	{0x09, 0x09, 0x08, 0x05, 0x00, 0x00, 0x00, 0x00},  
	{0x0a, 0x09, 0x08, 0x05, 0x00, 0x00, 0x00, 0x00},  
	{0x0a, 0x0a, 0x09, 0x05, 0x00, 0x00, 0x00, 0x00},  
	{0x0b, 0x0a, 0x09, 0x05, 0x00, 0x00, 0x00, 0x00},  
	{0x0b, 0x0b, 0x0a, 0x06, 0x00, 0x00, 0x00, 0x00},  
	{0x0c, 0x0c, 0x0a, 0x06, 0x00, 0x00, 0x00, 0x00},  
	{0x0d, 0x0c, 0x0b, 0x06, 0x00, 0x00, 0x00, 0x00},  
	{0x0d, 0x0d, 0x0c, 0x07, 0x00, 0x00, 0x00, 0x00},  
	{0x0e, 0x0e, 0x0c, 0x07, 0x00, 0x00, 0x00, 0x00},  
	{0x0f, 0x0f, 0x0d, 0x08, 0x00, 0x00, 0x00, 0x00},  
	{0x10, 0x10, 0x0e, 0x08, 0x00, 0x00, 0x00, 0x00},  
	{0x11, 0x11, 0x0f, 0x09, 0x00, 0x00, 0x00, 0x00},  
	{0x12, 0x12, 0x0f, 0x09, 0x00, 0x00, 0x00, 0x00},  
	{0x13, 0x13, 0x10, 0x0a, 0x00, 0x00, 0x00, 0x00},  
	{0x14, 0x14, 0x11, 0x0a, 0x00, 0x00, 0x00, 0x00},  
	{0x16, 0x15, 0x12, 0x0b, 0x00, 0x00, 0x00, 0x00},  
	{0x17, 0x16, 0x13, 0x0b, 0x00, 0x00, 0x00, 0x00},  
	{0x18, 0x17, 0x15, 0x0c, 0x00, 0x00, 0x00, 0x00},  
	{0x1a, 0x19, 0x16, 0x0d, 0x00, 0x00, 0x00, 0x00},  
	{0x1b, 0x1a, 0x17, 0x0e, 0x00, 0x00, 0x00, 0x00},  
	{0x1d, 0x1c, 0x18, 0x0e, 0x00, 0x00, 0x00, 0x00},  
	{0x1f, 0x1e, 0x1a, 0x0f, 0x00, 0x00, 0x00, 0x00},  
	{0x20, 0x20, 0x1b, 0x10, 0x00, 0x00, 0x00, 0x00},  
	{0x22, 0x21, 0x1d, 0x11, 0x00, 0x00, 0x00, 0x00},  
	{0x24, 0x23, 0x1f, 0x12, 0x00, 0x00, 0x00, 0x00},  
	{0x26, 0x25, 0x21, 0x13, 0x00, 0x00, 0x00, 0x00},  
	{0x28, 0x28, 0x24, 0x14, 0x00, 0x00, 0x00, 0x00},  
	{0x2b, 0x2a, 0x25, 0x15, 0x00, 0x00, 0x00, 0x00},  
	{0x2d, 0x2d, 0x17, 0x17, 0x00, 0x00, 0x00, 0x00},  
	{0x30, 0x2f, 0x29, 0x18, 0x00, 0x00, 0x00, 0x00},  
	{0x33, 0x32, 0x2b, 0x19, 0x00, 0x00, 0x00, 0x00},  
	{0x36, 0x35, 0x2e, 0x1b, 0x00, 0x00, 0x00, 0x00}	 
};

u32 TxScalingTable_Jaguar[TXSCALE_TABLE_SIZE] = {
	0x081,  
	0x088,  
	0x090,  
	0x099,  
	0x0A2,  
	0x0AC,  
	0x0B6,  
	0x0C0,  
	0x0CC,  
	0x0D8,  
	0x0E5,  
	0x0F2,  
	0x101,  
	0x110,  
	0x120,  
	0x131,  
	0x143,  
	0x156,  
	0x16A,  
	0x180,  
	0x197,  
	0x1AF,  
	0x1C8,  
	0x1E3,  
	0x200,  
	0x21E,  
	0x23E,  
	0x261,  
	0x285,  
	0x2AB,  
	0x2D3,  
	0x2FE,  
	0x32B,  
	0x35C,  
	0x38E,  
	0x3C4,  
	0x3FE   
};

 

static void odm_CommonInfoSelfInit(struct dm_odm_t *pDM_Odm)
{
	pDM_Odm->bCckHighPower = (bool) PHY_QueryBBReg(pDM_Odm->Adapter, ODM_REG(CCK_RPT_FORMAT, pDM_Odm), ODM_BIT(CCK_RPT_FORMAT, pDM_Odm));
	pDM_Odm->RFPathRxEnable = (u8) PHY_QueryBBReg(pDM_Odm->Adapter, ODM_REG(BB_RX_PATH, pDM_Odm), ODM_BIT(BB_RX_PATH, pDM_Odm));

	pDM_Odm->TxRate = 0xFF;
}

static void odm_CommonInfoSelfUpdate(struct dm_odm_t *pDM_Odm)
{
	u8 EntryCnt = 0;
	u8 i;
	PSTA_INFO_T	pEntry;

	if (*(pDM_Odm->pBandWidth) == ODM_BW40M) {
		if (*(pDM_Odm->pSecChOffset) == 1)
			pDM_Odm->ControlChannel = *(pDM_Odm->pChannel)-2;
		else if (*(pDM_Odm->pSecChOffset) == 2)
			pDM_Odm->ControlChannel = *(pDM_Odm->pChannel)+2;
	} else
		pDM_Odm->ControlChannel = *(pDM_Odm->pChannel);

	for (i = 0; i < ODM_ASSOCIATE_ENTRY_NUM; i++) {
		pEntry = pDM_Odm->pODM_StaInfo[i];
		if (IS_STA_VALID(pEntry))
			EntryCnt++;
	}

	if (EntryCnt == 1)
		pDM_Odm->bOneEntryOnly = true;
	else
		pDM_Odm->bOneEntryOnly = false;
}

static void odm_CmnInfoInit_Debug(struct dm_odm_t *pDM_Odm)
{
}

static void odm_BasicDbgMessage(struct dm_odm_t *pDM_Odm)
{
}

 
 
 
 
 
 

static void odm_RateAdaptiveMaskInit(struct dm_odm_t *pDM_Odm)
{
	struct odm_rate_adaptive *pOdmRA = &pDM_Odm->RateAdaptive;

	pOdmRA->Type = DM_Type_ByDriver;
	if (pOdmRA->Type == DM_Type_ByDriver)
		pDM_Odm->bUseRAMask = true;
	else
		pDM_Odm->bUseRAMask = false;

	pOdmRA->RATRState = DM_RATR_STA_INIT;
	pOdmRA->LdpcThres = 35;
	pOdmRA->bUseLdpc = false;
	pOdmRA->HighRSSIThresh = 50;
	pOdmRA->LowRSSIThresh = 20;
}

u32 ODM_Get_Rate_Bitmap(
	struct dm_odm_t *pDM_Odm,
	u32 macid,
	u32 ra_mask,
	u8 rssi_level
)
{
	PSTA_INFO_T	pEntry;
	u32 rate_bitmap = 0;
	u8 WirelessMode;

	pEntry = pDM_Odm->pODM_StaInfo[macid];
	if (!IS_STA_VALID(pEntry))
		return ra_mask;

	WirelessMode = pEntry->wireless_mode;

	switch (WirelessMode) {
	case ODM_WM_B:
		if (ra_mask & 0x0000000c)		 
			rate_bitmap = 0x0000000d;
		else
			rate_bitmap = 0x0000000f;
		break;

	case (ODM_WM_G):
		if (rssi_level == DM_RATR_STA_HIGH)
			rate_bitmap = 0x00000f00;
		else
			rate_bitmap = 0x00000ff0;
		break;

	case (ODM_WM_B|ODM_WM_G):
		if (rssi_level == DM_RATR_STA_HIGH)
			rate_bitmap = 0x00000f00;
		else if (rssi_level == DM_RATR_STA_MIDDLE)
			rate_bitmap = 0x00000ff0;
		else
			rate_bitmap = 0x00000ff5;
		break;

	case (ODM_WM_B|ODM_WM_G|ODM_WM_N24G):
	case (ODM_WM_B|ODM_WM_N24G):
	case (ODM_WM_G|ODM_WM_N24G):
		if (rssi_level == DM_RATR_STA_HIGH)
			rate_bitmap = 0x000f0000;
		else if (rssi_level == DM_RATR_STA_MIDDLE)
			rate_bitmap = 0x000ff000;
		else {
			if (*(pDM_Odm->pBandWidth) == ODM_BW40M)
				rate_bitmap = 0x000ff015;
			else
				rate_bitmap = 0x000ff005;
		}
		break;

	default:
		rate_bitmap = 0x0fffffff;
		break;
	}

	return ra_mask & rate_bitmap;

}

static void odm_RefreshRateAdaptiveMaskCE(struct dm_odm_t *pDM_Odm)
{
	u8 i;
	struct adapter *padapter =  pDM_Odm->Adapter;

	if (padapter->bDriverStopped) {
		return;
	}

	if (!pDM_Odm->bUseRAMask) {
		return;
	}

	for (i = 0; i < ODM_ASSOCIATE_ENTRY_NUM; i++) {
		PSTA_INFO_T pstat = pDM_Odm->pODM_StaInfo[i];

		if (IS_STA_VALID(pstat)) {
			if (is_multicast_ether_addr(pstat->hwaddr))   
				continue;

			if (true == ODM_RAStateCheck(pDM_Odm, pstat->rssi_stat.UndecoratedSmoothedPWDB, false, &pstat->rssi_level)) {
				 
				rtw_hal_update_ra_mask(pstat, pstat->rssi_level);
			}

		}
	}
}

 
static void odm_RefreshRateAdaptiveMask(struct dm_odm_t *pDM_Odm)
{

	if (!(pDM_Odm->SupportAbility & ODM_BB_RA_MASK)) {
		return;
	}
	odm_RefreshRateAdaptiveMaskCE(pDM_Odm);
}

 
 
bool ODM_RAStateCheck(
	struct dm_odm_t *pDM_Odm,
	s32 RSSI,
	bool bForceUpdate,
	u8 *pRATRState
)
{
	struct odm_rate_adaptive *pRA = &pDM_Odm->RateAdaptive;
	const u8 GoUpGap = 5;
	u8 HighRSSIThreshForRA = pRA->HighRSSIThresh;
	u8 LowRSSIThreshForRA = pRA->LowRSSIThresh;
	u8 RATRState;

	 
	 
	 
	switch (*pRATRState) {
	case DM_RATR_STA_INIT:
	case DM_RATR_STA_HIGH:
		break;

	case DM_RATR_STA_MIDDLE:
		HighRSSIThreshForRA += GoUpGap;
		break;

	case DM_RATR_STA_LOW:
		HighRSSIThreshForRA += GoUpGap;
		LowRSSIThreshForRA += GoUpGap;
		break;

	default:
		netdev_dbg(pDM_Odm->Adapter->pnetdev,
			   "wrong rssi level setting %d !", *pRATRState);
		break;
	}

	 
	if (RSSI > HighRSSIThreshForRA)
		RATRState = DM_RATR_STA_HIGH;
	else if (RSSI > LowRSSIThreshForRA)
		RATRState = DM_RATR_STA_MIDDLE;
	else
		RATRState = DM_RATR_STA_LOW;
	 

	if (*pRATRState != RATRState || bForceUpdate) {
		*pRATRState = RATRState;
		return true;
	}

	return false;
}

 

 
 
 

static void odm_RSSIMonitorInit(struct dm_odm_t *pDM_Odm)
{
	struct ra_t *pRA_Table = &pDM_Odm->DM_RA_Table;

	pRA_Table->firstconnect = false;

}

static void FindMinimumRSSI(struct adapter *padapter)
{
	struct hal_com_data	*pHalData = GET_HAL_DATA(padapter);
	struct dm_priv *pdmpriv = &pHalData->dmpriv;
	struct dm_odm_t *pDM_Odm = &pHalData->odmpriv;

	 

	if (
		(pDM_Odm->bLinked != true) &&
		(pdmpriv->EntryMinUndecoratedSmoothedPWDB == 0)
	) {
		pdmpriv->MinUndecoratedPWDBForDM = 0;
	} else
		pdmpriv->MinUndecoratedPWDBForDM = pdmpriv->EntryMinUndecoratedSmoothedPWDB;
}

static void odm_RSSIMonitorCheckCE(struct dm_odm_t *pDM_Odm)
{
	struct adapter *Adapter = pDM_Odm->Adapter;
	struct hal_com_data	*pHalData = GET_HAL_DATA(Adapter);
	struct dm_priv *pdmpriv = &pHalData->dmpriv;
	int i;
	int tmpEntryMaxPWDB = 0, tmpEntryMinPWDB = 0xff;
	u8 sta_cnt = 0;
	u32 PWDB_rssi[NUM_STA] = {0}; 
	struct ra_t *pRA_Table = &pDM_Odm->DM_RA_Table;

	if (pDM_Odm->bLinked != true)
		return;

	pRA_Table->firstconnect = pDM_Odm->bLinked;

	 
	{
		struct sta_info *psta;

		for (i = 0; i < ODM_ASSOCIATE_ENTRY_NUM; i++) {
			psta = pDM_Odm->pODM_StaInfo[i];
			if (IS_STA_VALID(psta)) {
				if (is_multicast_ether_addr(psta->hwaddr))   
					continue;

				if (psta->rssi_stat.UndecoratedSmoothedPWDB == (-1))
					continue;

				if (psta->rssi_stat.UndecoratedSmoothedPWDB < tmpEntryMinPWDB)
					tmpEntryMinPWDB = psta->rssi_stat.UndecoratedSmoothedPWDB;

				if (psta->rssi_stat.UndecoratedSmoothedPWDB > tmpEntryMaxPWDB)
					tmpEntryMaxPWDB = psta->rssi_stat.UndecoratedSmoothedPWDB;

				if (psta->rssi_stat.UndecoratedSmoothedPWDB != (-1))
					PWDB_rssi[sta_cnt++] = (psta->mac_id | (psta->rssi_stat.UndecoratedSmoothedPWDB<<16));
			}
		}

		 

		for (i = 0; i < sta_cnt; i++) {
			if (PWDB_rssi[i] != (0)) {
				if (pHalData->fw_ractrl == true) 
					rtl8723b_set_rssi_cmd(Adapter, (u8 *)(&PWDB_rssi[i]));
			}
		}
	}



	if (tmpEntryMaxPWDB != 0)	 
		pdmpriv->EntryMaxUndecoratedSmoothedPWDB = tmpEntryMaxPWDB;
	else
		pdmpriv->EntryMaxUndecoratedSmoothedPWDB = 0;

	if (tmpEntryMinPWDB != 0xff)  
		pdmpriv->EntryMinUndecoratedSmoothedPWDB = tmpEntryMinPWDB;
	else
		pdmpriv->EntryMinUndecoratedSmoothedPWDB = 0;

	FindMinimumRSSI(Adapter); 

	pDM_Odm->RSSI_Min = pdmpriv->MinUndecoratedPWDBForDM;
	 
}

static void odm_RSSIMonitorCheck(struct dm_odm_t *pDM_Odm)
{
	if (!(pDM_Odm->SupportAbility & ODM_BB_RSSI_MONITOR))
		return;

	odm_RSSIMonitorCheckCE(pDM_Odm);

}	 

 
 
 
static void odm_SwAntDetectInit(struct dm_odm_t *pDM_Odm)
{
	struct swat_t *pDM_SWAT_Table = &pDM_Odm->DM_SWAT_Table;

	pDM_SWAT_Table->SWAS_NoLink_BK_Reg92c = rtw_read32(pDM_Odm->Adapter, rDPDT_control);
	pDM_SWAT_Table->PreAntenna = MAIN_ANT;
	pDM_SWAT_Table->CurAntenna = MAIN_ANT;
	pDM_SWAT_Table->SWAS_NoLink_State = 0;
}

 
 
 

static u8 getSwingIndex(struct dm_odm_t *pDM_Odm)
{
	struct adapter *Adapter = pDM_Odm->Adapter;
	u8 i = 0;
	u32 bbSwing;
	u32 swingTableSize;
	u32 *pSwingTable;

	bbSwing = PHY_QueryBBReg(Adapter, rOFDM0_XATxIQImbalance, 0xFFC00000);

	pSwingTable = OFDMSwingTable_New;
	swingTableSize = OFDM_TABLE_SIZE;

	for (i = 0; i < swingTableSize; ++i) {
		u32 tableValue = pSwingTable[i];

		if (tableValue >= 0x100000)
			tableValue >>= 22;
		if (bbSwing == tableValue)
			break;
	}
	return i;
}

void odm_TXPowerTrackingInit(struct dm_odm_t *pDM_Odm)
{
	u8 defaultSwingIndex = getSwingIndex(pDM_Odm);
	u8 p = 0;
	struct adapter *Adapter = pDM_Odm->Adapter;
	struct hal_com_data *pHalData = GET_HAL_DATA(Adapter);


	struct dm_priv *pdmpriv = &pHalData->dmpriv;

	pdmpriv->bTXPowerTracking = true;
	pdmpriv->TXPowercount = 0;
	pdmpriv->bTXPowerTrackingInit = false;

	if (*(pDM_Odm->mp_mode) != 1)
		pdmpriv->TxPowerTrackControl = true;
	else
		pdmpriv->TxPowerTrackControl = false;

	 
	pDM_Odm->RFCalibrateInfo.ThermalValue = pHalData->EEPROMThermalMeter;
	pDM_Odm->RFCalibrateInfo.ThermalValue_IQK = pHalData->EEPROMThermalMeter;
	pDM_Odm->RFCalibrateInfo.ThermalValue_LCK = pHalData->EEPROMThermalMeter;

	 
	pDM_Odm->DefaultOfdmIndex = (defaultSwingIndex >= OFDM_TABLE_SIZE) ? 30 : defaultSwingIndex;
	pDM_Odm->DefaultCckIndex = 20;

	pDM_Odm->BbSwingIdxCckBase = pDM_Odm->DefaultCckIndex;
	pDM_Odm->RFCalibrateInfo.CCK_index = pDM_Odm->DefaultCckIndex;

	for (p = RF_PATH_A; p < MAX_RF_PATH; ++p) {
		pDM_Odm->BbSwingIdxOfdmBase[p] = pDM_Odm->DefaultOfdmIndex;
		pDM_Odm->RFCalibrateInfo.OFDM_index[p] = pDM_Odm->DefaultOfdmIndex;
		pDM_Odm->RFCalibrateInfo.DeltaPowerIndex[p] = 0;
		pDM_Odm->RFCalibrateInfo.DeltaPowerIndexLast[p] = 0;
		pDM_Odm->RFCalibrateInfo.PowerIndexOffset[p] = 0;
	}

}

void ODM_TXPowerTrackingCheck(struct dm_odm_t *pDM_Odm)
{
	struct adapter *Adapter = pDM_Odm->Adapter;

	if (!(pDM_Odm->SupportAbility & ODM_RF_TX_PWR_TRACK))
		return;

	if (!pDM_Odm->RFCalibrateInfo.TM_Trigger) {  
		PHY_SetRFReg(pDM_Odm->Adapter, RF_PATH_A, RF_T_METER_NEW, (BIT17 | BIT16), 0x03);

		pDM_Odm->RFCalibrateInfo.TM_Trigger = 1;
		return;
	} else {
		ODM_TXPowerTrackingCallback_ThermalMeter(Adapter);
		pDM_Odm->RFCalibrateInfo.TM_Trigger = 0;
	}
}

 
 
 

 
 
 
void ODM_DMInit(struct dm_odm_t *pDM_Odm)
{

	odm_CommonInfoSelfInit(pDM_Odm);
	odm_CmnInfoInit_Debug(pDM_Odm);
	odm_DIGInit(pDM_Odm);
	odm_NHMCounterStatisticsInit(pDM_Odm);
	odm_AdaptivityInit(pDM_Odm);
	odm_RateAdaptiveMaskInit(pDM_Odm);
	ODM_CfoTrackingInit(pDM_Odm);
	ODM_EdcaTurboInit(pDM_Odm);
	odm_RSSIMonitorInit(pDM_Odm);
	odm_TXPowerTrackingInit(pDM_Odm);

	ODM_ClearTxPowerTrackingState(pDM_Odm);

	odm_DynamicBBPowerSavingInit(pDM_Odm);
	odm_DynamicTxPowerInit(pDM_Odm);

	odm_SwAntDetectInit(pDM_Odm);
}

 
 
 
 
 
void ODM_DMWatchdog(struct dm_odm_t *pDM_Odm)
{
	odm_CommonInfoSelfUpdate(pDM_Odm);
	odm_BasicDbgMessage(pDM_Odm);
	odm_FalseAlarmCounterStatistics(pDM_Odm);
	odm_NHMCounterStatistics(pDM_Odm);

	odm_RSSIMonitorCheck(pDM_Odm);

	 
	 
	 
	 
	if ((adapter_to_pwrctl(pDM_Odm->Adapter)->pwr_mode != PS_MODE_ACTIVE)  
		 
		 
		 
		 
	) {
			odm_DIGbyRSSI_LPS(pDM_Odm);
	} else
		odm_DIG(pDM_Odm);

	{
		struct dig_t *pDM_DigTable = &pDM_Odm->DM_DigTable;

		odm_Adaptivity(pDM_Odm, pDM_DigTable->CurIGValue);
	}
	odm_CCKPacketDetectionThresh(pDM_Odm);

	if (*(pDM_Odm->pbPowerSaving) == true)
		return;


	odm_RefreshRateAdaptiveMask(pDM_Odm);
	odm_EdcaTurboCheck(pDM_Odm);
	ODM_CfoTracking(pDM_Odm);

	ODM_TXPowerTrackingCheck(pDM_Odm);

	 

	 
	 
	pDM_Odm->PhyDbgInfo.NumQryBeaconPkt = 0;
}


 
 
 
void ODM_CmnInfoInit(struct dm_odm_t *pDM_Odm, enum odm_cmninfo_e CmnInfo, u32 Value)
{
	 
	 
	 
	switch (CmnInfo) {
	 
	 
	 
	case ODM_CMNINFO_ABILITY:
		pDM_Odm->SupportAbility = (u32)Value;
		break;

	case ODM_CMNINFO_PLATFORM:
		pDM_Odm->SupportPlatform = (u8)Value;
		break;

	case ODM_CMNINFO_INTERFACE:
		pDM_Odm->SupportInterface = (u8)Value;
		break;

	case ODM_CMNINFO_IC_TYPE:
		pDM_Odm->SupportICType = Value;
		break;

	case ODM_CMNINFO_CUT_VER:
		pDM_Odm->CutVersion = (u8)Value;
		break;

	case ODM_CMNINFO_FAB_VER:
		pDM_Odm->FabVersion = (u8)Value;
		break;

	case ODM_CMNINFO_RFE_TYPE:
		pDM_Odm->RFEType = (u8)Value;
		break;

	case    ODM_CMNINFO_RF_ANTENNA_TYPE:
		pDM_Odm->AntDivType = (u8)Value;
		break;

	case ODM_CMNINFO_PACKAGE_TYPE:
		pDM_Odm->PackageType = (u8)Value;
		break;

	case ODM_CMNINFO_EXT_LNA:
		pDM_Odm->ExtLNA = (u8)Value;
		break;

	case ODM_CMNINFO_EXT_PA:
		pDM_Odm->ExtPA = (u8)Value;
		break;

	case ODM_CMNINFO_GPA:
		pDM_Odm->TypeGPA = (enum odm_type_gpa_e)Value;
		break;
	case ODM_CMNINFO_APA:
		pDM_Odm->TypeAPA = (enum odm_type_apa_e)Value;
		break;
	case ODM_CMNINFO_GLNA:
		pDM_Odm->TypeGLNA = (enum odm_type_glna_e)Value;
		break;
	case ODM_CMNINFO_ALNA:
		pDM_Odm->TypeALNA = (enum odm_type_alna_e)Value;
		break;

	case ODM_CMNINFO_EXT_TRSW:
		pDM_Odm->ExtTRSW = (u8)Value;
		break;
	case ODM_CMNINFO_PATCH_ID:
		pDM_Odm->PatchID = (u8)Value;
		break;
	case ODM_CMNINFO_BINHCT_TEST:
		pDM_Odm->bInHctTest = (bool)Value;
		break;
	case ODM_CMNINFO_BWIFI_TEST:
		pDM_Odm->bWIFITest = (bool)Value;
		break;

	case ODM_CMNINFO_SMART_CONCURRENT:
		pDM_Odm->bDualMacSmartConcurrent = (bool)Value;
		break;

	 
	default:
		 
		break;
	}

}


void ODM_CmnInfoHook(struct dm_odm_t *pDM_Odm, enum odm_cmninfo_e CmnInfo, void *pValue)
{
	 
	 
	 
	switch (CmnInfo) {
	 
	 
	 
	case ODM_CMNINFO_MAC_PHY_MODE:
		pDM_Odm->pMacPhyMode = pValue;
		break;

	case ODM_CMNINFO_TX_UNI:
		pDM_Odm->pNumTxBytesUnicast = pValue;
		break;

	case ODM_CMNINFO_RX_UNI:
		pDM_Odm->pNumRxBytesUnicast = pValue;
		break;

	case ODM_CMNINFO_WM_MODE:
		pDM_Odm->pwirelessmode = pValue;
		break;

	case ODM_CMNINFO_SEC_CHNL_OFFSET:
		pDM_Odm->pSecChOffset = pValue;
		break;

	case ODM_CMNINFO_SEC_MODE:
		pDM_Odm->pSecurity = pValue;
		break;

	case ODM_CMNINFO_BW:
		pDM_Odm->pBandWidth = pValue;
		break;

	case ODM_CMNINFO_CHNL:
		pDM_Odm->pChannel = pValue;
		break;

	case ODM_CMNINFO_DMSP_GET_VALUE:
		pDM_Odm->pbGetValueFromOtherMac = pValue;
		break;

	case ODM_CMNINFO_BUDDY_ADAPTOR:
		pDM_Odm->pBuddyAdapter = pValue;
		break;

	case ODM_CMNINFO_DMSP_IS_MASTER:
		pDM_Odm->pbMasterOfDMSP = pValue;
		break;

	case ODM_CMNINFO_SCAN:
		pDM_Odm->pbScanInProcess = pValue;
		break;

	case ODM_CMNINFO_POWER_SAVING:
		pDM_Odm->pbPowerSaving = pValue;
		break;

	case ODM_CMNINFO_ONE_PATH_CCA:
		pDM_Odm->pOnePathCCA = pValue;
		break;

	case ODM_CMNINFO_DRV_STOP:
		pDM_Odm->pbDriverStopped =  pValue;
		break;

	case ODM_CMNINFO_PNP_IN:
		pDM_Odm->pbDriverIsGoingToPnpSetPowerSleep =  pValue;
		break;

	case ODM_CMNINFO_INIT_ON:
		pDM_Odm->pinit_adpt_in_progress =  pValue;
		break;

	case ODM_CMNINFO_ANT_TEST:
		pDM_Odm->pAntennaTest =  pValue;
		break;

	case ODM_CMNINFO_NET_CLOSED:
		pDM_Odm->pbNet_closed = pValue;
		break;

	case ODM_CMNINFO_FORCED_RATE:
		pDM_Odm->pForcedDataRate = pValue;
		break;

	case ODM_CMNINFO_FORCED_IGI_LB:
		pDM_Odm->pu1ForcedIgiLb = pValue;
		break;

	case ODM_CMNINFO_MP_MODE:
		pDM_Odm->mp_mode = pValue;
		break;

	 
	 
	 

	 
	 

	 
	 
	 

	 
	 
	 

	 
	 
	 
	 
	default:
		 
		break;
	}

}


void ODM_CmnInfoPtrArrayHook(
	struct dm_odm_t *pDM_Odm,
	enum odm_cmninfo_e CmnInfo,
	u16 Index,
	void *pValue
)
{
	 
	 
	 
	switch (CmnInfo) {
	 
	 
	 
	case ODM_CMNINFO_STA_STATUS:
		pDM_Odm->pODM_StaInfo[Index] = (PSTA_INFO_T)pValue;
		break;
	 
	default:
		 
		break;
	}

}


 
 
 
void ODM_CmnInfoUpdate(struct dm_odm_t *pDM_Odm, u32 CmnInfo, u64 Value)
{
	 
	 
	 
	switch (CmnInfo) {
	case ODM_CMNINFO_LINK_IN_PROGRESS:
		pDM_Odm->bLinkInProcess = (bool)Value;
		break;

	case ODM_CMNINFO_ABILITY:
		pDM_Odm->SupportAbility = (u32)Value;
		break;

	case ODM_CMNINFO_WIFI_DIRECT:
		pDM_Odm->bWIFI_Direct = (bool)Value;
		break;

	case ODM_CMNINFO_WIFI_DISPLAY:
		pDM_Odm->bWIFI_Display = (bool)Value;
		break;

	case ODM_CMNINFO_LINK:
		pDM_Odm->bLinked = (bool)Value;
		break;

	case ODM_CMNINFO_STATION_STATE:
		pDM_Odm->bsta_state = (bool)Value;
		break;

	case ODM_CMNINFO_RSSI_MIN:
		pDM_Odm->RSSI_Min = (u8)Value;
		break;

	case ODM_CMNINFO_RA_THRESHOLD_HIGH:
		pDM_Odm->RateAdaptive.HighRSSIThresh = (u8)Value;
		break;

	case ODM_CMNINFO_RA_THRESHOLD_LOW:
		pDM_Odm->RateAdaptive.LowRSSIThresh = (u8)Value;
		break;
	 
	case ODM_CMNINFO_BT_ENABLED:
		pDM_Odm->bBtEnabled = (bool)Value;
		break;

	case ODM_CMNINFO_BT_HS_CONNECT_PROCESS:
		pDM_Odm->bBtConnectProcess = (bool)Value;
		break;

	case ODM_CMNINFO_BT_HS_RSSI:
		pDM_Odm->btHsRssi = (u8)Value;
		break;

	case ODM_CMNINFO_BT_OPERATION:
		pDM_Odm->bBtHsOperation = (bool)Value;
		break;

	case ODM_CMNINFO_BT_LIMITED_DIG:
		pDM_Odm->bBtLimitedDig = (bool)Value;
		break;

	case ODM_CMNINFO_BT_DISABLE_EDCA:
		pDM_Odm->bBtDisableEdcaTurbo = (bool)Value;
		break;

 
	default:
		 
		break;
	}


}

 
 
 
 

 

 

 
 
 

 

 
 
 

 

