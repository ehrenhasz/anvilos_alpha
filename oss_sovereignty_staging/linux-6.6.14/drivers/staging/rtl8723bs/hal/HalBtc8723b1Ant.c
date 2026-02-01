
 

#include "Mp_Precomp.h"

 
static struct coex_dm_8723b_1ant GLCoexDm8723b1Ant;
static struct coex_dm_8723b_1ant *pCoexDm = &GLCoexDm8723b1Ant;
static struct coex_sta_8723b_1ant GLCoexSta8723b1Ant;
static struct coex_sta_8723b_1ant *pCoexSta = &GLCoexSta8723b1Ant;

 
 
static u8 halbtc8723b1ant_BtRssiState(
	u8 levelNum, u8 rssiThresh, u8 rssiThresh1
)
{
	s32 btRssi = 0;
	u8 btRssiState = pCoexSta->preBtRssiState;

	btRssi = pCoexSta->btRssi;

	if (levelNum == 2) {
		if (
			(pCoexSta->preBtRssiState == BTC_RSSI_STATE_LOW) ||
			(pCoexSta->preBtRssiState == BTC_RSSI_STATE_STAY_LOW)
		) {
			if (btRssi >= (rssiThresh + BTC_RSSI_COEX_THRESH_TOL_8723B_1ANT))

				btRssiState = BTC_RSSI_STATE_HIGH;
			else
				btRssiState = BTC_RSSI_STATE_STAY_LOW;
		} else {
			if (btRssi < rssiThresh)
				btRssiState = BTC_RSSI_STATE_LOW;
			else
				btRssiState = BTC_RSSI_STATE_STAY_HIGH;
		}
	} else if (levelNum == 3) {
		if (rssiThresh > rssiThresh1)
			return pCoexSta->preBtRssiState;

		if (
			(pCoexSta->preBtRssiState == BTC_RSSI_STATE_LOW) ||
			(pCoexSta->preBtRssiState == BTC_RSSI_STATE_STAY_LOW)
		) {
			if (btRssi >= (rssiThresh + BTC_RSSI_COEX_THRESH_TOL_8723B_1ANT))
				btRssiState = BTC_RSSI_STATE_MEDIUM;
			else
				btRssiState = BTC_RSSI_STATE_STAY_LOW;
		} else if (
			(pCoexSta->preBtRssiState == BTC_RSSI_STATE_MEDIUM) ||
			(pCoexSta->preBtRssiState == BTC_RSSI_STATE_STAY_MEDIUM)
		) {
			if (btRssi >= (rssiThresh1 + BTC_RSSI_COEX_THRESH_TOL_8723B_1ANT))
				btRssiState = BTC_RSSI_STATE_HIGH;
			else if (btRssi < rssiThresh)
				btRssiState = BTC_RSSI_STATE_LOW;
			else
				btRssiState = BTC_RSSI_STATE_STAY_MEDIUM;
		} else {
			if (btRssi < rssiThresh1)
				btRssiState = BTC_RSSI_STATE_MEDIUM;
			else
				btRssiState = BTC_RSSI_STATE_STAY_HIGH;
		}
	}

	pCoexSta->preBtRssiState = btRssiState;

	return btRssiState;
}

static void halbtc8723b1ant_UpdateRaMask(
	struct btc_coexist *pBtCoexist, bool bForceExec, u32 disRateMask
)
{
	pCoexDm->curRaMask = disRateMask;

	if (bForceExec || (pCoexDm->preRaMask != pCoexDm->curRaMask))
		pBtCoexist->fBtcSet(
			pBtCoexist,
			BTC_SET_ACT_UPDATE_RAMASK,
			&pCoexDm->curRaMask
		);
	pCoexDm->preRaMask = pCoexDm->curRaMask;
}

static void halbtc8723b1ant_AutoRateFallbackRetry(
	struct btc_coexist *pBtCoexist, bool bForceExec, u8 type
)
{
	bool bWifiUnderBMode = false;

	pCoexDm->curArfrType = type;

	if (bForceExec || (pCoexDm->preArfrType != pCoexDm->curArfrType)) {
		switch (pCoexDm->curArfrType) {
		case 0:	 
			pBtCoexist->fBtcWrite4Byte(
				pBtCoexist, 0x430, pCoexDm->backupArfrCnt1
			);
			pBtCoexist->fBtcWrite4Byte(
				pBtCoexist, 0x434, pCoexDm->backupArfrCnt2
			);
			break;
		case 1:
			pBtCoexist->fBtcGet(
				pBtCoexist, BTC_GET_BL_WIFI_UNDER_B_MODE, &bWifiUnderBMode
			);
			if (bWifiUnderBMode) {
				pBtCoexist->fBtcWrite4Byte(pBtCoexist, 0x430, 0x0);
				pBtCoexist->fBtcWrite4Byte(pBtCoexist, 0x434, 0x01010101);
			} else {
				pBtCoexist->fBtcWrite4Byte(pBtCoexist, 0x430, 0x0);
				pBtCoexist->fBtcWrite4Byte(pBtCoexist, 0x434, 0x04030201);
			}
			break;
		default:
			break;
		}
	}

	pCoexDm->preArfrType = pCoexDm->curArfrType;
}

static void halbtc8723b1ant_RetryLimit(
	struct btc_coexist *pBtCoexist, bool bForceExec, u8 type
)
{
	pCoexDm->curRetryLimitType = type;

	if (
		bForceExec ||
		(pCoexDm->preRetryLimitType != pCoexDm->curRetryLimitType)
	) {
		switch (pCoexDm->curRetryLimitType) {
		case 0:	 
			pBtCoexist->fBtcWrite2Byte(
				pBtCoexist, 0x42a, pCoexDm->backupRetryLimit
			);
			break;
		case 1:	 
			pBtCoexist->fBtcWrite2Byte(pBtCoexist, 0x42a, 0x0808);
			break;
		default:
			break;
		}
	}

	pCoexDm->preRetryLimitType = pCoexDm->curRetryLimitType;
}

static void halbtc8723b1ant_AmpduMaxTime(
	struct btc_coexist *pBtCoexist, bool bForceExec, u8 type
)
{
	pCoexDm->curAmpduTimeType = type;

	if (
		bForceExec || (pCoexDm->preAmpduTimeType != pCoexDm->curAmpduTimeType)
	) {
		switch (pCoexDm->curAmpduTimeType) {
		case 0:	 
			pBtCoexist->fBtcWrite1Byte(
				pBtCoexist, 0x456, pCoexDm->backupAmpduMaxTime
			);
			break;
		case 1:	 
			pBtCoexist->fBtcWrite1Byte(pBtCoexist, 0x456, 0x38);
			break;
		default:
			break;
		}
	}

	pCoexDm->preAmpduTimeType = pCoexDm->curAmpduTimeType;
}

static void halbtc8723b1ant_LimitedTx(
	struct btc_coexist *pBtCoexist,
	bool bForceExec,
	u8 raMaskType,
	u8 arfrType,
	u8 retryLimitType,
	u8 ampduTimeType
)
{
	switch (raMaskType) {
	case 0:	 
		halbtc8723b1ant_UpdateRaMask(pBtCoexist, bForceExec, 0x0);
		break;
	case 1:	 
		halbtc8723b1ant_UpdateRaMask(pBtCoexist, bForceExec, 0x00000003);
		break;
	case 2:	 
		halbtc8723b1ant_UpdateRaMask(pBtCoexist, bForceExec, 0x0001f1f7);
		break;
	default:
		break;
	}

	halbtc8723b1ant_AutoRateFallbackRetry(pBtCoexist, bForceExec, arfrType);
	halbtc8723b1ant_RetryLimit(pBtCoexist, bForceExec, retryLimitType);
	halbtc8723b1ant_AmpduMaxTime(pBtCoexist, bForceExec, ampduTimeType);
}

static void halbtc8723b1ant_LimitedRx(
	struct btc_coexist *pBtCoexist,
	bool bForceExec,
	bool bRejApAggPkt,
	bool bBtCtrlAggBufSize,
	u8 aggBufSize
)
{
	bool bRejectRxAgg = bRejApAggPkt;
	bool bBtCtrlRxAggSize = bBtCtrlAggBufSize;
	u8 rxAggSize = aggBufSize;

	 
	 
	 
	pBtCoexist->fBtcSet(
		pBtCoexist, BTC_SET_BL_TO_REJ_AP_AGG_PKT, &bRejectRxAgg
	);
	 
	pBtCoexist->fBtcSet(
		pBtCoexist, BTC_SET_BL_BT_CTRL_AGG_SIZE, &bBtCtrlRxAggSize
	);
	 
	pBtCoexist->fBtcSet(pBtCoexist, BTC_SET_U1_AGG_BUF_SIZE, &rxAggSize);
	 
	pBtCoexist->fBtcSet(pBtCoexist, BTC_SET_ACT_AGGREGATE_CTRL, NULL);


}

static void halbtc8723b1ant_QueryBtInfo(struct btc_coexist *pBtCoexist)
{
	u8 H2C_Parameter[1] = {0};

	pCoexSta->bC2hBtInfoReqSent = true;

	H2C_Parameter[0] |= BIT0;	 

	pBtCoexist->fBtcFillH2c(pBtCoexist, 0x61, 1, H2C_Parameter);
}

static void halbtc8723b1ant_MonitorBtCtr(struct btc_coexist *pBtCoexist)
{
	u32 regHPTxRx, regLPTxRx, u4Tmp;
	u32 regHPTx = 0, regHPRx = 0, regLPTx = 0, regLPRx = 0;
	static u8 NumOfBtCounterChk;

        
	 

	if (pCoexSta->bUnderIps) {
		pCoexSta->highPriorityTx = 65535;
		pCoexSta->highPriorityRx = 65535;
		pCoexSta->lowPriorityTx = 65535;
		pCoexSta->lowPriorityRx = 65535;
		return;
	}

	regHPTxRx = 0x770;
	regLPTxRx = 0x774;

	u4Tmp = pBtCoexist->fBtcRead4Byte(pBtCoexist, regHPTxRx);
	regHPTx = u4Tmp & bMaskLWord;
	regHPRx = (u4Tmp & bMaskHWord) >> 16;

	u4Tmp = pBtCoexist->fBtcRead4Byte(pBtCoexist, regLPTxRx);
	regLPTx = u4Tmp & bMaskLWord;
	regLPRx = (u4Tmp & bMaskHWord) >> 16;

	pCoexSta->highPriorityTx = regHPTx;
	pCoexSta->highPriorityRx = regHPRx;
	pCoexSta->lowPriorityTx = regLPTx;
	pCoexSta->lowPriorityRx = regLPRx;

	if ((pCoexSta->lowPriorityTx >= 1050) && (!pCoexSta->bC2hBtInquiryPage))
		pCoexSta->popEventCnt++;

	 
	pBtCoexist->fBtcWrite1Byte(pBtCoexist, 0x76e, 0xc);

	if ((regHPTx == 0) && (regHPRx == 0) && (regLPTx == 0) && (regLPRx == 0)) {
		NumOfBtCounterChk++;
		if (NumOfBtCounterChk >= 3) {
			halbtc8723b1ant_QueryBtInfo(pBtCoexist);
			NumOfBtCounterChk = 0;
		}
	}
}


static void halbtc8723b1ant_MonitorWiFiCtr(struct btc_coexist *pBtCoexist)
{
	s32	wifiRssi = 0;
	bool bWifiBusy = false, bWifiUnderBMode = false;
	static u8 nCCKLockCounter;

	pBtCoexist->fBtcGet(pBtCoexist, BTC_GET_BL_WIFI_BUSY, &bWifiBusy);
	pBtCoexist->fBtcGet(pBtCoexist, BTC_GET_S4_WIFI_RSSI, &wifiRssi);
	pBtCoexist->fBtcGet(
		pBtCoexist, BTC_GET_BL_WIFI_UNDER_B_MODE, &bWifiUnderBMode
	);

	if (pCoexSta->bUnderIps) {
		pCoexSta->nCRCOK_CCK = 0;
		pCoexSta->nCRCOK_11g = 0;
		pCoexSta->nCRCOK_11n = 0;
		pCoexSta->nCRCOK_11nAgg = 0;

		pCoexSta->nCRCErr_CCK = 0;
		pCoexSta->nCRCErr_11g = 0;
		pCoexSta->nCRCErr_11n = 0;
		pCoexSta->nCRCErr_11nAgg = 0;
	} else {
		pCoexSta->nCRCOK_CCK	= pBtCoexist->fBtcRead4Byte(pBtCoexist, 0xf88);
		pCoexSta->nCRCOK_11g	= pBtCoexist->fBtcRead2Byte(pBtCoexist, 0xf94);
		pCoexSta->nCRCOK_11n	= pBtCoexist->fBtcRead2Byte(pBtCoexist, 0xf90);
		pCoexSta->nCRCOK_11nAgg = pBtCoexist->fBtcRead2Byte(pBtCoexist, 0xfb8);

		pCoexSta->nCRCErr_CCK	 = pBtCoexist->fBtcRead4Byte(pBtCoexist, 0xf84);
		pCoexSta->nCRCErr_11g	 = pBtCoexist->fBtcRead2Byte(pBtCoexist, 0xf96);
		pCoexSta->nCRCErr_11n	 = pBtCoexist->fBtcRead2Byte(pBtCoexist, 0xf92);
		pCoexSta->nCRCErr_11nAgg = pBtCoexist->fBtcRead2Byte(pBtCoexist, 0xfba);
	}


	 
	pBtCoexist->fBtcWrite1ByteBitMask(pBtCoexist, 0xf16, 0x1, 0x1);
	pBtCoexist->fBtcWrite1ByteBitMask(pBtCoexist, 0xf16, 0x1, 0x0);

	if (bWifiBusy && (wifiRssi >= 30) && !bWifiUnderBMode) {
		if (
			(pCoexDm->btStatus == BT_8723B_1ANT_BT_STATUS_ACL_BUSY) ||
			(pCoexDm->btStatus == BT_8723B_1ANT_BT_STATUS_ACL_SCO_BUSY) ||
			(pCoexDm->btStatus == BT_8723B_1ANT_BT_STATUS_SCO_BUSY)
		) {
			if (
				pCoexSta->nCRCOK_CCK > (
					pCoexSta->nCRCOK_11g +
					pCoexSta->nCRCOK_11n +
					pCoexSta->nCRCOK_11nAgg
				)
			) {
				if (nCCKLockCounter < 5)
					nCCKLockCounter++;
			} else {
				if (nCCKLockCounter > 0)
					nCCKLockCounter--;
			}

		} else {
			if (nCCKLockCounter > 0)
				nCCKLockCounter--;
		}
	} else {
		if (nCCKLockCounter > 0)
			nCCKLockCounter--;
	}

	if (!pCoexSta->bPreCCKLock) {

		if (nCCKLockCounter >= 5)
			pCoexSta->bCCKLock = true;
		else
			pCoexSta->bCCKLock = false;
	} else {
		if (nCCKLockCounter == 0)
			pCoexSta->bCCKLock = false;
		else
			pCoexSta->bCCKLock = true;
	}

	pCoexSta->bPreCCKLock =  pCoexSta->bCCKLock;


}

