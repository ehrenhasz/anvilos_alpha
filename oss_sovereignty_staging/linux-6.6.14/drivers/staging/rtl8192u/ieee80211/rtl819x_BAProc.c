
 
#include <asm/byteorder.h>
#include <asm/unaligned.h>
#include "ieee80211.h"
#include "rtl819x_BA.h"

 
static void ActivateBAEntry(struct ieee80211_device *ieee, struct ba_record *pBA, u16 Time)
{
	pBA->valid = true;
	if (Time != 0)
		mod_timer(&pBA->timer, jiffies + msecs_to_jiffies(Time));
}

 
static void DeActivateBAEntry(struct ieee80211_device *ieee, struct ba_record *pBA)
{
	pBA->valid = false;
	del_timer_sync(&pBA->timer);
}
 
static u8 TxTsDeleteBA(struct ieee80211_device *ieee, struct tx_ts_record *pTxTs)
{
	struct ba_record *pAdmittedBa = &pTxTs->tx_admitted_ba_record;  
	struct ba_record *pPendingBa = &pTxTs->tx_pending_ba_record;
	u8			bSendDELBA = false;

	
	if (pPendingBa->valid) {
		DeActivateBAEntry(ieee, pPendingBa);
		bSendDELBA = true;
	}

	
	if (pAdmittedBa->valid) {
		DeActivateBAEntry(ieee, pAdmittedBa);
		bSendDELBA = true;
	}

	return bSendDELBA;
}

 
static u8 RxTsDeleteBA(struct ieee80211_device *ieee, struct rx_ts_record *pRxTs)
{
	struct ba_record       *pBa = &pRxTs->rx_admitted_ba_record;
	u8			bSendDELBA = false;

	if (pBa->valid) {
		DeActivateBAEntry(ieee, pBa);
		bSendDELBA = true;
	}

	return bSendDELBA;
}

 
void ResetBaEntry(struct ba_record *pBA)
{
	pBA->valid			= false;
	pBA->param_set.short_data	= 0;
	pBA->timeout_value		= 0;
	pBA->dialog_token		= 0;
	pBA->start_seq_ctrl.short_data	= 0;
}

 
static struct sk_buff *ieee80211_ADDBA(struct ieee80211_device *ieee, u8 *Dst, struct ba_record *pBA, u16 StatusCode, u8 type)
{
	struct sk_buff *skb = NULL;
	struct rtl_80211_hdr_3addr *BAReq = NULL;
	u8 *tag = NULL;
	u16 len = ieee->tx_headroom + 9;
	
	IEEE80211_DEBUG(IEEE80211_DL_TRACE | IEEE80211_DL_BA, "========>%s(), frame(%d) sentd to:%pM, ieee->dev:%p\n", __func__, type, Dst, ieee->dev);
	if (pBA == NULL) {
		IEEE80211_DEBUG(IEEE80211_DL_ERR, "pBA is NULL\n");
		return NULL;
	}
	skb = dev_alloc_skb(len + sizeof(struct rtl_80211_hdr_3addr)); 
	if (!skb)
		return NULL;

	memset(skb->data, 0, sizeof(struct rtl_80211_hdr_3addr));	
	skb_reserve(skb, ieee->tx_headroom);

	BAReq = skb_put(skb, sizeof(struct rtl_80211_hdr_3addr));

	memcpy(BAReq->addr1, Dst, ETH_ALEN);
	memcpy(BAReq->addr2, ieee->dev->dev_addr, ETH_ALEN);

	memcpy(BAReq->addr3, ieee->current_network.bssid, ETH_ALEN);

	BAReq->frame_ctl = cpu_to_le16(IEEE80211_STYPE_MANAGE_ACT); 

	
	tag = skb_put(skb, 9);
	*tag++ = ACT_CAT_BA;
	*tag++ = type;
	
	*tag++ = pBA->dialog_token;

