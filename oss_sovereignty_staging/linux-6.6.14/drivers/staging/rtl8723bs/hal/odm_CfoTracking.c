
 

#include "odm_precomp.h"

static void odm_SetCrystalCap(void *pDM_VOID, u8 CrystalCap)
{
	struct dm_odm_t *pDM_Odm = (struct dm_odm_t *)pDM_VOID;
	struct cfo_tracking *pCfoTrack = &pDM_Odm->DM_CfoTrack;

	if (pCfoTrack->CrystalCap == CrystalCap)
		return;

	pCfoTrack->CrystalCap = CrystalCap;

	 
	CrystalCap = CrystalCap & 0x3F;
	PHY_SetBBReg(
		pDM_Odm->Adapter,
		REG_MAC_PHY_CTRL,
		0x00FFF000,
		(CrystalCap | (CrystalCap << 6))
	);
}

static u8 odm_GetDefaultCrytaltalCap(void *pDM_VOID)
{
	struct dm_odm_t *pDM_Odm = (struct dm_odm_t *)pDM_VOID;

	struct adapter *Adapter = pDM_Odm->Adapter;
	struct hal_com_data *pHalData = GET_HAL_DATA(Adapter);

	return pHalData->CrystalCap & 0x3f;
}

static void odm_SetATCStatus(void *pDM_VOID, bool ATCStatus)
{
	struct dm_odm_t *pDM_Odm = (struct dm_odm_t *)pDM_VOID;
	struct cfo_tracking *pCfoTrack = &pDM_Odm->DM_CfoTrack;

	if (pCfoTrack->bATCStatus == ATCStatus)
		return;

	PHY_SetBBReg(
		pDM_Odm->Adapter,
		ODM_REG(BB_ATC, pDM_Odm),
		ODM_BIT(BB_ATC, pDM_Odm),
		ATCStatus
	);
	pCfoTrack->bATCStatus = ATCStatus;
}

static bool odm_GetATCStatus(void *pDM_VOID)
{
	bool ATCStatus;
	struct dm_odm_t *pDM_Odm = (struct dm_odm_t *)pDM_VOID;

	ATCStatus = (bool)PHY_QueryBBReg(
		pDM_Odm->Adapter,
		ODM_REG(BB_ATC, pDM_Odm),
		ODM_BIT(BB_ATC, pDM_Odm)
	);
	return ATCStatus;
}

void ODM_CfoTrackingReset(void *pDM_VOID)
{
	struct dm_odm_t *pDM_Odm = (struct dm_odm_t *)pDM_VOID;
	struct cfo_tracking *pCfoTrack = &pDM_Odm->DM_CfoTrack;

	pCfoTrack->DefXCap = odm_GetDefaultCrytaltalCap(pDM_Odm);
	pCfoTrack->bAdjust = true;

	odm_SetCrystalCap(pDM_Odm, pCfoTrack->DefXCap);
	odm_SetATCStatus(pDM_Odm, true);
}

void ODM_CfoTrackingInit(void *pDM_VOID)
{
	struct dm_odm_t *pDM_Odm = (struct dm_odm_t *)pDM_VOID;
	struct cfo_tracking *pCfoTrack = &pDM_Odm->DM_CfoTrack;

	pCfoTrack->DefXCap =
		pCfoTrack->CrystalCap = odm_GetDefaultCrytaltalCap(pDM_Odm);
	pCfoTrack->bATCStatus = odm_GetATCStatus(pDM_Odm);
	pCfoTrack->bAdjust = true;
}