static bool halbtc8723b1ant_IsWifiStatusChanged(struct btc_coexist *pBtCoexist)
{
	static bool	bPreWifiBusy, bPreUnder4way, bPreBtHsOn;
	bool bWifiBusy = false, bUnder4way = false, bBtHsOn = false;
	bool bWifiConnected = false;

	pBtCoexist->fBtcGet(
		pBtCoexist, BTC_GET_BL_WIFI_CONNECTED, &bWifiConnected
	);
	pBtCoexist->fBtcGet(pBtCoexist, BTC_GET_BL_WIFI_BUSY, &bWifiBusy);
	pBtCoexist->fBtcGet(pBtCoexist, BTC_GET_BL_HS_OPERATION, &bBtHsOn);
	pBtCoexist->fBtcGet(
		pBtCoexist, BTC_GET_BL_WIFI_4_WAY_PROGRESS, &bUnder4way
	);

	if (bWifiConnected) {
		if (bWifiBusy != bPreWifiBusy) {
			bPreWifiBusy = bWifiBusy;
			return true;
		}

		if (bUnder4way != bPreUnder4way) {
			bPreUnder4way = bUnder4way;
			return true;
		}

		if (bBtHsOn != bPreBtHsOn) {
			bPreBtHsOn = bBtHsOn;
			return true;
		}
	}

	return false;
}

static void halbtc8723b1ant_UpdateBtLinkInfo(struct btc_coexist *pBtCoexist)
{
	struct btc_bt_link_info *pBtLinkInfo = &pBtCoexist->btLinkInfo;
	bool bBtHsOn = false;

	pBtCoexist->fBtcGet(pBtCoexist, BTC_GET_BL_HS_OPERATION, &bBtHsOn);

	pBtLinkInfo->bBtLinkExist = pCoexSta->bBtLinkExist;
	pBtLinkInfo->bScoExist = pCoexSta->bScoExist;
	pBtLinkInfo->bA2dpExist = pCoexSta->bA2dpExist;
	pBtLinkInfo->bPanExist = pCoexSta->bPanExist;
	pBtLinkInfo->bHidExist = pCoexSta->bHidExist;

	 
	if (bBtHsOn) {
		pBtLinkInfo->bPanExist = true;
		pBtLinkInfo->bBtLinkExist = true;
	}

	 
	if (
		pBtLinkInfo->bScoExist &&
		!pBtLinkInfo->bA2dpExist &&
		!pBtLinkInfo->bPanExist &&
		!pBtLinkInfo->bHidExist
	)
		pBtLinkInfo->bScoOnly = true;
	else
		pBtLinkInfo->bScoOnly = false;

	 
	if (
		!pBtLinkInfo->bScoExist &&
		pBtLinkInfo->bA2dpExist &&
		!pBtLinkInfo->bPanExist &&
		!pBtLinkInfo->bHidExist
	)
		pBtLinkInfo->bA2dpOnly = true;
	else
		pBtLinkInfo->bA2dpOnly = false;

	 
	if (
		!pBtLinkInfo->bScoExist &&
		!pBtLinkInfo->bA2dpExist &&
		pBtLinkInfo->bPanExist &&
		!pBtLinkInfo->bHidExist
	)
		pBtLinkInfo->bPanOnly = true;
	else
		pBtLinkInfo->bPanOnly = false;

	 
	if (
		!pBtLinkInfo->bScoExist &&
		!pBtLinkInfo->bA2dpExist &&
		!pBtLinkInfo->bPanExist &&
		pBtLinkInfo->bHidExist
	)
		pBtLinkInfo->bHidOnly = true;
	else
		pBtLinkInfo->bHidOnly = false;
}

static u8 halbtc8723b1ant_ActionAlgorithm(struct btc_coexist *pBtCoexist)
{
	struct btc_bt_link_info *pBtLinkInfo = &pBtCoexist->btLinkInfo;
	bool bBtHsOn = false;
	u8 algorithm = BT_8723B_1ANT_COEX_ALGO_UNDEFINED;
	u8 numOfDiffProfile = 0;

	pBtCoexist->fBtcGet(pBtCoexist, BTC_GET_BL_HS_OPERATION, &bBtHsOn);

	if (!pBtLinkInfo->bBtLinkExist)
		return algorithm;

	if (pBtLinkInfo->bScoExist)
		numOfDiffProfile++;
	if (pBtLinkInfo->bHidExist)
		numOfDiffProfile++;
	if (pBtLinkInfo->bPanExist)
		numOfDiffProfile++;
	if (pBtLinkInfo->bA2dpExist)
		numOfDiffProfile++;

	if (numOfDiffProfile == 1) {
		if (pBtLinkInfo->bScoExist) {
			algorithm = BT_8723B_1ANT_COEX_ALGO_SCO;
		} else {
			if (pBtLinkInfo->bHidExist) {
				algorithm = BT_8723B_1ANT_COEX_ALGO_HID;
			} else if (pBtLinkInfo->bA2dpExist) {
				algorithm = BT_8723B_1ANT_COEX_ALGO_A2DP;
			} else if (pBtLinkInfo->bPanExist) {
				if (bBtHsOn)
					algorithm = BT_8723B_1ANT_COEX_ALGO_PANHS;
				else
					algorithm = BT_8723B_1ANT_COEX_ALGO_PANEDR;
			}
		}
	} else if (numOfDiffProfile == 2) {
		if (pBtLinkInfo->bScoExist) {
			if (pBtLinkInfo->bHidExist) {
				algorithm = BT_8723B_1ANT_COEX_ALGO_HID;
			} else if (pBtLinkInfo->bA2dpExist) {
				algorithm = BT_8723B_1ANT_COEX_ALGO_SCO;
			} else if (pBtLinkInfo->bPanExist) {
				if (bBtHsOn)
					algorithm = BT_8723B_1ANT_COEX_ALGO_SCO;
				else
					algorithm = BT_8723B_1ANT_COEX_ALGO_PANEDR_HID;
			}
		} else {
			if (pBtLinkInfo->bHidExist && pBtLinkInfo->bA2dpExist) {
				algorithm = BT_8723B_1ANT_COEX_ALGO_HID_A2DP;
			} else if (pBtLinkInfo->bHidExist && pBtLinkInfo->bPanExist) {
				if (bBtHsOn)
					algorithm = BT_8723B_1ANT_COEX_ALGO_HID_A2DP;
				else
					algorithm = BT_8723B_1ANT_COEX_ALGO_PANEDR_HID;
			} else if (pBtLinkInfo->bPanExist && pBtLinkInfo->bA2dpExist) {
				if (bBtHsOn)
					algorithm = BT_8723B_1ANT_COEX_ALGO_A2DP_PANHS;
				else
					algorithm = BT_8723B_1ANT_COEX_ALGO_PANEDR_A2DP;
			}
		}
	} else if (numOfDiffProfile == 3) {
		if (pBtLinkInfo->bScoExist) {
			if (pBtLinkInfo->bHidExist && pBtLinkInfo->bA2dpExist) {
				algorithm = BT_8723B_1ANT_COEX_ALGO_HID;
			} else if (
				pBtLinkInfo->bHidExist && pBtLinkInfo->bPanExist
			) {
				if (bBtHsOn)
					algorithm = BT_8723B_1ANT_COEX_ALGO_HID_A2DP;
				else
					algorithm = BT_8723B_1ANT_COEX_ALGO_PANEDR_HID;
			} else if (pBtLinkInfo->bPanExist && pBtLinkInfo->bA2dpExist) {
				if (bBtHsOn)
					algorithm = BT_8723B_1ANT_COEX_ALGO_SCO;
				else
					algorithm = BT_8723B_1ANT_COEX_ALGO_PANEDR_HID;
			}
		} else {
			if (
				pBtLinkInfo->bHidExist &&
				pBtLinkInfo->bPanExist &&
				pBtLinkInfo->bA2dpExist
			) {
				if (bBtHsOn)
					algorithm = BT_8723B_1ANT_COEX_ALGO_HID_A2DP;
				else
					algorithm = BT_8723B_1ANT_COEX_ALGO_HID_A2DP_PANEDR;
			}
		}
	} else if (numOfDiffProfile >= 3) {
		if (pBtLinkInfo->bScoExist) {
			if (
				pBtLinkInfo->bHidExist &&
				pBtLinkInfo->bPanExist &&
				pBtLinkInfo->bA2dpExist
			) {
				if (!bBtHsOn)
					algorithm = BT_8723B_1ANT_COEX_ALGO_PANEDR_HID;

			}
		}
	}

	return algorithm;
}

static void halbtc8723b1ant_SetSwPenaltyTxRateAdaptive(
	struct btc_coexist *pBtCoexist, bool bLowPenaltyRa
)
{
	u8 H2C_Parameter[6] = {0};

	H2C_Parameter[0] = 0x6;	 

	if (bLowPenaltyRa) {
		H2C_Parameter[1] |= BIT0;
		H2C_Parameter[2] = 0x00;   
		H2C_Parameter[3] = 0xf7;   
		H2C_Parameter[4] = 0xf8;   
		H2C_Parameter[5] = 0xf9;	 
	}

	pBtCoexist->fBtcFillH2c(pBtCoexist, 0x69, 6, H2C_Parameter);
}

static void halbtc8723b1ant_LowPenaltyRa(
	struct btc_coexist *pBtCoexist, bool bForceExec, bool bLowPenaltyRa
)
{
	pCoexDm->bCurLowPenaltyRa = bLowPenaltyRa;

	if (!bForceExec) {
		if (pCoexDm->bPreLowPenaltyRa == pCoexDm->bCurLowPenaltyRa)
			return;
	}
	halbtc8723b1ant_SetSwPenaltyTxRateAdaptive(
		pBtCoexist, pCoexDm->bCurLowPenaltyRa
	);

	pCoexDm->bPreLowPenaltyRa = pCoexDm->bCurLowPenaltyRa;
}

static void halbtc8723b1ant_SetCoexTable(
	struct btc_coexist *pBtCoexist,
	u32 val0x6c0,
	u32 val0x6c4,
	u32 val0x6c8,
	u8 val0x6cc
)
{
	pBtCoexist->fBtcWrite4Byte(pBtCoexist, 0x6c0, val0x6c0);

	pBtCoexist->fBtcWrite4Byte(pBtCoexist, 0x6c4, val0x6c4);

	pBtCoexist->fBtcWrite4Byte(pBtCoexist, 0x6c8, val0x6c8);

	pBtCoexist->fBtcWrite1Byte(pBtCoexist, 0x6cc, val0x6cc);
}

static void halbtc8723b1ant_CoexTable(
	struct btc_coexist *pBtCoexist,
	bool bForceExec,
	u32 val0x6c0,
	u32 val0x6c4,
	u32 val0x6c8,
	u8 val0x6cc
)
{
	pCoexDm->curVal0x6c0 = val0x6c0;
	pCoexDm->curVal0x6c4 = val0x6c4;
	pCoexDm->curVal0x6c8 = val0x6c8;
	pCoexDm->curVal0x6cc = val0x6cc;

	if (!bForceExec) {
		if (
			(pCoexDm->preVal0x6c0 == pCoexDm->curVal0x6c0) &&
		    (pCoexDm->preVal0x6c4 == pCoexDm->curVal0x6c4) &&
		    (pCoexDm->preVal0x6c8 == pCoexDm->curVal0x6c8) &&
		    (pCoexDm->preVal0x6cc == pCoexDm->curVal0x6cc)
		)
			return;
	}

	halbtc8723b1ant_SetCoexTable(
		pBtCoexist, val0x6c0, val0x6c4, val0x6c8, val0x6cc
	);

	pCoexDm->preVal0x6c0 = pCoexDm->curVal0x6c0;
	pCoexDm->preVal0x6c4 = pCoexDm->curVal0x6c4;
	pCoexDm->preVal0x6c8 = pCoexDm->curVal0x6c8;
	pCoexDm->preVal0x6cc = pCoexDm->curVal0x6cc;
}

static void halbtc8723b1ant_CoexTableWithType(
	struct btc_coexist *pBtCoexist, bool bForceExec, u8 type
)
{
	pCoexSta->nCoexTableType = type;

	switch (type) {
	case 0:
		halbtc8723b1ant_CoexTable(
			pBtCoexist, bForceExec, 0x55555555, 0x55555555, 0xffffff, 0x3
		);
		break;
	case 1:
		halbtc8723b1ant_CoexTable(
			pBtCoexist, bForceExec, 0x55555555, 0x5a5a5a5a, 0xffffff, 0x3
		);
		break;
	case 2:
		halbtc8723b1ant_CoexTable(
			pBtCoexist, bForceExec, 0x5a5a5a5a, 0x5a5a5a5a, 0xffffff, 0x3
		);
		break;
	case 3:
		halbtc8723b1ant_CoexTable(
			pBtCoexist, bForceExec, 0xaaaa5555, 0xaaaa5a5a, 0xffffff, 0x3
		);
		break;
	case 4:
		halbtc8723b1ant_CoexTable(
			pBtCoexist, bForceExec, 0x55555555, 0xaaaa5a5a, 0xffffff, 0x3
		);
		break;
	case 5:
		halbtc8723b1ant_CoexTable(
			pBtCoexist, bForceExec, 0x5a5a5a5a, 0xaaaa5a5a, 0xffffff, 0x3
		);
		break;
	case 6:
		halbtc8723b1ant_CoexTable(
			pBtCoexist, bForceExec, 0x55555555, 0xaaaaaaaa, 0xffffff, 0x3
		);
		break;
	case 7:
		halbtc8723b1ant_CoexTable(
			pBtCoexist, bForceExec, 0xaaaaaaaa, 0xaaaaaaaa, 0xffffff, 0x3
		);
		break;
	default:
		break;
	}
}