	if (type == ACT_ADDBARSP) {
		
		netdev_info(ieee->dev, "=====>to send ADDBARSP\n");

		put_unaligned_le16(StatusCode, tag);
		tag += 2;
	}
	

	put_unaligned_le16(pBA->param_set.short_data, tag);
	tag += 2;
	

	put_unaligned_le16(pBA->timeout_value, tag);
	tag += 2;

	if (type == ACT_ADDBAREQ) {
	
		memcpy(tag, (u8 *)&(pBA->start_seq_ctrl), 2);
		tag += 2;
	}

	IEEE80211_DEBUG_DATA(IEEE80211_DL_DATA | IEEE80211_DL_BA, skb->data, skb->len);
	return skb;
	
}


 
static struct sk_buff *ieee80211_DELBA(
	struct ieee80211_device  *ieee,
	u8		         *dst,
	struct ba_record         *pBA,
	enum tr_select		 TxRxSelect,
	u16			 ReasonCode
	)
{
	union delba_param_set	DelbaParamSet;
	struct sk_buff *skb = NULL;
	struct rtl_80211_hdr_3addr *Delba = NULL;
	u8 *tag = NULL;
	
	u16 len = 6 + ieee->tx_headroom;

	if (net_ratelimit())
		IEEE80211_DEBUG(IEEE80211_DL_TRACE | IEEE80211_DL_BA,
				"========>%s(), ReasonCode(%d) sentd to:%pM\n",
				__func__, ReasonCode, dst);

	memset(&DelbaParamSet, 0, 2);

	DelbaParamSet.field.initiator	= (TxRxSelect == TX_DIR) ? 1 : 0;
	DelbaParamSet.field.tid	= pBA->param_set.field.tid;

	skb = dev_alloc_skb(len + sizeof(struct rtl_80211_hdr_3addr)); 
	if (!skb)
		return NULL;

	skb_reserve(skb, ieee->tx_headroom);

	Delba = skb_put(skb, sizeof(struct rtl_80211_hdr_3addr));

	memcpy(Delba->addr1, dst, ETH_ALEN);
	memcpy(Delba->addr2, ieee->dev->dev_addr, ETH_ALEN);
	memcpy(Delba->addr3, ieee->current_network.bssid, ETH_ALEN);
	Delba->frame_ctl = cpu_to_le16(IEEE80211_STYPE_MANAGE_ACT); 

	tag = skb_put(skb, 6);

	*tag++ = ACT_CAT_BA;
	*tag++ = ACT_DELBA;

	

	put_unaligned_le16(DelbaParamSet.short_data, tag);
	tag += 2;
	

	put_unaligned_le16(ReasonCode, tag);
	tag += 2;

	IEEE80211_DEBUG_DATA(IEEE80211_DL_DATA | IEEE80211_DL_BA, skb->data, skb->len);
	if (net_ratelimit())
		IEEE80211_DEBUG(IEEE80211_DL_TRACE | IEEE80211_DL_BA,
				"<=====%s()\n", __func__);
	return skb;
}

 
static void ieee80211_send_ADDBAReq(struct ieee80211_device *ieee,
				    u8 *dst, struct ba_record *pBA)
{
	struct sk_buff *skb;
	skb = ieee80211_ADDBA(ieee, dst, pBA, 0, ACT_ADDBAREQ); 

	if (skb) {
		softmac_mgmt_xmit(skb, ieee);
		
		
		
	} else {
		IEEE80211_DEBUG(IEEE80211_DL_ERR, "alloc skb error in function %s()\n", __func__);
	}
}

 
static void ieee80211_send_ADDBARsp(struct ieee80211_device *ieee, u8 *dst,
				    struct ba_record *pBA, u16 StatusCode)
{
	struct sk_buff *skb;
	skb = ieee80211_ADDBA(ieee, dst, pBA, StatusCode, ACT_ADDBARSP); 
	if (skb) {
		softmac_mgmt_xmit(skb, ieee);
		
	} else {
		IEEE80211_DEBUG(IEEE80211_DL_ERR, "alloc skb error in function %s()\n", __func__);
	}