void ODM_CfoTracking(void *pDM_VOID)
{
	struct dm_odm_t *pDM_Odm = (struct dm_odm_t *)pDM_VOID;
	struct cfo_tracking *pCfoTrack = &pDM_Odm->DM_CfoTrack;
	int CFO_kHz_A, CFO_ave = 0;
	int CFO_ave_diff;
	int CrystalCap = (int)pCfoTrack->CrystalCap;
	u8 Adjust_Xtal = 1;

	 
	if (!(pDM_Odm->SupportAbility & ODM_BB_CFO_TRACKING)) {
		return;
	}

	if (!pDM_Odm->bLinked || !pDM_Odm->bOneEntryOnly) {
		 
		ODM_CfoTrackingReset(pDM_Odm);
	} else {
		 
		 
		if (pCfoTrack->packetCount == pCfoTrack->packetCount_pre) {
			return;
		}
		pCfoTrack->packetCount_pre = pCfoTrack->packetCount;

		 
		CFO_kHz_A =  (int)(pCfoTrack->CFO_tail[0] * 3125)  / 1280;

		CFO_ave = CFO_kHz_A;

		 
		CFO_ave_diff =
			(pCfoTrack->CFO_ave_pre >= CFO_ave) ?
			(pCfoTrack->CFO_ave_pre-CFO_ave) :
			(CFO_ave-pCfoTrack->CFO_ave_pre);

		if (
			CFO_ave_diff > 20 &&
			pCfoTrack->largeCFOHit == 0 &&
			!pCfoTrack->bAdjust
		) {
			pCfoTrack->largeCFOHit = 1;
			return;
		} else
			pCfoTrack->largeCFOHit = 0;
		pCfoTrack->CFO_ave_pre = CFO_ave;

		 
		if (pCfoTrack->bAdjust == false) {
			if (CFO_ave > CFO_TH_XTAL_HIGH || CFO_ave < (-CFO_TH_XTAL_HIGH))
				pCfoTrack->bAdjust = true;
		} else {
			if (CFO_ave < CFO_TH_XTAL_LOW && CFO_ave > (-CFO_TH_XTAL_LOW))
				pCfoTrack->bAdjust = false;
		}

		 
		if (pDM_Odm->bBtEnabled) {
			pCfoTrack->bAdjust = false;
			odm_SetCrystalCap(pDM_Odm, pCfoTrack->DefXCap);
		}

		 
		if (pCfoTrack->bAdjust) {
			if (CFO_ave > CFO_TH_XTAL_LOW)
				Adjust_Xtal = Adjust_Xtal+((CFO_ave-CFO_TH_XTAL_LOW)>>2);
			else if (CFO_ave < (-CFO_TH_XTAL_LOW))
				Adjust_Xtal = Adjust_Xtal+((CFO_TH_XTAL_LOW-CFO_ave)>>2);
		}

		 
		if (pCfoTrack->bAdjust) {
			if (CFO_ave > CFO_TH_XTAL_LOW)
				CrystalCap = CrystalCap + Adjust_Xtal;
			else if (CFO_ave < (-CFO_TH_XTAL_LOW))
				CrystalCap = CrystalCap - Adjust_Xtal;

			if (CrystalCap > 0x3f)
				CrystalCap = 0x3f;
			else if (CrystalCap < 0)
				CrystalCap = 0;

			odm_SetCrystalCap(pDM_Odm, (u8)CrystalCap);
		}

		 
		if (CFO_ave < CFO_TH_ATC && CFO_ave > -CFO_TH_ATC) {
			odm_SetATCStatus(pDM_Odm, false);
		} else {
			odm_SetATCStatus(pDM_Odm, true);
		}
	}
}

void odm_parsing_cfo(void *dm_void, void *pkt_info_void, s8 *cfotail)
{
	struct dm_odm_t *dm_odm = (struct dm_odm_t *)dm_void;
	struct odm_packet_info *pkt_info = pkt_info_void;
	struct cfo_tracking *cfo_track = &dm_odm->DM_CfoTrack;
	u8 i;

	if (!(dm_odm->SupportAbility & ODM_BB_CFO_TRACKING))
		return;

	if (pkt_info->station_id != 0) {
		 
		for (i = RF_PATH_A; i <= RF_PATH_B; i++)
			cfo_track->CFO_tail[i] = (int)cfotail[i];

		 
		if (cfo_track->packetCount == 0xffffffff)
			cfo_track->packetCount = 0;
		else
			cfo_track->packetCount++;
	}
}