static void halbtc8723b1ant_SetFwIgnoreWlanAct(
	struct btc_coexist *pBtCoexist, bool bEnable
)
{
	u8 H2C_Parameter[1] = {0};

	if (bEnable)
		H2C_Parameter[0] |= BIT0;  

	pBtCoexist->fBtcFillH2c(pBtCoexist, 0x63, 1, H2C_Parameter);
}

static void halbtc8723b1ant_IgnoreWlanAct(
	struct btc_coexist *pBtCoexist, bool bForceExec, bool bEnable
)
{
	pCoexDm->bCurIgnoreWlanAct = bEnable;

	if (!bForceExec) {
		if (pCoexDm->bPreIgnoreWlanAct == pCoexDm->bCurIgnoreWlanAct)
			return;
	}
	halbtc8723b1ant_SetFwIgnoreWlanAct(pBtCoexist, bEnable);

	pCoexDm->bPreIgnoreWlanAct = pCoexDm->bCurIgnoreWlanAct;
}

static void halbtc8723b1ant_SetLpsRpwm(
	struct btc_coexist *pBtCoexist, u8 lpsVal, u8 rpwmVal
)
{
	u8 lps = lpsVal;
	u8 rpwm = rpwmVal;

	pBtCoexist->fBtcSet(pBtCoexist, BTC_SET_U1_LPS_VAL, &lps);
	pBtCoexist->fBtcSet(pBtCoexist, BTC_SET_U1_RPWM_VAL, &rpwm);
}

static void halbtc8723b1ant_LpsRpwm(
	struct btc_coexist *pBtCoexist, bool bForceExec, u8 lpsVal, u8 rpwmVal
)
{
	pCoexDm->curLps = lpsVal;
	pCoexDm->curRpwm = rpwmVal;

	if (!bForceExec) {
		if (
			(pCoexDm->preLps == pCoexDm->curLps) &&
			(pCoexDm->preRpwm == pCoexDm->curRpwm)
		) {
			return;
		}
	}
	halbtc8723b1ant_SetLpsRpwm(pBtCoexist, lpsVal, rpwmVal);

	pCoexDm->preLps = pCoexDm->curLps;
	pCoexDm->preRpwm = pCoexDm->curRpwm;
}

static void halbtc8723b1ant_SwMechanism(
	struct btc_coexist *pBtCoexist, bool bLowPenaltyRA
)
{
	halbtc8723b1ant_LowPenaltyRa(pBtCoexist, NORMAL_EXEC, bLowPenaltyRA);
}

static void halbtc8723b1ant_SetAntPath(
	struct btc_coexist *pBtCoexist, u8 antPosType, bool bInitHwCfg, bool bWifiOff
)
{
	struct btc_board_info *pBoardInfo = &pBtCoexist->boardInfo;
	u32 fwVer = 0, u4Tmp = 0, cntBtCalChk = 0;
	bool bPgExtSwitch = false;
	bool bUseExtSwitch = false;
	bool bIsInMpMode = false;
	u8 H2C_Parameter[2] = {0}, u1Tmp = 0;

	pBtCoexist->fBtcGet(pBtCoexist, BTC_GET_BL_EXT_SWITCH, &bPgExtSwitch);
	pBtCoexist->fBtcGet(pBtCoexist, BTC_GET_U4_WIFI_FW_VER, &fwVer);  

	if ((fwVer > 0 && fwVer < 0xc0000) || bPgExtSwitch)
		bUseExtSwitch = true;

	if (bInitHwCfg) {
		pBtCoexist->fBtcSetRfReg(pBtCoexist, BTC_RF_A, 0x1, 0xfffff, 0x780);  
		pBtCoexist->fBtcSetBtReg(pBtCoexist, BTC_BT_REG_RF, 0x3c, 0x15);  

		if (fwVer >= 0x180000) {
			 
			H2C_Parameter[0] = 1;
			pBtCoexist->fBtcFillH2c(pBtCoexist, 0x6E, 1, H2C_Parameter);
		} else  
			pBtCoexist->fBtcWrite1Byte(pBtCoexist, 0x765, 0x18);

		 
		pBtCoexist->fBtcWrite1Byte(pBtCoexist, 0x76e, 0x4);

		pBtCoexist->fBtcWrite1ByteBitMask(pBtCoexist, 0x67, 0x20, 0x1);  

		pBtCoexist->fBtcWrite1ByteBitMask(pBtCoexist, 0x39, 0x8, 0x1);
		pBtCoexist->fBtcWrite1Byte(pBtCoexist, 0x974, 0xff);
		pBtCoexist->fBtcWrite1ByteBitMask(pBtCoexist, 0x944, 0x3, 0x3);
		pBtCoexist->fBtcWrite1Byte(pBtCoexist, 0x930, 0x77);
	} else if (bWifiOff) {
		if (fwVer >= 0x180000) {
			 
			H2C_Parameter[0] = 1;
			pBtCoexist->fBtcFillH2c(pBtCoexist, 0x6E, 1, H2C_Parameter);
		} else  
			pBtCoexist->fBtcWrite1Byte(pBtCoexist, 0x765, 0x18);

		 
		pBtCoexist->fBtcWrite1Byte(pBtCoexist, 0x76e, 0x4);

		pBtCoexist->fBtcGet(pBtCoexist, BTC_GET_BL_WIFI_IS_IN_MP_MODE, &bIsInMpMode);
		if (!bIsInMpMode)
			pBtCoexist->fBtcWrite1ByteBitMask(pBtCoexist, 0x67, 0x20, 0x0);  
		else
			pBtCoexist->fBtcWrite1ByteBitMask(pBtCoexist, 0x67, 0x20, 0x1);  

		 
		u4Tmp = pBtCoexist->fBtcRead4Byte(pBtCoexist, 0x4c);
		u4Tmp &= ~BIT23;
		u4Tmp &= ~BIT24;
		pBtCoexist->fBtcWrite4Byte(pBtCoexist, 0x4c, u4Tmp);
	} else {
		 
		if (fwVer >= 0x180000) {
			if (pBtCoexist->fBtcRead1Byte(pBtCoexist, 0x765) != 0) {
				H2C_Parameter[0] = 0;
				pBtCoexist->fBtcFillH2c(pBtCoexist, 0x6E, 1, H2C_Parameter);
			}
		} else {
			 
			while (cntBtCalChk <= 20) {
				u1Tmp = pBtCoexist->fBtcRead1Byte(pBtCoexist, 0x49d);
				cntBtCalChk++;

				if (u1Tmp & BIT0)
					mdelay(50);
				else
					break;
			}

			 
			pBtCoexist->fBtcWrite1Byte(pBtCoexist, 0x765, 0x0);
		}

		if (pBtCoexist->fBtcRead1Byte(pBtCoexist, 0x76e) != 0xc)
			 
			pBtCoexist->fBtcWrite1Byte(pBtCoexist, 0x76e, 0xc);
	}

	if (bUseExtSwitch) {
		if (bInitHwCfg) {
			 
			u4Tmp = pBtCoexist->fBtcRead4Byte(pBtCoexist, 0x4c);
			u4Tmp &= ~BIT23;
			u4Tmp |= BIT24;
			pBtCoexist->fBtcWrite4Byte(pBtCoexist, 0x4c, u4Tmp);

			pBtCoexist->fBtcWrite4Byte(pBtCoexist, 0x948, 0x0);  

			if (pBoardInfo->btdmAntPos == BTC_ANTENNA_AT_MAIN_PORT) {
				 
				H2C_Parameter[0] = 0;
				H2C_Parameter[1] = 1;   
				pBtCoexist->fBtcFillH2c(pBtCoexist, 0x65, 2, H2C_Parameter);
			} else {
				 
				H2C_Parameter[0] = 1;
				H2C_Parameter[1] = 1;   
				pBtCoexist->fBtcFillH2c(pBtCoexist, 0x65, 2, H2C_Parameter);
			}
		}


		 
		switch (antPosType) {
		case BTC_ANT_PATH_WIFI:
			if (pBoardInfo->btdmAntPos == BTC_ANTENNA_AT_MAIN_PORT)
				pBtCoexist->fBtcWrite1ByteBitMask(pBtCoexist, 0x92c, 0x3, 0x1);
			else
				pBtCoexist->fBtcWrite1ByteBitMask(pBtCoexist, 0x92c, 0x3, 0x2);
			break;
		case BTC_ANT_PATH_BT:
			if (pBoardInfo->btdmAntPos == BTC_ANTENNA_AT_MAIN_PORT)
				pBtCoexist->fBtcWrite1ByteBitMask(pBtCoexist, 0x92c, 0x3, 0x2);
			else
				pBtCoexist->fBtcWrite1ByteBitMask(pBtCoexist, 0x92c, 0x3, 0x1);
			break;
		default:
		case BTC_ANT_PATH_PTA:
			if (pBoardInfo->btdmAntPos == BTC_ANTENNA_AT_MAIN_PORT)
				pBtCoexist->fBtcWrite1ByteBitMask(pBtCoexist, 0x92c, 0x3, 0x1);
			else
				pBtCoexist->fBtcWrite1ByteBitMask(pBtCoexist, 0x92c, 0x3, 0x2);
			break;
		}

	} else {
		if (bInitHwCfg) {
			 
			u4Tmp = pBtCoexist->fBtcRead4Byte(pBtCoexist, 0x4c);
			u4Tmp |= BIT23;
			u4Tmp &= ~BIT24;
			pBtCoexist->fBtcWrite4Byte(pBtCoexist, 0x4c, u4Tmp);

			 
			pBtCoexist->fBtcWrite1ByteBitMask(pBtCoexist, 0x64, 0x1, 0x0);

			if (pBoardInfo->btdmAntPos == BTC_ANTENNA_AT_MAIN_PORT) {

				 
				H2C_Parameter[0] = 0;
				H2C_Parameter[1] = 0;   
				pBtCoexist->fBtcFillH2c(pBtCoexist, 0x65, 2, H2C_Parameter);
			} else {

				 
				H2C_Parameter[0] = 1;
				H2C_Parameter[1] = 0;   
				pBtCoexist->fBtcFillH2c(pBtCoexist, 0x65, 2, H2C_Parameter);
			}
		}


		 
		switch (antPosType) {
		case BTC_ANT_PATH_WIFI:
			if (pBoardInfo->btdmAntPos == BTC_ANTENNA_AT_MAIN_PORT)
				pBtCoexist->fBtcWrite4Byte(pBtCoexist, 0x948, 0x0);
			else
				pBtCoexist->fBtcWrite4Byte(pBtCoexist, 0x948, 0x280);
			break;
		case BTC_ANT_PATH_BT:
			if (pBoardInfo->btdmAntPos == BTC_ANTENNA_AT_MAIN_PORT)
				pBtCoexist->fBtcWrite4Byte(pBtCoexist, 0x948, 0x280);
			else
				pBtCoexist->fBtcWrite4Byte(pBtCoexist, 0x948, 0x0);
			break;
		default:
		case BTC_ANT_PATH_PTA:
			if (pBoardInfo->btdmAntPos == BTC_ANTENNA_AT_MAIN_PORT)
				pBtCoexist->fBtcWrite4Byte(pBtCoexist, 0x948, 0x200);
			else
				pBtCoexist->fBtcWrite4Byte(pBtCoexist, 0x948, 0x80);
			break;
		}
	}
}

static void halbtc8723b1ant_SetFwPstdma(
	struct btc_coexist *pBtCoexist, u8 byte1, u8 byte2, u8 byte3, u8 byte4, u8 byte5
)
{
	u8 H2C_Parameter[5] = {0};
	u8 realByte1 = byte1, realByte5 = byte5;
	bool bApEnable = false;

	pBtCoexist->fBtcGet(pBtCoexist, BTC_GET_BL_WIFI_AP_MODE_ENABLE, &bApEnable);

	if (bApEnable) {
		if (byte1 & BIT4 && !(byte1 & BIT5)) {
			realByte1 &= ~BIT4;
			realByte1 |= BIT5;

			realByte5 |= BIT5;
			realByte5 &= ~BIT6;
		}
	}

	H2C_Parameter[0] = realByte1;
	H2C_Parameter[1] = byte2;
	H2C_Parameter[2] = byte3;
	H2C_Parameter[3] = byte4;
	H2C_Parameter[4] = realByte5;

	pCoexDm->psTdmaPara[0] = realByte1;
	pCoexDm->psTdmaPara[1] = byte2;
	pCoexDm->psTdmaPara[2] = byte3;
	pCoexDm->psTdmaPara[3] = byte4;
	pCoexDm->psTdmaPara[4] = realByte5;

	pBtCoexist->fBtcFillH2c(pBtCoexist, 0x60, 5, H2C_Parameter);
}