	return;

}
 

static void ieee80211_send_DELBA(struct ieee80211_device *ieee, u8 *dst,
				 struct ba_record *pBA, enum tr_select TxRxSelect,
				 u16 ReasonCode)
{
	struct sk_buff *skb;
	skb = ieee80211_DELBA(ieee, dst, pBA, TxRxSelect, ReasonCode); 
	if (skb) {
		softmac_mgmt_xmit(skb, ieee);
		
	} else {
		IEEE80211_DEBUG(IEEE80211_DL_ERR, "alloc skb error in function %s()\n", __func__);
	}
}

 
int ieee80211_rx_ADDBAReq(struct ieee80211_device *ieee, struct sk_buff *skb)
{
	struct rtl_80211_hdr_3addr *req = NULL;
	u16 rc = 0;
	u8 *dst = NULL, *pDialogToken = NULL, *tag = NULL;
	struct ba_record *pBA = NULL;
	union ba_param_set     *pBaParamSet = NULL;
	u16 *pBaTimeoutVal = NULL;
	union sequence_control *pBaStartSeqCtrl = NULL;
	struct rx_ts_record  *pTS = NULL;

	if (skb->len < sizeof(struct rtl_80211_hdr_3addr) + 9) {
		IEEE80211_DEBUG(IEEE80211_DL_ERR,
				" Invalid skb len in BAREQ(%d / %zu)\n",
				skb->len,
				(sizeof(struct rtl_80211_hdr_3addr) + 9));
		return -1;
	}

	IEEE80211_DEBUG_DATA(IEEE80211_DL_DATA | IEEE80211_DL_BA, skb->data, skb->len);

	req = (struct rtl_80211_hdr_3addr *)skb->data;
	tag = (u8 *)req;
	dst = &req->addr2[0];
	tag += sizeof(struct rtl_80211_hdr_3addr);
	pDialogToken = tag + 2;  
	pBaParamSet = (union ba_param_set *)(tag + 3);   
	pBaTimeoutVal = (u16 *)(tag + 5);
	pBaStartSeqCtrl = (union sequence_control *)(req + 7);

	netdev_info(ieee->dev, "====================>rx ADDBAREQ from :%pM\n", dst);

	if ((ieee->current_network.qos_data.active == 0) ||
		(!ieee->pHTInfo->bCurrentHTSupport)) 
	
	{
		rc = ADDBA_STATUS_REFUSED;
		IEEE80211_DEBUG(IEEE80211_DL_ERR, "Failed to reply on ADDBA_REQ as some capability is not ready(%d, %d)\n", ieee->current_network.qos_data.active, ieee->pHTInfo->bCurrentHTSupport);
		goto OnADDBAReq_Fail;
	}
	
	
	if (!GetTs(
			ieee,
			(struct ts_common_info **)(&pTS),
			dst,
			(u8)(pBaParamSet->field.tid),
			RX_DIR,
			true)) {
		rc = ADDBA_STATUS_REFUSED;
		IEEE80211_DEBUG(IEEE80211_DL_ERR, "can't get TS in %s()\n", __func__);
		goto OnADDBAReq_Fail;
	}
	pBA = &pTS->rx_admitted_ba_record;
	
	
	
	
	if (pBaParamSet->field.ba_policy == BA_POLICY_DELAYED) {
		rc = ADDBA_STATUS_INVALID_PARAM;
		IEEE80211_DEBUG(IEEE80211_DL_ERR, "BA Policy is not correct in %s()\n", __func__);
		goto OnADDBAReq_Fail;
	}
		
	
	DeActivateBAEntry(ieee, pBA);
	pBA->dialog_token = *pDialogToken;
	pBA->param_set = *pBaParamSet;
	pBA->timeout_value = *pBaTimeoutVal;
	pBA->start_seq_ctrl = *pBaStartSeqCtrl;
	
	if (ieee->GetHalfNmodeSupportByAPsHandler(ieee->dev))
		pBA->param_set.field.buffer_size = 1;
	else
		pBA->param_set.field.buffer_size = 32;
	ActivateBAEntry(ieee, pBA, pBA->timeout_value);
	ieee80211_send_ADDBARsp(ieee, dst, pBA, ADDBA_STATUS_SUCCESS);

	
	return 0;

OnADDBAReq_Fail:
	{
		struct ba_record	BA;
		BA.param_set = *pBaParamSet;
		BA.timeout_value = *pBaTimeoutVal;
		BA.dialog_token = *pDialogToken;
		BA.param_set.field.ba_policy = BA_POLICY_IMMEDIATE;
		ieee80211_send_ADDBARsp(ieee, dst, &BA, rc);
		return 0; 
	}

}

 
int ieee80211_rx_ADDBARsp(struct ieee80211_device *ieee, struct sk_buff *skb)
{
	struct rtl_80211_hdr_3addr *rsp = NULL;
	struct ba_record        *pPendingBA, *pAdmittedBA;
	struct tx_ts_record     *pTS = NULL;
	u8 *dst = NULL, *pDialogToken = NULL, *tag = NULL;
	u16 *pStatusCode = NULL, *pBaTimeoutVal = NULL;
	union ba_param_set       *pBaParamSet = NULL;
	u16			ReasonCode;

	if (skb->len < sizeof(struct rtl_80211_hdr_3addr) + 9) {
		IEEE80211_DEBUG(IEEE80211_DL_ERR,
				" Invalid skb len in BARSP(%d / %zu)\n",
				skb->len,
				(sizeof(struct rtl_80211_hdr_3addr) + 9));
		return -1;
	}
	rsp = (struct rtl_80211_hdr_3addr *)skb->data;
	tag = (u8 *)rsp;
	dst = &rsp->addr2[0];
	tag += sizeof(struct rtl_80211_hdr_3addr);
	pDialogToken = tag + 2;
	pStatusCode = (u16 *)(tag + 3);
	pBaParamSet = (union ba_param_set *)(tag + 5);
	pBaTimeoutVal = (u16 *)(tag + 7);

	
	
	if (ieee->current_network.qos_data.active == 0  ||
	    !ieee->pHTInfo->bCurrentHTSupport ||
	    !ieee->pHTInfo->bCurrentAMPDUEnable) {
		IEEE80211_DEBUG(IEEE80211_DL_ERR, "reject to ADDBA_RSP as some capability is not ready(%d, %d, %d)\n", ieee->current_network.qos_data.active, ieee->pHTInfo->bCurrentHTSupport, ieee->pHTInfo->bCurrentAMPDUEnable);
		ReasonCode = DELBA_REASON_UNKNOWN_BA;
		goto OnADDBARsp_Reject;
	}


	
	
	
	
	if (!GetTs(
			ieee,
			(struct ts_common_info **)(&pTS),
			dst,
			(u8)(pBaParamSet->field.tid),
			TX_DIR,
			false)) {
		IEEE80211_DEBUG(IEEE80211_DL_ERR, "can't get TS in %s()\n", __func__);
		ReasonCode = DELBA_REASON_UNKNOWN_BA;
		goto OnADDBARsp_Reject;
	}

	pTS->add_ba_req_in_progress = false;
	pPendingBA = &pTS->tx_pending_ba_record;
	pAdmittedBA = &pTS->tx_admitted_ba_record;


	
	
	
	
	if (pAdmittedBA->valid) {
		
		IEEE80211_DEBUG(IEEE80211_DL_BA, "OnADDBARsp(): Recv ADDBA Rsp. Drop because already admit it! \n");
		return -1;
	} else if ((!pPendingBA->valid) || (*pDialogToken != pPendingBA->dialog_token)) {
		IEEE80211_DEBUG(IEEE80211_DL_ERR,  "OnADDBARsp(): Recv ADDBA Rsp. BA invalid, DELBA! \n");
		ReasonCode = DELBA_REASON_UNKNOWN_BA;
		goto OnADDBARsp_Reject;
	} else {
		IEEE80211_DEBUG(IEEE80211_DL_BA, "OnADDBARsp(): Recv ADDBA Rsp. BA is admitted! Status code:%X\n", *pStatusCode);
		DeActivateBAEntry(ieee, pPendingBA);
	}


	if (*pStatusCode == ADDBA_STATUS_SUCCESS) {
		
		
		
		
		
		if (pBaParamSet->field.ba_policy == BA_POLICY_DELAYED) {
			
			pTS->add_ba_req_delayed = true;
			DeActivateBAEntry(ieee, pAdmittedBA);
			ReasonCode = DELBA_REASON_END_BA;
			goto OnADDBARsp_Reject;
		}


		
		
		
		pAdmittedBA->dialog_token = *pDialogToken;
		pAdmittedBA->timeout_value = *pBaTimeoutVal;
		pAdmittedBA->start_seq_ctrl = pPendingBA->start_seq_ctrl;
		pAdmittedBA->param_set = *pBaParamSet;
		DeActivateBAEntry(ieee, pAdmittedBA);
		ActivateBAEntry(ieee, pAdmittedBA, *pBaTimeoutVal);
	} else {
		
		pTS->add_ba_req_delayed = true;
	}

	
	return 0;

OnADDBARsp_Reject:
	{
		struct ba_record	BA;
		BA.param_set = *pBaParamSet;
		ieee80211_send_DELBA(ieee, dst, &BA, TX_DIR, ReasonCode);
		return 0;
	}

}

 
int ieee80211_rx_DELBA(struct ieee80211_device *ieee, struct sk_buff *skb)
{
	struct rtl_80211_hdr_3addr *delba = NULL;
	union delba_param_set   *pDelBaParamSet = NULL;
	u8			*dst = NULL;

	if (skb->len < sizeof(struct rtl_80211_hdr_3addr) + 6) {
		IEEE80211_DEBUG(IEEE80211_DL_ERR,
				" Invalid skb len in DELBA(%d / %zu)\n",
				skb->len,
				(sizeof(struct rtl_80211_hdr_3addr) + 6));
		return -1;
	}

	if (ieee->current_network.qos_data.active == 0 ||
	    !ieee->pHTInfo->bCurrentHTSupport) {
		IEEE80211_DEBUG(IEEE80211_DL_ERR, "received DELBA while QOS or HT is not supported(%d, %d)\n", ieee->current_network.qos_data.active, ieee->pHTInfo->bCurrentHTSupport);
		return -1;
	}

	IEEE80211_DEBUG_DATA(IEEE80211_DL_DATA | IEEE80211_DL_BA, skb->data, skb->len);
	delba = (struct rtl_80211_hdr_3addr *)skb->data;
	dst = &delba->addr2[0];
	pDelBaParamSet = (union delba_param_set *)&delba->payload[2];

	if (pDelBaParamSet->field.initiator == 1) {
		struct rx_ts_record *pRxTs;

		if (!GetTs(
				ieee,
				(struct ts_common_info **)&pRxTs,
				dst,
				(u8)pDelBaParamSet->field.tid,
				RX_DIR,
				false)) {
			IEEE80211_DEBUG(IEEE80211_DL_ERR,  "can't get TS for RXTS in %s()\n", __func__);
			return -1;
		}

		RxTsDeleteBA(ieee, pRxTs);
	} else {
		struct tx_ts_record *pTxTs;

		if (!GetTs(
			ieee,
			(struct ts_common_info **)&pTxTs,
			dst,
			(u8)pDelBaParamSet->field.tid,
			TX_DIR,
			false)) {
			IEEE80211_DEBUG(IEEE80211_DL_ERR,  "can't get TS for TXTS in %s()\n", __func__);
			return -1;
		}

		pTxTs->using_ba = false;
		pTxTs->add_ba_req_in_progress = false;
		pTxTs->add_ba_req_delayed = false;
		del_timer_sync(&pTxTs->ts_add_ba_timer);
		
		TxTsDeleteBA(ieee, pTxTs);
	}
	return 0;
}