static void halbtc8723b1ant_PsTdma(
	struct btc_coexist *pBtCoexist, bool bForceExec, bool bTurnOn, u8 type
)
{
	struct btc_bt_link_info *pBtLinkInfo = &pBtCoexist->btLinkInfo;
	bool bWifiBusy = false;
	u8 rssiAdjustVal = 0;
	u8 psTdmaByte4Val = 0x50, psTdmaByte0Val = 0x51, psTdmaByte3Val =  0x10;
	s8 nWiFiDurationAdjust = 0x0;
	 

	pCoexDm->bCurPsTdmaOn = bTurnOn;
	pCoexDm->curPsTdma = type;

	pBtCoexist->fBtcGet(pBtCoexist, BTC_GET_BL_WIFI_BUSY, &bWifiBusy);

	if (!bForceExec) {
		if (
			(pCoexDm->bPrePsTdmaOn == pCoexDm->bCurPsTdmaOn) &&
			(pCoexDm->prePsTdma == pCoexDm->curPsTdma)
		)
			return;
	}

	if (pCoexSta->nScanAPNum <= 5)
		nWiFiDurationAdjust = 5;
	else if  (pCoexSta->nScanAPNum >= 40)
		nWiFiDurationAdjust = -15;
	else if  (pCoexSta->nScanAPNum >= 20)
		nWiFiDurationAdjust = -10;

	if (!pCoexSta->bForceLpsOn) {  
		psTdmaByte0Val = 0x61;   
		psTdmaByte3Val = 0x11;  
		psTdmaByte4Val = 0x10;  
	}


	if (bTurnOn) {
		if (pBtLinkInfo->bSlaveRole)
			psTdmaByte4Val = psTdmaByte4Val | 0x1;   


		switch (type) {
		default:
			halbtc8723b1ant_SetFwPstdma(
				pBtCoexist, 0x51, 0x1a, 0x1a, 0x0, psTdmaByte4Val
			);
			break;
		case 1:
			halbtc8723b1ant_SetFwPstdma(
				pBtCoexist,
				psTdmaByte0Val,
				0x3a + nWiFiDurationAdjust,
				0x03,
				psTdmaByte3Val,
				psTdmaByte4Val
			);
			break;
		case 2:
			halbtc8723b1ant_SetFwPstdma(
				pBtCoexist,
				psTdmaByte0Val,
				0x2d + nWiFiDurationAdjust,
				0x03,
				psTdmaByte3Val,
				psTdmaByte4Val
			);
			break;
		case 3:
			halbtc8723b1ant_SetFwPstdma(
				pBtCoexist, 0x51, 0x1d, 0x1d, 0x0, 0x10
			);
			break;
		case 4:
			halbtc8723b1ant_SetFwPstdma(
				pBtCoexist, 0x93, 0x15, 0x3, 0x14, 0x0
			);
			break;
		case 5:
			halbtc8723b1ant_SetFwPstdma(
				pBtCoexist, 0x61, 0x15, 0x3, 0x11, 0x10
			);
			break;
		case 6:
			halbtc8723b1ant_SetFwPstdma(
				pBtCoexist, 0x61, 0x20, 0x3, 0x11, 0x11
			);
			break;
		case 7:
			halbtc8723b1ant_SetFwPstdma(pBtCoexist, 0x13, 0xc, 0x5, 0x0, 0x0);
			break;
		case 8:
			halbtc8723b1ant_SetFwPstdma(
				pBtCoexist, 0x93, 0x25, 0x3, 0x10, 0x0
			);
			break;
		case 9:
			halbtc8723b1ant_SetFwPstdma(
				pBtCoexist,
				psTdmaByte0Val,
				0x21,
				0x3,
				psTdmaByte3Val,
				psTdmaByte4Val
			);
			break;
		case 10:
			halbtc8723b1ant_SetFwPstdma(pBtCoexist, 0x13, 0xa, 0xa, 0x0, 0x40);
			break;
		case 11:
			halbtc8723b1ant_SetFwPstdma(
				pBtCoexist,
				psTdmaByte0Val,
				0x21,
				0x03,
				psTdmaByte3Val,
				psTdmaByte4Val
			);
			break;
		case 12:
			halbtc8723b1ant_SetFwPstdma(
				pBtCoexist, 0x51, 0x0a, 0x0a, 0x0, 0x50
			);
			break;
		case 13:
			halbtc8723b1ant_SetFwPstdma(
				pBtCoexist, 0x51, 0x12, 0x12, 0x0, 0x10
			);
			break;
		case 14:
			halbtc8723b1ant_SetFwPstdma(
				pBtCoexist, 0x51, 0x21, 0x3, 0x10, psTdmaByte4Val
			);
			break;
		case 15:
			halbtc8723b1ant_SetFwPstdma(
				pBtCoexist, 0x13, 0xa, 0x3, 0x8, 0x0
			);
			break;
		case 16:
			halbtc8723b1ant_SetFwPstdma(
				pBtCoexist, 0x93, 0x15, 0x3, 0x10, 0x0
			);
			break;
		case 18:
			halbtc8723b1ant_SetFwPstdma(
				pBtCoexist, 0x93, 0x25, 0x3, 0x10, 0x0
			);
			break;
		case 20:
			halbtc8723b1ant_SetFwPstdma(
				pBtCoexist, 0x61, 0x3f, 0x03, 0x11, 0x10

			);
			break;
		case 21:
			halbtc8723b1ant_SetFwPstdma(
				pBtCoexist, 0x61, 0x25, 0x03, 0x11, 0x11
			);
			break;
		case 22:
			halbtc8723b1ant_SetFwPstdma(
				pBtCoexist, 0x61, 0x25, 0x03, 0x11, 0x10
			);
			break;
		case 23:
			halbtc8723b1ant_SetFwPstdma(
				pBtCoexist, 0xe3, 0x25, 0x3, 0x31, 0x18
			);
			break;
		case 24:
			halbtc8723b1ant_SetFwPstdma(
				pBtCoexist, 0xe3, 0x15, 0x3, 0x31, 0x18
			);
			break;
		case 25:
			halbtc8723b1ant_SetFwPstdma(
				pBtCoexist, 0xe3, 0xa, 0x3, 0x31, 0x18
			);
			break;
		case 26:
			halbtc8723b1ant_SetFwPstdma(
				pBtCoexist, 0xe3, 0xa, 0x3, 0x31, 0x18
			);
			break;
		case 27:
			halbtc8723b1ant_SetFwPstdma(
				pBtCoexist, 0xe3, 0x25, 0x3, 0x31, 0x98
			);
			break;
		case 28:
			halbtc8723b1ant_SetFwPstdma(
				pBtCoexist, 0x69, 0x25, 0x3, 0x31, 0x0
			);
			break;
		case 29:
			halbtc8723b1ant_SetFwPstdma(
				pBtCoexist, 0xab, 0x1a, 0x1a, 0x1, 0x10
			);
			break;
		case 30:
			halbtc8723b1ant_SetFwPstdma(
				pBtCoexist, 0x51, 0x30, 0x3, 0x10, 0x10
			);
			break;
		case 31:
			halbtc8723b1ant_SetFwPstdma(
				pBtCoexist, 0xd3, 0x1a, 0x1a, 0x0, 0x58
			);
			break;
		case 32:
			halbtc8723b1ant_SetFwPstdma(
				pBtCoexist, 0x61, 0x35, 0x3, 0x11, 0x11
			);
			break;
		case 33:
			halbtc8723b1ant_SetFwPstdma(
				pBtCoexist, 0xa3, 0x25, 0x3, 0x30, 0x90
			);
			break;
		case 34:
			halbtc8723b1ant_SetFwPstdma(
				pBtCoexist, 0x53, 0x1a, 0x1a, 0x0, 0x10
			);
			break;
		case 35:
			halbtc8723b1ant_SetFwPstdma(
				pBtCoexist, 0x63, 0x1a, 0x1a, 0x0, 0x10
			);
			break;
		case 36:
			halbtc8723b1ant_SetFwPstdma(
				pBtCoexist, 0xd3, 0x12, 0x3, 0x14, 0x50
			);
			break;
		case 40:  
			 
			halbtc8723b1ant_SetFwPstdma(
				pBtCoexist, 0x23, 0x18, 0x00, 0x10, 0x24
			);
			break;
		}
	} else {

		 
		switch (type) {
		case 8:  
			halbtc8723b1ant_SetFwPstdma(pBtCoexist, 0x8, 0x0, 0x0, 0x0, 0x0);
			halbtc8723b1ant_SetAntPath(
				pBtCoexist, BTC_ANT_PATH_PTA, false, false
			);
			break;
		case 0:
		default:   
			halbtc8723b1ant_SetFwPstdma(pBtCoexist, 0x0, 0x0, 0x0, 0x0, 0x0);
			halbtc8723b1ant_SetAntPath(
				pBtCoexist, BTC_ANT_PATH_BT, false, false
			);
			break;
		case 9:    
			halbtc8723b1ant_SetFwPstdma(pBtCoexist, 0x0, 0x0, 0x0, 0x0, 0x0);
			halbtc8723b1ant_SetAntPath(
				pBtCoexist, BTC_ANT_PATH_WIFI, false, false
			);
			break;
		}
	}

	rssiAdjustVal = 0;
	pBtCoexist->fBtcSet(
		pBtCoexist, BTC_SET_U1_RSSI_ADJ_VAL_FOR_1ANT_COEX_TYPE, &rssiAdjustVal
	);

	 
	pCoexDm->bPrePsTdmaOn = pCoexDm->bCurPsTdmaOn;
	pCoexDm->prePsTdma = pCoexDm->curPsTdma;
}

static bool halbtc8723b1ant_IsCommonAction(struct btc_coexist *pBtCoexist)
{
	bool bCommon = false, bWifiConnected = false, bWifiBusy = false;

	pBtCoexist->fBtcGet(pBtCoexist, BTC_GET_BL_WIFI_CONNECTED, &bWifiConnected);
	pBtCoexist->fBtcGet(pBtCoexist, BTC_GET_BL_WIFI_BUSY, &bWifiBusy);

	if (
		!bWifiConnected &&
		pCoexDm->btStatus == BT_8723B_1ANT_BT_STATUS_NON_CONNECTED_IDLE
	) {
		 

		bCommon = true;
	} else if (
		bWifiConnected &&
		(pCoexDm->btStatus == BT_8723B_1ANT_BT_STATUS_NON_CONNECTED_IDLE)
	) {
		 

		bCommon = true;
	} else if (
		!bWifiConnected &&
		(pCoexDm->btStatus == BT_8723B_1ANT_BT_STATUS_CONNECTED_IDLE)
	) {
		 

		bCommon = true;
	} else if (
		bWifiConnected &&
		(pCoexDm->btStatus == BT_8723B_1ANT_BT_STATUS_CONNECTED_IDLE)
	) {
		 

		bCommon = true;
	} else if (
		!bWifiConnected &&
		(pCoexDm->btStatus != BT_8723B_1ANT_BT_STATUS_CONNECTED_IDLE)
	) {
		 

		bCommon = true;
	} else {
		bCommon = false;
	}

	return bCommon;
}


static void halbtc8723b1ant_TdmaDurationAdjustForAcl(
	struct btc_coexist *pBtCoexist, u8 wifiStatus
)
{
	static s32 up, dn, m, n, WaitCount;
	s32 result;    
	u8 retryCount = 0, btInfoExt;

	if (
		(wifiStatus == BT_8723B_1ANT_WIFI_STATUS_NON_CONNECTED_ASSO_AUTH_SCAN) ||
		(wifiStatus == BT_8723B_1ANT_WIFI_STATUS_CONNECTED_SCAN) ||
		(wifiStatus == BT_8723B_1ANT_WIFI_STATUS_CONNECTED_SPECIAL_PKT)
	) {
		if (
			pCoexDm->curPsTdma != 1 &&
			pCoexDm->curPsTdma != 2 &&
			pCoexDm->curPsTdma != 3 &&
			pCoexDm->curPsTdma != 9
		) {
			halbtc8723b1ant_PsTdma(pBtCoexist, NORMAL_EXEC, true, 9);
			pCoexDm->psTdmaDuAdjType = 9;

			up = 0;
			dn = 0;
			m = 1;
			n = 3;
			result = 0;
			WaitCount = 0;
		}
		return;
	}

	if (!pCoexDm->bAutoTdmaAdjust) {
		pCoexDm->bAutoTdmaAdjust = true;

		halbtc8723b1ant_PsTdma(pBtCoexist, NORMAL_EXEC, true, 2);
		pCoexDm->psTdmaDuAdjType = 2;
		 
		up = 0;
		dn = 0;
		m = 1;
		n = 3;
		result = 0;
		WaitCount = 0;
	} else {
		 
		retryCount = pCoexSta->btRetryCnt;
		btInfoExt = pCoexSta->btInfoExt;

		if (pCoexSta->lowPriorityTx > 1050 || pCoexSta->lowPriorityRx > 1250)
			retryCount++;

		result = 0;
		WaitCount++;

		if (retryCount == 0) {  
			up++;
			dn--;

			if (dn <= 0)
				dn = 0;

			if (up >= n) {  
				WaitCount = 0;
				n = 3;
				up = 0;
				dn = 0;
				result = 1;
			}
		} else if (retryCount <= 3) {  
			up--;
			dn++;

			if (up <= 0)
				up = 0;

			if (dn == 2) {  
				if (WaitCount <= 2)
					m++;  
				else
					m = 1;

				if (m >= 20)  
					m = 20;

				n = 3 * m;
				up = 0;
				dn = 0;
				WaitCount = 0;
				result = -1;
			}
		} else {  
			if (WaitCount == 1)
				m++;  
			else
				m = 1;

			if (m >= 20)  
				m = 20;

			n = 3 * m;
			up = 0;
			dn = 0;
			WaitCount = 0;
			result = -1;
		}

		if (result == -1) {
			if (
				BT_INFO_8723B_1ANT_A2DP_BASIC_RATE(btInfoExt) &&
				((pCoexDm->curPsTdma == 1) || (pCoexDm->curPsTdma == 2))
			) {
				halbtc8723b1ant_PsTdma(pBtCoexist, NORMAL_EXEC, true, 9);
				pCoexDm->psTdmaDuAdjType = 9;
			} else if (pCoexDm->curPsTdma == 1) {
				halbtc8723b1ant_PsTdma(pBtCoexist, NORMAL_EXEC, true, 2);
				pCoexDm->psTdmaDuAdjType = 2;
			} else if (pCoexDm->curPsTdma == 2) {
				halbtc8723b1ant_PsTdma(pBtCoexist, NORMAL_EXEC, true, 9);
				pCoexDm->psTdmaDuAdjType = 9;
			} else if (pCoexDm->curPsTdma == 9) {
				halbtc8723b1ant_PsTdma(pBtCoexist, NORMAL_EXEC, true, 11);
				pCoexDm->psTdmaDuAdjType = 11;
			}
		} else if (result == 1) {
			if (
				BT_INFO_8723B_1ANT_A2DP_BASIC_RATE(btInfoExt) &&
				((pCoexDm->curPsTdma == 1) || (pCoexDm->curPsTdma == 2))
			) {
				halbtc8723b1ant_PsTdma(pBtCoexist, NORMAL_EXEC, true, 9);
				pCoexDm->psTdmaDuAdjType = 9;
			} else if (pCoexDm->curPsTdma == 11) {
				halbtc8723b1ant_PsTdma(pBtCoexist, NORMAL_EXEC, true, 9);
				pCoexDm->psTdmaDuAdjType = 9;
			} else if (pCoexDm->curPsTdma == 9) {
				halbtc8723b1ant_PsTdma(pBtCoexist, NORMAL_EXEC, true, 2);
				pCoexDm->psTdmaDuAdjType = 2;
			} else if (pCoexDm->curPsTdma == 2) {
				halbtc8723b1ant_PsTdma(pBtCoexist, NORMAL_EXEC, true, 1);
				pCoexDm->psTdmaDuAdjType = 1;
			}
		}

		if (
			pCoexDm->curPsTdma != 1 &&
			pCoexDm->curPsTdma != 2 &&
			pCoexDm->curPsTdma != 9 &&
			pCoexDm->curPsTdma != 11
		)  
			halbtc8723b1ant_PsTdma(
				pBtCoexist, NORMAL_EXEC, true, pCoexDm->psTdmaDuAdjType
			);
	}
}

static void halbtc8723b1ant_PsTdmaCheckForPowerSaveState(
	struct btc_coexist *pBtCoexist, bool bNewPsState
)
{
	u8 lpsMode = 0x0;

	pBtCoexist->fBtcGet(pBtCoexist, BTC_GET_U1_LPS_MODE, &lpsMode);

	if (lpsMode) {	 
		if (bNewPsState) {
			 
		} else  
			halbtc8723b1ant_PsTdma(pBtCoexist, NORMAL_EXEC, false, 0);
	} else {						 
		if (bNewPsState)  
			halbtc8723b1ant_PsTdma(pBtCoexist, NORMAL_EXEC, false, 0);
		else {
			 
		}
	}
}

static void halbtc8723b1ant_PowerSaveState(
	struct btc_coexist *pBtCoexist, u8 psType, u8 lpsVal, u8 rpwmVal
)
{
	bool bLowPwrDisable = false;

	switch (psType) {
	case BTC_PS_WIFI_NATIVE:
		 
		bLowPwrDisable = false;
		pBtCoexist->fBtcSet(
			pBtCoexist, BTC_SET_ACT_DISABLE_LOW_POWER, &bLowPwrDisable
		);
		pBtCoexist->fBtcSet(pBtCoexist, BTC_SET_ACT_NORMAL_LPS, NULL);
		pCoexSta->bForceLpsOn = false;
		break;
	case BTC_PS_LPS_ON:
		halbtc8723b1ant_PsTdmaCheckForPowerSaveState(pBtCoexist, true);
		halbtc8723b1ant_LpsRpwm(pBtCoexist, NORMAL_EXEC, lpsVal, rpwmVal);
		 
		bLowPwrDisable = true;
		pBtCoexist->fBtcSet(
			pBtCoexist, BTC_SET_ACT_DISABLE_LOW_POWER, &bLowPwrDisable
		);
		 
		pBtCoexist->fBtcSet(pBtCoexist, BTC_SET_ACT_ENTER_LPS, NULL);
		pCoexSta->bForceLpsOn = true;
		break;
	case BTC_PS_LPS_OFF:
		halbtc8723b1ant_PsTdmaCheckForPowerSaveState(pBtCoexist, false);
		pBtCoexist->fBtcSet(pBtCoexist, BTC_SET_ACT_LEAVE_LPS, NULL);
		pCoexSta->bForceLpsOn = false;
		break;
	default:
		break;
	}
}

 
 
 
 
 

 
 
 
 
 
static void halbtc8723b1ant_ActionWifiMultiPort(struct btc_coexist *pBtCoexist)
{
	halbtc8723b1ant_PowerSaveState(pBtCoexist, BTC_PS_WIFI_NATIVE, 0x0, 0x0);

	halbtc8723b1ant_PsTdma(pBtCoexist, NORMAL_EXEC, false, 8);
	halbtc8723b1ant_CoexTableWithType(pBtCoexist, NORMAL_EXEC, 2);
}

static void halbtc8723b1ant_ActionHs(struct btc_coexist *pBtCoexist)
{
	halbtc8723b1ant_PsTdma(pBtCoexist, NORMAL_EXEC, true, 5);
	halbtc8723b1ant_CoexTableWithType(pBtCoexist, NORMAL_EXEC, 2);
}

static void halbtc8723b1ant_ActionBtInquiry(struct btc_coexist *pBtCoexist)
{
	struct btc_bt_link_info *pBtLinkInfo = &pBtCoexist->btLinkInfo;
	bool bWifiConnected = false;
	bool bApEnable = false;
	bool bWifiBusy = false;
	bool bBtBusy = false;

	pBtCoexist->fBtcGet(pBtCoexist, BTC_GET_BL_WIFI_AP_MODE_ENABLE, &bApEnable);
	pBtCoexist->fBtcGet(pBtCoexist, BTC_GET_BL_WIFI_CONNECTED, &bWifiConnected);
	pBtCoexist->fBtcGet(pBtCoexist, BTC_GET_BL_WIFI_BUSY, &bWifiBusy);
	pBtCoexist->fBtcSet(pBtCoexist, BTC_SET_BL_BT_TRAFFIC_BUSY, &bBtBusy);

	if (!bWifiConnected && !pCoexSta->bWiFiIsHighPriTask) {
		halbtc8723b1ant_PowerSaveState(pBtCoexist, BTC_PS_WIFI_NATIVE, 0x0, 0x0);
		halbtc8723b1ant_PsTdma(pBtCoexist, NORMAL_EXEC, false, 8);

		halbtc8723b1ant_CoexTableWithType(pBtCoexist, NORMAL_EXEC, 0);
	} else if (
		pBtLinkInfo->bScoExist ||
		pBtLinkInfo->bHidExist ||
		pBtLinkInfo->bA2dpExist
	) {
		 
		halbtc8723b1ant_PowerSaveState(pBtCoexist, BTC_PS_WIFI_NATIVE, 0x0, 0x0);
		halbtc8723b1ant_PsTdma(pBtCoexist, NORMAL_EXEC, true, 32);

		halbtc8723b1ant_CoexTableWithType(pBtCoexist, NORMAL_EXEC, 4);
	} else if (pBtLinkInfo->bPanExist || bWifiBusy) {
		halbtc8723b1ant_PowerSaveState(pBtCoexist, BTC_PS_WIFI_NATIVE, 0x0, 0x0);
		halbtc8723b1ant_PsTdma(pBtCoexist, NORMAL_EXEC, true, 20);

		halbtc8723b1ant_CoexTableWithType(pBtCoexist, NORMAL_EXEC, 4);
	} else {
		halbtc8723b1ant_PowerSaveState(pBtCoexist, BTC_PS_WIFI_NATIVE, 0x0, 0x0);
		halbtc8723b1ant_PsTdma(pBtCoexist, NORMAL_EXEC, false, 8);

		halbtc8723b1ant_CoexTableWithType(pBtCoexist, NORMAL_EXEC, 7);
	}
}

static void halbtc8723b1ant_ActionBtScoHidOnlyBusy(
	struct btc_coexist *pBtCoexist, u8 wifiStatus
)
{
	struct btc_bt_link_info *pBtLinkInfo = &pBtCoexist->btLinkInfo;
	bool bWifiConnected = false;

	pBtCoexist->fBtcGet(pBtCoexist, BTC_GET_BL_WIFI_CONNECTED, &bWifiConnected);

	 

	if (pBtLinkInfo->bScoExist) {
		halbtc8723b1ant_PsTdma(pBtCoexist, NORMAL_EXEC, true, 5);
		halbtc8723b1ant_CoexTableWithType(pBtCoexist, NORMAL_EXEC, 5);
	} else {  
		halbtc8723b1ant_PsTdma(pBtCoexist, NORMAL_EXEC, true, 6);
		halbtc8723b1ant_CoexTableWithType(pBtCoexist, NORMAL_EXEC, 5);
	}
}

static void halbtc8723b1ant_ActionWifiConnectedBtAclBusy(
	struct btc_coexist *pBtCoexist, u8 wifiStatus
)
{
	struct btc_bt_link_info *pBtLinkInfo = &pBtCoexist->btLinkInfo;

	halbtc8723b1ant_BtRssiState(2, 28, 0);

	if ((pCoexSta->lowPriorityRx >= 1000) && (pCoexSta->lowPriorityRx != 65535))
		pBtLinkInfo->bSlaveRole = true;
	else
		pBtLinkInfo->bSlaveRole = false;

	if (pBtLinkInfo->bHidOnly) {  
		halbtc8723b1ant_ActionBtScoHidOnlyBusy(pBtCoexist, wifiStatus);
		pCoexDm->bAutoTdmaAdjust = false;
		return;
	} else if (pBtLinkInfo->bA2dpOnly) {  
		if (wifiStatus == BT_8723B_1ANT_WIFI_STATUS_CONNECTED_IDLE) {
			halbtc8723b1ant_PsTdma(pBtCoexist, NORMAL_EXEC, true, 32);
			halbtc8723b1ant_CoexTableWithType(pBtCoexist, NORMAL_EXEC, 4);
			pCoexDm->bAutoTdmaAdjust = false;
		} else {
			halbtc8723b1ant_TdmaDurationAdjustForAcl(pBtCoexist, wifiStatus);
			halbtc8723b1ant_CoexTableWithType(pBtCoexist, NORMAL_EXEC, 4);
			pCoexDm->bAutoTdmaAdjust = true;
		}
	} else if (pBtLinkInfo->bHidExist && pBtLinkInfo->bA2dpExist) {  
		halbtc8723b1ant_PsTdma(pBtCoexist, NORMAL_EXEC, true, 14);
		pCoexDm->bAutoTdmaAdjust = false;

		halbtc8723b1ant_CoexTableWithType(pBtCoexist, NORMAL_EXEC, 4);
	} else if (
		pBtLinkInfo->bPanOnly ||
		(pBtLinkInfo->bHidExist && pBtLinkInfo->bPanExist)
	) {  
		halbtc8723b1ant_PsTdma(pBtCoexist, NORMAL_EXEC, true, 3);
		halbtc8723b1ant_CoexTableWithType(pBtCoexist, NORMAL_EXEC, 4);
		pCoexDm->bAutoTdmaAdjust = false;
	} else if (
		(pBtLinkInfo->bA2dpExist && pBtLinkInfo->bPanExist) ||
		(pBtLinkInfo->bHidExist && pBtLinkInfo->bA2dpExist && pBtLinkInfo->bPanExist)
	) {  
		halbtc8723b1ant_PsTdma(pBtCoexist, NORMAL_EXEC, true, 13);
		halbtc8723b1ant_CoexTableWithType(pBtCoexist, NORMAL_EXEC, 4);
		pCoexDm->bAutoTdmaAdjust = false;
	} else {
		 
		halbtc8723b1ant_PsTdma(pBtCoexist, NORMAL_EXEC, true, 32);
		halbtc8723b1ant_CoexTableWithType(pBtCoexist, NORMAL_EXEC, 4);
		pCoexDm->bAutoTdmaAdjust = false;
	}
}

static void halbtc8723b1ant_ActionWifiNotConnected(struct btc_coexist *pBtCoexist)
{
	 
	halbtc8723b1ant_PowerSaveState(pBtCoexist, BTC_PS_WIFI_NATIVE, 0x0, 0x0);

	 
	halbtc8723b1ant_PsTdma(pBtCoexist, FORCE_EXEC, false, 8);
	halbtc8723b1ant_CoexTableWithType(pBtCoexist, NORMAL_EXEC, 0);
}

static void halbtc8723b1ant_ActionWifiNotConnectedScan(
	struct btc_coexist *pBtCoexist
)
{
	struct btc_bt_link_info *pBtLinkInfo = &pBtCoexist->btLinkInfo;

	halbtc8723b1ant_PowerSaveState(pBtCoexist, BTC_PS_WIFI_NATIVE, 0x0, 0x0);

	 
	if (pCoexDm->btStatus == BT_8723B_1ANT_BT_STATUS_ACL_BUSY) {
		if (pBtLinkInfo->bA2dpExist) {
			halbtc8723b1ant_PsTdma(pBtCoexist, NORMAL_EXEC, true, 32);
			halbtc8723b1ant_CoexTableWithType(pBtCoexist, NORMAL_EXEC, 4);
		} else if (pBtLinkInfo->bA2dpExist && pBtLinkInfo->bPanExist) {
			halbtc8723b1ant_PsTdma(pBtCoexist, NORMAL_EXEC, true, 22);
			halbtc8723b1ant_CoexTableWithType(pBtCoexist, NORMAL_EXEC, 4);
		} else {
			halbtc8723b1ant_PsTdma(pBtCoexist, NORMAL_EXEC, true, 20);
			halbtc8723b1ant_CoexTableWithType(pBtCoexist, NORMAL_EXEC, 4);
		}
	} else if (
		(pCoexDm->btStatus == BT_8723B_1ANT_BT_STATUS_SCO_BUSY) ||
		(pCoexDm->btStatus == BT_8723B_1ANT_BT_STATUS_ACL_SCO_BUSY)
	) {
		halbtc8723b1ant_ActionBtScoHidOnlyBusy(
			pBtCoexist, BT_8723B_1ANT_WIFI_STATUS_CONNECTED_SCAN
		);
	} else {
		 
		halbtc8723b1ant_PsTdma(pBtCoexist, NORMAL_EXEC, false, 8);
		halbtc8723b1ant_CoexTableWithType(pBtCoexist, NORMAL_EXEC, 2);
	}
}