void
TsInitAddBA(
	struct ieee80211_device *ieee,
	struct tx_ts_record     *pTS,
	u8		Policy,
	u8		bOverwritePending
	)
{
	struct ba_record *pBA = &pTS->tx_pending_ba_record;

	if (pBA->valid && !bOverwritePending)
		return;

	
	DeActivateBAEntry(ieee, pBA);

	pBA->dialog_token++;						
	pBA->param_set.field.amsdu_support = 0;	
	pBA->param_set.field.ba_policy = Policy;	
	pBA->param_set.field.tid = pTS->ts_common_info.t_spec.ts_info.uc_tsid;	
	
	pBA->param_set.field.buffer_size = 32;		
	pBA->timeout_value = 0;					
	pBA->start_seq_ctrl.field.seq_num = (pTS->tx_cur_seq + 3) % 4096;	

	ActivateBAEntry(ieee, pBA, BA_SETUP_TIMEOUT);

	ieee80211_send_ADDBAReq(ieee, pTS->ts_common_info.addr, pBA);
}

void
TsInitDelBA(struct ieee80211_device *ieee, struct ts_common_info *pTsCommonInfo, enum tr_select TxRxSelect)
{
	if (TxRxSelect == TX_DIR) {
		struct tx_ts_record *pTxTs = (struct tx_ts_record *)pTsCommonInfo;

		if (TxTsDeleteBA(ieee, pTxTs))
			ieee80211_send_DELBA(
				ieee,
				pTsCommonInfo->addr,
				(pTxTs->tx_admitted_ba_record.valid) ? (&pTxTs->tx_admitted_ba_record) : (&pTxTs->tx_pending_ba_record),
				TxRxSelect,
				DELBA_REASON_END_BA);
	} else if (TxRxSelect == RX_DIR) {
		struct rx_ts_record *pRxTs = (struct rx_ts_record *)pTsCommonInfo;
		if (RxTsDeleteBA(ieee, pRxTs))
			ieee80211_send_DELBA(
				ieee,
				pTsCommonInfo->addr,
				&pRxTs->rx_admitted_ba_record,
				TxRxSelect,
				DELBA_REASON_END_BA);
	}
}
 