static void halbtc8723b1ant_ActionWifiNotConnectedAssoAuth(
	struct btc_coexist *pBtCoexist
)
{
	struct btc_bt_link_info *pBtLinkInfo = &pBtCoexist->btLinkInfo;

	halbtc8723b1ant_PowerSaveState(pBtCoexist, BTC_PS_WIFI_NATIVE, 0x0, 0x0);

	 
	if (
		(pBtLinkInfo->bScoExist) ||
		(pBtLinkInfo->bHidExist) ||
		(pBtLinkInfo->bA2dpExist)
	) {
		halbtc8723b1ant_PsTdma(pBtCoexist, NORMAL_EXEC, true, 32);
		halbtc8723b1ant_CoexTableWithType(pBtCoexist, NORMAL_EXEC, 4);
	} else if (pBtLinkInfo->bPanExist) {
		halbtc8723b1ant_PsTdma(pBtCoexist, NORMAL_EXEC, true, 20);
		halbtc8723b1ant_CoexTableWithType(pBtCoexist, NORMAL_EXEC, 4);
	} else {
		halbtc8723b1ant_PsTdma(pBtCoexist, NORMAL_EXEC, false, 8);
		halbtc8723b1ant_CoexTableWithType(pBtCoexist, NORMAL_EXEC, 2);
	}
}

static void halbtc8723b1ant_ActionWifiConnectedScan(struct btc_coexist *pBtCoexist)
{
	struct btc_bt_link_info *pBtLinkInfo = &pBtCoexist->btLinkInfo;

	halbtc8723b1ant_PowerSaveState(pBtCoexist, BTC_PS_WIFI_NATIVE, 0x0, 0x0);

	 
	if (pCoexDm->btStatus == BT_8723B_1ANT_BT_STATUS_ACL_BUSY) {
		if (pBtLinkInfo->bA2dpExist) {
			halbtc8723b1ant_PsTdma(pBtCoexist, NORMAL_EXEC, true, 32);
			halbtc8723b1ant_CoexTableWithType(pBtCoexist, NORMAL_EXEC, 4);
		} else if (pBtLinkInfo->bA2dpExist && pBtLinkInfo->bPanExist) {
			halbtc8723b1ant_PsTdma(pBtCoexist, NORMAL_EXEC, true, 22);
			halbtc8723b1ant_CoexTableWithType(pBtCoexist, NORMAL_EXEC, 4);
		} else {
			halbtc8723b1ant_PsTdma(pBtCoexist, NORMAL_EXEC, true, 20);
			halbtc8723b1ant_CoexTableWithType(pBtCoexist, NORMAL_EXEC, 4);
		}
	} else if (
		(pCoexDm->btStatus == BT_8723B_1ANT_BT_STATUS_SCO_BUSY) ||
		(pCoexDm->btStatus == BT_8723B_1ANT_BT_STATUS_ACL_SCO_BUSY)
	) {
		halbtc8723b1ant_ActionBtScoHidOnlyBusy(
			pBtCoexist, BT_8723B_1ANT_WIFI_STATUS_CONNECTED_SCAN
		);
	} else {
		 
		halbtc8723b1ant_PsTdma(pBtCoexist, NORMAL_EXEC, false, 8);
		halbtc8723b1ant_CoexTableWithType(pBtCoexist, NORMAL_EXEC, 2);
	}
}

static void halbtc8723b1ant_ActionWifiConnectedSpecialPacket(
	struct btc_coexist *pBtCoexist
)
{
	struct btc_bt_link_info *pBtLinkInfo = &pBtCoexist->btLinkInfo;

	halbtc8723b1ant_PowerSaveState(pBtCoexist, BTC_PS_WIFI_NATIVE, 0x0, 0x0);

	 
	if (
		(pBtLinkInfo->bScoExist) ||
		(pBtLinkInfo->bHidExist) ||
		(pBtLinkInfo->bA2dpExist)
	) {
		halbtc8723b1ant_PsTdma(pBtCoexist, NORMAL_EXEC, true, 32);
		halbtc8723b1ant_CoexTableWithType(pBtCoexist, NORMAL_EXEC, 4);
	} else if (pBtLinkInfo->bPanExist) {
		halbtc8723b1ant_PsTdma(pBtCoexist, NORMAL_EXEC, true, 20);
		halbtc8723b1ant_CoexTableWithType(pBtCoexist, NORMAL_EXEC, 4);
	} else {
		halbtc8723b1ant_PsTdma(pBtCoexist, NORMAL_EXEC, false, 8);
		halbtc8723b1ant_CoexTableWithType(pBtCoexist, NORMAL_EXEC, 2);
	}
}

static void halbtc8723b1ant_ActionWifiConnected(struct btc_coexist *pBtCoexist)
{
	bool bWifiBusy = false;
	bool bScan = false, bLink = false, bRoam = false;
	bool bUnder4way = false, bApEnable = false;

	pBtCoexist->fBtcGet(pBtCoexist, BTC_GET_BL_WIFI_4_WAY_PROGRESS, &bUnder4way);
	if (bUnder4way) {
		halbtc8723b1ant_ActionWifiConnectedSpecialPacket(pBtCoexist);
		return;
	}

	pBtCoexist->fBtcGet(pBtCoexist, BTC_GET_BL_WIFI_SCAN, &bScan);
	pBtCoexist->fBtcGet(pBtCoexist, BTC_GET_BL_WIFI_LINK, &bLink);
	pBtCoexist->fBtcGet(pBtCoexist, BTC_GET_BL_WIFI_ROAM, &bRoam);
	if (bScan || bLink || bRoam) {
		if (bScan)
			halbtc8723b1ant_ActionWifiConnectedScan(pBtCoexist);
		else
			halbtc8723b1ant_ActionWifiConnectedSpecialPacket(pBtCoexist);
		return;
	}

	pBtCoexist->fBtcGet(pBtCoexist, BTC_GET_BL_WIFI_AP_MODE_ENABLE, &bApEnable);
	pBtCoexist->fBtcGet(pBtCoexist, BTC_GET_BL_WIFI_BUSY, &bWifiBusy);

	 
	if (
		!bApEnable &&
		pCoexDm->btStatus == BT_8723B_1ANT_BT_STATUS_ACL_BUSY &&
		!pBtCoexist->btLinkInfo.bHidOnly
	) {
		if (pBtCoexist->btLinkInfo.bA2dpOnly) {  
			if (!bWifiBusy)
				halbtc8723b1ant_PowerSaveState(
					pBtCoexist, BTC_PS_WIFI_NATIVE, 0x0, 0x0
				);
			else {  
				if  (pCoexSta->nScanAPNum >= BT_8723B_1ANT_WIFI_NOISY_THRESH)   
					halbtc8723b1ant_PowerSaveState(
						pBtCoexist, BTC_PS_WIFI_NATIVE, 0x0, 0x0
					);
				else
					halbtc8723b1ant_PowerSaveState(
						pBtCoexist, BTC_PS_LPS_ON, 0x50, 0x4
					);
			}
		} else if (
			(!pCoexSta->bPanExist) &&
			(!pCoexSta->bA2dpExist) &&
			(!pCoexSta->bHidExist)
		)
			halbtc8723b1ant_PowerSaveState(pBtCoexist, BTC_PS_WIFI_NATIVE, 0x0, 0x0);
		else
			halbtc8723b1ant_PowerSaveState(pBtCoexist, BTC_PS_LPS_ON, 0x50, 0x4);
	} else
		halbtc8723b1ant_PowerSaveState(pBtCoexist, BTC_PS_WIFI_NATIVE, 0x0, 0x0);

	 
	if (!bWifiBusy) {
		if (pCoexDm->btStatus == BT_8723B_1ANT_BT_STATUS_ACL_BUSY) {
			halbtc8723b1ant_ActionWifiConnectedBtAclBusy(
				pBtCoexist,
				BT_8723B_1ANT_WIFI_STATUS_CONNECTED_IDLE
			);
		} else if (
			(pCoexDm->btStatus == BT_8723B_1ANT_BT_STATUS_SCO_BUSY) ||
			(pCoexDm->btStatus == BT_8723B_1ANT_BT_STATUS_ACL_SCO_BUSY)
		) {
			halbtc8723b1ant_ActionBtScoHidOnlyBusy(pBtCoexist,
				BT_8723B_1ANT_WIFI_STATUS_CONNECTED_IDLE);
		} else {
			halbtc8723b1ant_PsTdma(pBtCoexist, NORMAL_EXEC, false, 8);

			if ((pCoexSta->highPriorityTx) + (pCoexSta->highPriorityRx) <= 60)
				halbtc8723b1ant_CoexTableWithType(pBtCoexist, NORMAL_EXEC, 2);
			else
				halbtc8723b1ant_CoexTableWithType(pBtCoexist, NORMAL_EXEC, 7);
		}
	} else {
		if (pCoexDm->btStatus == BT_8723B_1ANT_BT_STATUS_ACL_BUSY) {
			halbtc8723b1ant_ActionWifiConnectedBtAclBusy(
				pBtCoexist,
				BT_8723B_1ANT_WIFI_STATUS_CONNECTED_BUSY
			);
		} else if (
			(pCoexDm->btStatus == BT_8723B_1ANT_BT_STATUS_SCO_BUSY) ||
			(pCoexDm->btStatus == BT_8723B_1ANT_BT_STATUS_ACL_SCO_BUSY)
		) {
			halbtc8723b1ant_ActionBtScoHidOnlyBusy(
				pBtCoexist,
				BT_8723B_1ANT_WIFI_STATUS_CONNECTED_BUSY
			);
		} else {
			halbtc8723b1ant_PsTdma(pBtCoexist, NORMAL_EXEC, false, 8);

			if ((pCoexSta->highPriorityTx) + (pCoexSta->highPriorityRx) <= 60)
				halbtc8723b1ant_CoexTableWithType(pBtCoexist, NORMAL_EXEC, 2);
			else
				halbtc8723b1ant_CoexTableWithType(pBtCoexist, NORMAL_EXEC, 7);
		}
	}
}

static void halbtc8723b1ant_RunSwCoexistMechanism(struct btc_coexist *pBtCoexist)
{
	u8 algorithm = 0;

	algorithm = halbtc8723b1ant_ActionAlgorithm(pBtCoexist);
	pCoexDm->curAlgorithm = algorithm;

	if (halbtc8723b1ant_IsCommonAction(pBtCoexist)) {

	} else {
		switch (pCoexDm->curAlgorithm) {
		case BT_8723B_1ANT_COEX_ALGO_SCO:
			 
			break;
		case BT_8723B_1ANT_COEX_ALGO_HID:
			 
			break;
		case BT_8723B_1ANT_COEX_ALGO_A2DP:
			 
			break;
		case BT_8723B_1ANT_COEX_ALGO_A2DP_PANHS:
			 
			break;
		case BT_8723B_1ANT_COEX_ALGO_PANEDR:
			 
			break;
		case BT_8723B_1ANT_COEX_ALGO_PANHS:
			 
			break;
		case BT_8723B_1ANT_COEX_ALGO_PANEDR_A2DP:
			 
			break;
		case BT_8723B_1ANT_COEX_ALGO_PANEDR_HID:
			 
			break;
		case BT_8723B_1ANT_COEX_ALGO_HID_A2DP_PANEDR:
			 
			break;
		case BT_8723B_1ANT_COEX_ALGO_HID_A2DP:
			 
			break;
		default:
			break;
		}
		pCoexDm->preAlgorithm = pCoexDm->curAlgorithm;
	}
}

static void halbtc8723b1ant_RunCoexistMechanism(struct btc_coexist *pBtCoexist)
{
	struct btc_bt_link_info *pBtLinkInfo = &pBtCoexist->btLinkInfo;
	bool bWifiConnected = false, bBtHsOn = false;
	bool bIncreaseScanDevNum = false;
	bool bBtCtrlAggBufSize = false;
	u8 aggBufSize = 5;
	u32 wifiLinkStatus = 0;
	u32 numOfWifiLink = 0;

	if (pBtCoexist->bManualControl)
		return;

	if (pBtCoexist->bStopCoexDm)
		return;

	if (pCoexSta->bUnderIps)
		return;

	if (
		(pCoexDm->btStatus == BT_8723B_1ANT_BT_STATUS_ACL_BUSY) ||
		(pCoexDm->btStatus == BT_8723B_1ANT_BT_STATUS_SCO_BUSY) ||
		(pCoexDm->btStatus == BT_8723B_1ANT_BT_STATUS_ACL_SCO_BUSY)
	){
		bIncreaseScanDevNum = true;
	}

	pBtCoexist->fBtcSet(
		pBtCoexist,
		BTC_SET_BL_INC_SCAN_DEV_NUM,
		&bIncreaseScanDevNum
	);
	pBtCoexist->fBtcGet(
		pBtCoexist,
		BTC_GET_BL_WIFI_CONNECTED,
		&bWifiConnected
	);

	pBtCoexist->fBtcGet(
		pBtCoexist,
		BTC_GET_U4_WIFI_LINK_STATUS,
		&wifiLinkStatus
	);
	numOfWifiLink = wifiLinkStatus >> 16;

	if ((numOfWifiLink >= 2) || (wifiLinkStatus & WIFI_P2P_GO_CONNECTED)) {
		halbtc8723b1ant_LimitedTx(pBtCoexist, NORMAL_EXEC, 0, 0, 0, 0);
		halbtc8723b1ant_LimitedRx(pBtCoexist, NORMAL_EXEC, false, bBtCtrlAggBufSize, aggBufSize);

		if ((pBtLinkInfo->bA2dpExist) && (pCoexSta->bC2hBtInquiryPage))
			halbtc8723b1ant_ActionBtInquiry(pBtCoexist);
		else
			halbtc8723b1ant_ActionWifiMultiPort(pBtCoexist);

		return;
	}

	if ((pBtLinkInfo->bBtLinkExist) && (bWifiConnected)) {
		halbtc8723b1ant_LimitedTx(pBtCoexist, NORMAL_EXEC, 1, 1, 0, 1);

		if (pBtLinkInfo->bScoExist)
			halbtc8723b1ant_LimitedRx(pBtCoexist, NORMAL_EXEC, false, true, 0x5);
		else
			halbtc8723b1ant_LimitedRx(pBtCoexist, NORMAL_EXEC, false, true, 0x8);

		halbtc8723b1ant_SwMechanism(pBtCoexist, true);
		halbtc8723b1ant_RunSwCoexistMechanism(pBtCoexist);   
	} else {
		halbtc8723b1ant_LimitedTx(pBtCoexist, NORMAL_EXEC, 0, 0, 0, 0);

		halbtc8723b1ant_LimitedRx(pBtCoexist, NORMAL_EXEC, false, false, 0x5);

		halbtc8723b1ant_SwMechanism(pBtCoexist, false);
		halbtc8723b1ant_RunSwCoexistMechanism(pBtCoexist);  
	}

	pBtCoexist->fBtcGet(pBtCoexist, BTC_GET_BL_HS_OPERATION, &bBtHsOn);
	if (pCoexSta->bC2hBtInquiryPage) {
		halbtc8723b1ant_ActionBtInquiry(pBtCoexist);
		return;
	} else if (bBtHsOn) {
		halbtc8723b1ant_ActionHs(pBtCoexist);
		return;
	}


	if (!bWifiConnected) {
		bool bScan = false, bLink = false, bRoam = false;

		pBtCoexist->fBtcGet(pBtCoexist, BTC_GET_BL_WIFI_SCAN, &bScan);
		pBtCoexist->fBtcGet(pBtCoexist, BTC_GET_BL_WIFI_LINK, &bLink);
		pBtCoexist->fBtcGet(pBtCoexist, BTC_GET_BL_WIFI_ROAM, &bRoam);

		if (bScan || bLink || bRoam) {
			if (bScan)
				halbtc8723b1ant_ActionWifiNotConnectedScan(pBtCoexist);
			else
				halbtc8723b1ant_ActionWifiNotConnectedAssoAuth(pBtCoexist);
		} else
			halbtc8723b1ant_ActionWifiNotConnected(pBtCoexist);
	} else  
		halbtc8723b1ant_ActionWifiConnected(pBtCoexist);
}