void BaSetupTimeOut(struct timer_list *t)
{
	struct tx_ts_record *pTxTs = from_timer(pTxTs, t, tx_pending_ba_record.timer);

	pTxTs->add_ba_req_in_progress = false;
	pTxTs->add_ba_req_delayed = true;
	pTxTs->tx_pending_ba_record.valid = false;
}

void TxBaInactTimeout(struct timer_list *t)
{
	struct tx_ts_record *pTxTs = from_timer(pTxTs, t, tx_admitted_ba_record.timer);
	struct ieee80211_device *ieee = container_of(pTxTs, struct ieee80211_device, TxTsRecord[pTxTs->num]);
	TxTsDeleteBA(ieee, pTxTs);
	ieee80211_send_DELBA(
		ieee,
		pTxTs->ts_common_info.addr,
		&pTxTs->tx_admitted_ba_record,
		TX_DIR,
		DELBA_REASON_TIMEOUT);
}

void RxBaInactTimeout(struct timer_list *t)
{
	struct rx_ts_record *pRxTs = from_timer(pRxTs, t, rx_admitted_ba_record.timer);
	struct ieee80211_device *ieee = container_of(pRxTs, struct ieee80211_device, RxTsRecord[pRxTs->num]);

	RxTsDeleteBA(ieee, pRxTs);
	ieee80211_send_DELBA(
		ieee,
		pRxTs->ts_common_info.addr,
		&pRxTs->rx_admitted_ba_record,
		RX_DIR,
		DELBA_REASON_TIMEOUT);
}