static void halbtc8723b1ant_InitCoexDm(struct btc_coexist *pBtCoexist)
{
	 

	 
	halbtc8723b1ant_SwMechanism(pBtCoexist, false);

	 
	halbtc8723b1ant_CoexTableWithType(pBtCoexist, FORCE_EXEC, 0);

	pCoexSta->popEventCnt = 0;
}

static void halbtc8723b1ant_InitHwConfig(
	struct btc_coexist *pBtCoexist,
	bool bBackUp,
	bool bWifiOnly
)
{
	pBtCoexist->fBtcWrite1ByteBitMask(pBtCoexist, 0x550, 0x8, 0x1);   

	 
	pBtCoexist->fBtcWrite1Byte(pBtCoexist, 0x790, 0x5);

	 
	pBtCoexist->fBtcWrite1Byte(pBtCoexist, 0x778, 0x1);
	pBtCoexist->fBtcWrite1ByteBitMask(pBtCoexist, 0x40, 0x20, 0x1);

	 
	if (bWifiOnly) {
		halbtc8723b1ant_SetAntPath(pBtCoexist, BTC_ANT_PATH_WIFI, true, false);
		halbtc8723b1ant_PsTdma(pBtCoexist, FORCE_EXEC, false, 9);
	} else
		halbtc8723b1ant_SetAntPath(pBtCoexist, BTC_ANT_PATH_BT, true, false);

	 
	halbtc8723b1ant_CoexTableWithType(pBtCoexist, FORCE_EXEC, 0);

	pBtCoexist->fBtcRead4Byte(pBtCoexist, 0x948);
	pBtCoexist->fBtcRead1Byte(pBtCoexist, 0x765);
	pBtCoexist->fBtcRead1Byte(pBtCoexist, 0x67);
}

 
 
 
 
 
 
void EXhalbtc8723b1ant_PowerOnSetting(struct btc_coexist *pBtCoexist)
{
	struct btc_board_info *pBoardInfo = &pBtCoexist->boardInfo;
	u8 u1Tmp = 0x0;
	u16 u2Tmp = 0x0;

	pBtCoexist->fBtcWrite1Byte(pBtCoexist, 0x67, 0x20);

	 
	u2Tmp = pBtCoexist->fBtcRead2Byte(pBtCoexist, 0x2);
	pBtCoexist->fBtcWrite2Byte(pBtCoexist, 0x2, u2Tmp | BIT0 | BIT1);

	 
	pBtCoexist->fBtcWrite1Byte(pBtCoexist, 0x765, 0x18);
	 
	pBtCoexist->fBtcWrite1Byte(pBtCoexist, 0x76e, 0x4);

	 
	 
	 
	 
	 
	 
	 
	if (pBtCoexist->chipInterface == BTC_INTF_USB) {
		 
		pBtCoexist->fBtcWrite4Byte(pBtCoexist, 0x948, 0x0);

		u1Tmp |= 0x1;	 
		pBtCoexist->fBtcWriteLocalReg1Byte(pBtCoexist, 0xfe08, u1Tmp);

		pBoardInfo->btdmAntPos = BTC_ANTENNA_AT_AUX_PORT;
	} else {
		 
		if (pBoardInfo->singleAntPath == 0) {
			 
			pBtCoexist->fBtcWrite4Byte(pBtCoexist, 0x948, 0x280);
			pBoardInfo->btdmAntPos = BTC_ANTENNA_AT_MAIN_PORT;
		} else if (pBoardInfo->singleAntPath == 1) {
			 
			pBtCoexist->fBtcWrite4Byte(pBtCoexist, 0x948, 0x0);
			u1Tmp |= 0x1;	 
			pBoardInfo->btdmAntPos = BTC_ANTENNA_AT_AUX_PORT;
		}

		if (pBtCoexist->chipInterface == BTC_INTF_PCI)
			pBtCoexist->fBtcWriteLocalReg1Byte(pBtCoexist, 0x384, u1Tmp);
		else if (pBtCoexist->chipInterface == BTC_INTF_SDIO)
			pBtCoexist->fBtcWriteLocalReg1Byte(pBtCoexist, 0x60, u1Tmp);
	}
}

void EXhalbtc8723b1ant_InitHwConfig(struct btc_coexist *pBtCoexist, bool bWifiOnly)
{
	halbtc8723b1ant_InitHwConfig(pBtCoexist, true, bWifiOnly);
}

void EXhalbtc8723b1ant_InitCoexDm(struct btc_coexist *pBtCoexist)
{
	pBtCoexist->bStopCoexDm = false;

	halbtc8723b1ant_InitCoexDm(pBtCoexist);

	halbtc8723b1ant_QueryBtInfo(pBtCoexist);
}

void EXhalbtc8723b1ant_IpsNotify(struct btc_coexist *pBtCoexist, u8 type)
{
	if (pBtCoexist->bManualControl ||	pBtCoexist->bStopCoexDm)
		return;

	if (type == BTC_IPS_ENTER) {
		pCoexSta->bUnderIps = true;

		halbtc8723b1ant_PsTdma(pBtCoexist, NORMAL_EXEC, false, 0);
		halbtc8723b1ant_CoexTableWithType(pBtCoexist, NORMAL_EXEC, 0);
		halbtc8723b1ant_SetAntPath(pBtCoexist, BTC_ANT_PATH_BT, false, true);
	} else if (type == BTC_IPS_LEAVE) {
		pCoexSta->bUnderIps = false;

		halbtc8723b1ant_InitHwConfig(pBtCoexist, false, false);
		halbtc8723b1ant_InitCoexDm(pBtCoexist);
		halbtc8723b1ant_QueryBtInfo(pBtCoexist);
	}
}

void EXhalbtc8723b1ant_LpsNotify(struct btc_coexist *pBtCoexist, u8 type)
{
	if (pBtCoexist->bManualControl || pBtCoexist->bStopCoexDm)
		return;

	if (type == BTC_LPS_ENABLE)
		pCoexSta->bUnderLps = true;
	else if (type == BTC_LPS_DISABLE)
		pCoexSta->bUnderLps = false;
}

void EXhalbtc8723b1ant_ScanNotify(struct btc_coexist *pBtCoexist, u8 type)
{
	bool bWifiConnected = false, bBtHsOn = false;
	u32 wifiLinkStatus = 0;
	u32 numOfWifiLink = 0;
	bool bBtCtrlAggBufSize = false;
	u8 aggBufSize = 5;

	if (pBtCoexist->bManualControl || pBtCoexist->bStopCoexDm)
		return;

	if (type == BTC_SCAN_START) {
		pCoexSta->bWiFiIsHighPriTask = true;

		halbtc8723b1ant_PsTdma(pBtCoexist, FORCE_EXEC, false, 8);   
		pBtCoexist->fBtcRead4Byte(pBtCoexist, 0x948);
		pBtCoexist->fBtcRead1Byte(pBtCoexist, 0x765);
		pBtCoexist->fBtcRead1Byte(pBtCoexist, 0x67);
	} else {
		pCoexSta->bWiFiIsHighPriTask = false;

		pBtCoexist->fBtcGet(
			pBtCoexist, BTC_GET_U1_AP_NUM, &pCoexSta->nScanAPNum
		);
	}

	if (pBtCoexist->btInfo.bBtDisabled)
		return;

	pBtCoexist->fBtcGet(pBtCoexist, BTC_GET_BL_HS_OPERATION, &bBtHsOn);
	pBtCoexist->fBtcGet(pBtCoexist, BTC_GET_BL_WIFI_CONNECTED, &bWifiConnected);

	halbtc8723b1ant_QueryBtInfo(pBtCoexist);

	pBtCoexist->fBtcGet(pBtCoexist, BTC_GET_U4_WIFI_LINK_STATUS, &wifiLinkStatus);
	numOfWifiLink = wifiLinkStatus >> 16;

	if (numOfWifiLink >= 2) {
		halbtc8723b1ant_LimitedTx(pBtCoexist, NORMAL_EXEC, 0, 0, 0, 0);
		halbtc8723b1ant_LimitedRx(
			pBtCoexist, NORMAL_EXEC, false, bBtCtrlAggBufSize, aggBufSize
		);
		halbtc8723b1ant_ActionWifiMultiPort(pBtCoexist);
		return;
	}

	if (pCoexSta->bC2hBtInquiryPage) {
		halbtc8723b1ant_ActionBtInquiry(pBtCoexist);
		return;
	} else if (bBtHsOn) {
		halbtc8723b1ant_ActionHs(pBtCoexist);
		return;
	}

	if (type == BTC_SCAN_START) {
		if (!bWifiConnected)	 
			halbtc8723b1ant_ActionWifiNotConnectedScan(pBtCoexist);
		else	 
			halbtc8723b1ant_ActionWifiConnectedScan(pBtCoexist);
	} else if (type == BTC_SCAN_FINISH) {
		if (!bWifiConnected)	 
			halbtc8723b1ant_ActionWifiNotConnected(pBtCoexist);
		else
			halbtc8723b1ant_ActionWifiConnected(pBtCoexist);
	}
}

void EXhalbtc8723b1ant_ConnectNotify(struct btc_coexist *pBtCoexist, u8 type)
{
	bool bWifiConnected = false, bBtHsOn = false;
	u32 wifiLinkStatus = 0;
	u32 numOfWifiLink = 0;
	bool bBtCtrlAggBufSize = false;
	u8 aggBufSize = 5;

	if (
		pBtCoexist->bManualControl ||
		pBtCoexist->bStopCoexDm ||
		pBtCoexist->btInfo.bBtDisabled
	)
		return;

	if (type == BTC_ASSOCIATE_START) {
		pCoexSta->bWiFiIsHighPriTask = true;
		 pCoexDm->nArpCnt = 0;
	} else {
		pCoexSta->bWiFiIsHighPriTask = false;
		 
	}

	pBtCoexist->fBtcGet(pBtCoexist, BTC_GET_U4_WIFI_LINK_STATUS, &wifiLinkStatus);
	numOfWifiLink = wifiLinkStatus >> 16;
	if (numOfWifiLink >= 2) {
		halbtc8723b1ant_LimitedTx(pBtCoexist, NORMAL_EXEC, 0, 0, 0, 0);
		halbtc8723b1ant_LimitedRx(pBtCoexist, NORMAL_EXEC, false, bBtCtrlAggBufSize, aggBufSize);
		halbtc8723b1ant_ActionWifiMultiPort(pBtCoexist);
		return;
	}

	pBtCoexist->fBtcGet(pBtCoexist, BTC_GET_BL_HS_OPERATION, &bBtHsOn);
	if (pCoexSta->bC2hBtInquiryPage) {
		halbtc8723b1ant_ActionBtInquiry(pBtCoexist);
		return;
	} else if (bBtHsOn) {
		halbtc8723b1ant_ActionHs(pBtCoexist);
		return;
	}

	if (type == BTC_ASSOCIATE_START) {
		halbtc8723b1ant_ActionWifiNotConnectedAssoAuth(pBtCoexist);
	} else if (type == BTC_ASSOCIATE_FINISH) {
		pBtCoexist->fBtcGet(pBtCoexist, BTC_GET_BL_WIFI_CONNECTED, &bWifiConnected);
		if (!bWifiConnected)  
			halbtc8723b1ant_ActionWifiNotConnected(pBtCoexist);
		else
			halbtc8723b1ant_ActionWifiConnected(pBtCoexist);
	}
}

void EXhalbtc8723b1ant_MediaStatusNotify(struct btc_coexist *pBtCoexist, u8 type)
{
	u8 H2C_Parameter[3] = {0};
	u32 wifiBw;
	u8 wifiCentralChnl;
	bool bWifiUnderBMode = false;

	if (
		pBtCoexist->bManualControl ||
		pBtCoexist->bStopCoexDm ||
		pBtCoexist->btInfo.bBtDisabled
	)
		return;

	if (type == BTC_MEDIA_CONNECT) {
		pBtCoexist->fBtcGet(pBtCoexist, BTC_GET_BL_WIFI_UNDER_B_MODE, &bWifiUnderBMode);

		 
		if (bWifiUnderBMode) {
			pBtCoexist->fBtcWrite1Byte(pBtCoexist, 0x6cd, 0x00);  
			pBtCoexist->fBtcWrite1Byte(pBtCoexist, 0x6cf, 0x00);  
		} else {
			pBtCoexist->fBtcWrite1Byte(pBtCoexist, 0x6cd, 0x10);  
			pBtCoexist->fBtcWrite1Byte(pBtCoexist, 0x6cf, 0x10);  
		}

		pCoexDm->backupArfrCnt1 = pBtCoexist->fBtcRead4Byte(pBtCoexist, 0x430);
		pCoexDm->backupArfrCnt2 = pBtCoexist->fBtcRead4Byte(pBtCoexist, 0x434);
		pCoexDm->backupRetryLimit = pBtCoexist->fBtcRead2Byte(pBtCoexist, 0x42a);
		pCoexDm->backupAmpduMaxTime = pBtCoexist->fBtcRead1Byte(pBtCoexist, 0x456);
	} else {
		pCoexDm->nArpCnt = 0;

		pBtCoexist->fBtcWrite1Byte(pBtCoexist, 0x6cd, 0x0);  
		pBtCoexist->fBtcWrite1Byte(pBtCoexist, 0x6cf, 0x0);  
	}

	 
	pBtCoexist->fBtcGet(pBtCoexist, BTC_GET_U1_WIFI_CENTRAL_CHNL, &wifiCentralChnl);
	if ((type == BTC_MEDIA_CONNECT) && (wifiCentralChnl <= 14)) {
		 
		H2C_Parameter[0] = 0x0;
		H2C_Parameter[1] = wifiCentralChnl;
		pBtCoexist->fBtcGet(pBtCoexist, BTC_GET_U4_WIFI_BW, &wifiBw);

		if (wifiBw == BTC_WIFI_BW_HT40)
			H2C_Parameter[2] = 0x30;
		else
			H2C_Parameter[2] = 0x20;
	}

	pCoexDm->wifiChnlInfo[0] = H2C_Parameter[0];
	pCoexDm->wifiChnlInfo[1] = H2C_Parameter[1];
	pCoexDm->wifiChnlInfo[2] = H2C_Parameter[2];

	pBtCoexist->fBtcFillH2c(pBtCoexist, 0x66, 3, H2C_Parameter);
}

void EXhalbtc8723b1ant_SpecialPacketNotify(struct btc_coexist *pBtCoexist, u8 type)
{
	bool bBtHsOn = false;
	u32 wifiLinkStatus = 0;
	u32 numOfWifiLink = 0;
	bool bBtCtrlAggBufSize = false;
	u8 aggBufSize = 5;

	if (
		pBtCoexist->bManualControl ||
		pBtCoexist->bStopCoexDm ||
		pBtCoexist->btInfo.bBtDisabled
	)
		return;

	if (
		type == BTC_PACKET_DHCP ||
		type == BTC_PACKET_EAPOL ||
		type == BTC_PACKET_ARP
	) {
		if (type == BTC_PACKET_ARP) {
			pCoexDm->nArpCnt++;

			if (pCoexDm->nArpCnt >= 10)  
				pCoexSta->bWiFiIsHighPriTask = false;
			else
				pCoexSta->bWiFiIsHighPriTask = true;
		} else {
			pCoexSta->bWiFiIsHighPriTask = true;
		}
	} else {
		pCoexSta->bWiFiIsHighPriTask = false;
	}

	pCoexSta->specialPktPeriodCnt = 0;

	pBtCoexist->fBtcGet(
		pBtCoexist, BTC_GET_U4_WIFI_LINK_STATUS, &wifiLinkStatus
	);
	numOfWifiLink = wifiLinkStatus >> 16;

	if (numOfWifiLink >= 2) {
		halbtc8723b1ant_LimitedTx(pBtCoexist, NORMAL_EXEC, 0, 0, 0, 0);
		halbtc8723b1ant_LimitedRx(
			pBtCoexist, NORMAL_EXEC, false, bBtCtrlAggBufSize, aggBufSize
		);
		halbtc8723b1ant_ActionWifiMultiPort(pBtCoexist);
		return;
	}

	pBtCoexist->fBtcGet(pBtCoexist, BTC_GET_BL_HS_OPERATION, &bBtHsOn);
	if (pCoexSta->bC2hBtInquiryPage) {
		halbtc8723b1ant_ActionBtInquiry(pBtCoexist);
		return;
	} else if (bBtHsOn) {
		halbtc8723b1ant_ActionHs(pBtCoexist);
		return;
	}

	if (
		type == BTC_PACKET_DHCP ||
		type == BTC_PACKET_EAPOL ||
		((type == BTC_PACKET_ARP) && (pCoexSta->bWiFiIsHighPriTask))
	)
		halbtc8723b1ant_ActionWifiConnectedSpecialPacket(pBtCoexist);
}

void EXhalbtc8723b1ant_BtInfoNotify(
	struct btc_coexist *pBtCoexist, u8 *tmpBuf, u8 length
)
{
	u8 btInfo = 0;
	u8 i, rspSource = 0;
	bool bWifiConnected = false;
	bool bBtBusy = false;

	pCoexSta->bC2hBtInfoReqSent = false;

	rspSource = tmpBuf[0] & 0xf;
	if (rspSource >= BT_INFO_SRC_8723B_1ANT_MAX)
		rspSource = BT_INFO_SRC_8723B_1ANT_WIFI_FW;
	pCoexSta->btInfoC2hCnt[rspSource]++;

	for (i = 0; i < length; i++) {
		pCoexSta->btInfoC2h[rspSource][i] = tmpBuf[i];
		if (i == 1)
			btInfo = tmpBuf[i];
	}

	if (rspSource != BT_INFO_SRC_8723B_1ANT_WIFI_FW) {
		pCoexSta->btRetryCnt = pCoexSta->btInfoC2h[rspSource][2] & 0xf;

		if (pCoexSta->btRetryCnt >= 1)
			pCoexSta->popEventCnt++;

		if (pCoexSta->btInfoC2h[rspSource][2] & 0x20)
			pCoexSta->bC2hBtPage = true;
		else
			pCoexSta->bC2hBtPage = false;

		pCoexSta->btRssi = pCoexSta->btInfoC2h[rspSource][3] * 2 - 90;
		 

		pCoexSta->btInfoExt = pCoexSta->btInfoC2h[rspSource][4];

		pCoexSta->bBtTxRxMask = (pCoexSta->btInfoC2h[rspSource][2] & 0x40);
		pBtCoexist->fBtcSet(pBtCoexist, BTC_SET_BL_BT_TX_RX_MASK, &pCoexSta->bBtTxRxMask);

		if (!pCoexSta->bBtTxRxMask) {
			 
			pBtCoexist->fBtcSetBtReg(pBtCoexist, BTC_BT_REG_RF, 0x3c, 0x15);
		}

		 
		 
		if (pCoexSta->btInfoExt & BIT1) {
			pBtCoexist->fBtcGet(pBtCoexist, BTC_GET_BL_WIFI_CONNECTED, &bWifiConnected);
			if (bWifiConnected)
				EXhalbtc8723b1ant_MediaStatusNotify(pBtCoexist, BTC_MEDIA_CONNECT);
			else
				EXhalbtc8723b1ant_MediaStatusNotify(pBtCoexist, BTC_MEDIA_DISCONNECT);
		}

		if (pCoexSta->btInfoExt & BIT3) {
			if (!pBtCoexist->bManualControl && !pBtCoexist->bStopCoexDm)
				halbtc8723b1ant_IgnoreWlanAct(pBtCoexist, FORCE_EXEC, false);
		} else {
			 
		}
	}

	 
	if (btInfo & BT_INFO_8723B_1ANT_B_INQ_PAGE)
		pCoexSta->bC2hBtInquiryPage = true;
	else
		pCoexSta->bC2hBtInquiryPage = false;

	 
	if (!(btInfo & BT_INFO_8723B_1ANT_B_CONNECTION)) {
		pCoexSta->bBtLinkExist = false;
		pCoexSta->bPanExist = false;
		pCoexSta->bA2dpExist = false;
		pCoexSta->bHidExist = false;
		pCoexSta->bScoExist = false;
	} else {	 
		pCoexSta->bBtLinkExist = true;
		if (btInfo & BT_INFO_8723B_1ANT_B_FTP)
			pCoexSta->bPanExist = true;
		else
			pCoexSta->bPanExist = false;

		if (btInfo & BT_INFO_8723B_1ANT_B_A2DP)
			pCoexSta->bA2dpExist = true;
		else
			pCoexSta->bA2dpExist = false;

		if (btInfo & BT_INFO_8723B_1ANT_B_HID)
			pCoexSta->bHidExist = true;
		else
			pCoexSta->bHidExist = false;

		if (btInfo & BT_INFO_8723B_1ANT_B_SCO_ESCO)
			pCoexSta->bScoExist = true;
		else
			pCoexSta->bScoExist = false;
	}

	halbtc8723b1ant_UpdateBtLinkInfo(pBtCoexist);

	btInfo = btInfo & 0x1f;   

	if (!(btInfo & BT_INFO_8723B_1ANT_B_CONNECTION)) {
		pCoexDm->btStatus = BT_8723B_1ANT_BT_STATUS_NON_CONNECTED_IDLE;
	} else if (btInfo == BT_INFO_8723B_1ANT_B_CONNECTION)	{
		 
		pCoexDm->btStatus = BT_8723B_1ANT_BT_STATUS_CONNECTED_IDLE;
	} else if (
		(btInfo & BT_INFO_8723B_1ANT_B_SCO_ESCO) ||
		(btInfo & BT_INFO_8723B_1ANT_B_SCO_BUSY)
	) {
		pCoexDm->btStatus = BT_8723B_1ANT_BT_STATUS_SCO_BUSY;
	} else if (btInfo & BT_INFO_8723B_1ANT_B_ACL_BUSY) {
		if (pCoexDm->btStatus != BT_8723B_1ANT_BT_STATUS_ACL_BUSY)
			pCoexDm->bAutoTdmaAdjust = false;

		pCoexDm->btStatus = BT_8723B_1ANT_BT_STATUS_ACL_BUSY;
	} else {
		pCoexDm->btStatus = BT_8723B_1ANT_BT_STATUS_MAX;
	}

	if (
		(pCoexDm->btStatus == BT_8723B_1ANT_BT_STATUS_ACL_BUSY) ||
		(pCoexDm->btStatus == BT_8723B_1ANT_BT_STATUS_SCO_BUSY) ||
		(pCoexDm->btStatus == BT_8723B_1ANT_BT_STATUS_ACL_SCO_BUSY)
	)
		bBtBusy = true;
	else
		bBtBusy = false;
	pBtCoexist->fBtcSet(pBtCoexist, BTC_SET_BL_BT_TRAFFIC_BUSY, &bBtBusy);

	halbtc8723b1ant_RunCoexistMechanism(pBtCoexist);
}

void EXhalbtc8723b1ant_HaltNotify(struct btc_coexist *pBtCoexist)
{
	halbtc8723b1ant_PowerSaveState(pBtCoexist, BTC_PS_WIFI_NATIVE, 0x0, 0x0);
	halbtc8723b1ant_PsTdma(pBtCoexist, FORCE_EXEC, false, 0);
	halbtc8723b1ant_SetAntPath(pBtCoexist, BTC_ANT_PATH_BT, false, true);

	halbtc8723b1ant_IgnoreWlanAct(pBtCoexist, FORCE_EXEC, true);

	EXhalbtc8723b1ant_MediaStatusNotify(pBtCoexist, BTC_MEDIA_DISCONNECT);

	pBtCoexist->bStopCoexDm = true;
}

void EXhalbtc8723b1ant_PnpNotify(struct btc_coexist *pBtCoexist, u8 pnpState)
{
	if (pnpState == BTC_WIFI_PNP_SLEEP) {
		halbtc8723b1ant_PowerSaveState(pBtCoexist, BTC_PS_WIFI_NATIVE, 0x0, 0x0);
		halbtc8723b1ant_PsTdma(pBtCoexist, NORMAL_EXEC, false, 0);
		halbtc8723b1ant_CoexTableWithType(pBtCoexist, NORMAL_EXEC, 2);
		halbtc8723b1ant_SetAntPath(pBtCoexist, BTC_ANT_PATH_BT, false, true);

		pBtCoexist->bStopCoexDm = true;
	} else if (pnpState == BTC_WIFI_PNP_WAKE_UP) {
		pBtCoexist->bStopCoexDm = false;
		halbtc8723b1ant_InitHwConfig(pBtCoexist, false, false);
		halbtc8723b1ant_InitCoexDm(pBtCoexist);
		halbtc8723b1ant_QueryBtInfo(pBtCoexist);
	}
}

void EXhalbtc8723b1ant_Periodical(struct btc_coexist *pBtCoexist)
{
	static u8 disVerInfoCnt;
	u32 fwVer = 0, btPatchVer = 0;

	if (disVerInfoCnt <= 5) {
		disVerInfoCnt += 1;
		pBtCoexist->fBtcGet(pBtCoexist, BTC_GET_U4_BT_PATCH_VER, &btPatchVer);
		pBtCoexist->fBtcGet(pBtCoexist, BTC_GET_U4_WIFI_FW_VER, &fwVer);
	}

	halbtc8723b1ant_MonitorBtCtr(pBtCoexist);
	halbtc8723b1ant_MonitorWiFiCtr(pBtCoexist);

	if (
		halbtc8723b1ant_IsWifiStatusChanged(pBtCoexist) ||
		pCoexDm->bAutoTdmaAdjust
	)
		halbtc8723b1ant_RunCoexistMechanism(pBtCoexist);

	pCoexSta->specialPktPeriodCnt++;
}
