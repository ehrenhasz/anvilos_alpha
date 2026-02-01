
#include "ieee80211.h"
#include <linux/etherdevice.h>
#include <linux/slab.h>
#include "rtl819x_TS.h"

static void TsSetupTimeOut(struct timer_list *unused)
{
	
	
}

static void TsInactTimeout(struct timer_list *unused)
{
	
	
	
}

 
static void RxPktPendingTimeout(struct timer_list *t)
{
	struct rx_ts_record     *pRxTs = from_timer(pRxTs, t, rx_pkt_pending_timer);
	struct ieee80211_device *ieee = container_of(pRxTs, struct ieee80211_device, RxTsRecord[pRxTs->num]);

	struct rx_reorder_entry	*pReorderEntry = NULL;

	
	unsigned long flags = 0;
	u8 index = 0;
	bool bPktInBuf = false;

	spin_lock_irqsave(&(ieee->reorder_spinlock), flags);
	IEEE80211_DEBUG(IEEE80211_DL_REORDER, "==================>%s()\n", __func__);
	if (pRxTs->rx_timeout_indicate_seq != 0xffff) {
		
		while (!list_empty(&pRxTs->rx_pending_pkt_list)) {
			pReorderEntry = list_entry(pRxTs->rx_pending_pkt_list.prev, struct rx_reorder_entry, List);
			if (index == 0)
				pRxTs->rx_indicate_seq = pReorderEntry->SeqNum;

			if (SN_LESS(pReorderEntry->SeqNum, pRxTs->rx_indicate_seq) ||
				SN_EQUAL(pReorderEntry->SeqNum, pRxTs->rx_indicate_seq)) {
				list_del_init(&pReorderEntry->List);

				if (SN_EQUAL(pReorderEntry->SeqNum, pRxTs->rx_indicate_seq))
					pRxTs->rx_indicate_seq = (pRxTs->rx_indicate_seq + 1) % 4096;

				IEEE80211_DEBUG(IEEE80211_DL_REORDER, "%s: IndicateSeq: %d\n", __func__, pReorderEntry->SeqNum);
				ieee->stats_IndicateArray[index] = pReorderEntry->prxb;
				index++;

				list_add_tail(&pReorderEntry->List, &ieee->RxReorder_Unused_List);
			} else {
				bPktInBuf = true;
				break;
			}
		}
	}

	if (index > 0) {
		
		pRxTs->rx_timeout_indicate_seq = 0xffff;

		
		if (index > REORDER_WIN_SIZE) {
			IEEE80211_DEBUG(IEEE80211_DL_ERR, "RxReorderIndicatePacket(): Rx Reorder buffer full!! \n");
			spin_unlock_irqrestore(&(ieee->reorder_spinlock), flags);
			return;
		}
		ieee80211_indicate_packets(ieee, ieee->stats_IndicateArray, index);
	}

	if (bPktInBuf && (pRxTs->rx_timeout_indicate_seq == 0xffff)) {
		pRxTs->rx_timeout_indicate_seq = pRxTs->rx_indicate_seq;
		mod_timer(&pRxTs->rx_pkt_pending_timer,
			  jiffies + msecs_to_jiffies(ieee->pHTInfo->RxReorderPendingTime));
	}
	spin_unlock_irqrestore(&(ieee->reorder_spinlock), flags);
}

 
static void TsAddBaProcess(struct timer_list *t)
{
	struct tx_ts_record *pTxTs = from_timer(pTxTs, t, ts_add_ba_timer);
	u8 num = pTxTs->num;
	struct ieee80211_device *ieee = container_of(pTxTs, struct ieee80211_device, TxTsRecord[num]);

	TsInitAddBA(ieee, pTxTs, BA_POLICY_IMMEDIATE, false);
	IEEE80211_DEBUG(IEEE80211_DL_BA, "%s: ADDBA Req is started!! \n", __func__);
}


static void ResetTsCommonInfo(struct ts_common_info *pTsCommonInfo)
{
	eth_zero_addr(pTsCommonInfo->addr);
	memset(&pTsCommonInfo->t_spec, 0, sizeof(struct tspec_body));
	memset(&pTsCommonInfo->t_class, 0, sizeof(union qos_tclas) * TCLAS_NUM);
	pTsCommonInfo->t_clas_proc = 0;
	pTsCommonInfo->t_clas_num = 0;
}

static void ResetTxTsEntry(struct tx_ts_record *pTS)
{
	ResetTsCommonInfo(&pTS->ts_common_info);
	pTS->tx_cur_seq = 0;
	pTS->add_ba_req_in_progress = false;
	pTS->add_ba_req_delayed = false;
	pTS->using_ba = false;
	ResetBaEntry(&pTS->tx_admitted_ba_record); 
	ResetBaEntry(&pTS->tx_pending_ba_record);
}

static void ResetRxTsEntry(struct rx_ts_record *pTS)
{
	ResetTsCommonInfo(&pTS->ts_common_info);
	pTS->rx_indicate_seq = 0xffff; 
	pTS->rx_timeout_indicate_seq = 0xffff; 
	ResetBaEntry(&pTS->rx_admitted_ba_record);	  
}

void TSInitialize(struct ieee80211_device *ieee)
{
	struct tx_ts_record     *pTxTS  = ieee->TxTsRecord;
	struct rx_ts_record     *pRxTS  = ieee->RxTsRecord;
	struct rx_reorder_entry	*pRxReorderEntry = ieee->RxReorderEntry;
	u8				count = 0;
	IEEE80211_DEBUG(IEEE80211_DL_TS, "==========>%s()\n", __func__);
	
	INIT_LIST_HEAD(&ieee->Tx_TS_Admit_List);
	INIT_LIST_HEAD(&ieee->Tx_TS_Pending_List);
	INIT_LIST_HEAD(&ieee->Tx_TS_Unused_List);

	for (count = 0; count < TOTAL_TS_NUM; count++) {
		
		pTxTS->num = count;
		
		
		timer_setup(&pTxTS->ts_common_info.setup_timer, TsSetupTimeOut,
			    0);
		timer_setup(&pTxTS->ts_common_info.inact_timer, TsInactTimeout,
			    0);
		timer_setup(&pTxTS->ts_add_ba_timer, TsAddBaProcess, 0);
		timer_setup(&pTxTS->tx_pending_ba_record.timer, BaSetupTimeOut,
			    0);
		timer_setup(&pTxTS->tx_admitted_ba_record.timer,
			    TxBaInactTimeout, 0);
		ResetTxTsEntry(pTxTS);
		list_add_tail(&pTxTS->ts_common_info.list, &ieee->Tx_TS_Unused_List);
		pTxTS++;
	}

	
	INIT_LIST_HEAD(&ieee->Rx_TS_Admit_List);
	INIT_LIST_HEAD(&ieee->Rx_TS_Pending_List);
	INIT_LIST_HEAD(&ieee->Rx_TS_Unused_List);
	for (count = 0; count < TOTAL_TS_NUM; count++) {
		pRxTS->num = count;
		INIT_LIST_HEAD(&pRxTS->rx_pending_pkt_list);
		timer_setup(&pRxTS->ts_common_info.setup_timer, TsSetupTimeOut,
			    0);
		timer_setup(&pRxTS->ts_common_info.inact_timer, TsInactTimeout,
			    0);
		timer_setup(&pRxTS->rx_admitted_ba_record.timer,
			    RxBaInactTimeout, 0);
		timer_setup(&pRxTS->rx_pkt_pending_timer, RxPktPendingTimeout, 0);
		ResetRxTsEntry(pRxTS);
		list_add_tail(&pRxTS->ts_common_info.list, &ieee->Rx_TS_Unused_List);
		pRxTS++;
	}
	
	INIT_LIST_HEAD(&ieee->RxReorder_Unused_List);
	for (count = 0; count < REORDER_ENTRY_NUM; count++) {
		list_add_tail(&pRxReorderEntry->List, &ieee->RxReorder_Unused_List);
		if (count == (REORDER_ENTRY_NUM - 1))
			break;
		pRxReorderEntry = &ieee->RxReorderEntry[count + 1];
	}
}

static void AdmitTS(struct ieee80211_device *ieee,
		    struct ts_common_info *pTsCommonInfo, u32 InactTime)
{
	del_timer_sync(&pTsCommonInfo->setup_timer);
	del_timer_sync(&pTsCommonInfo->inact_timer);

	if (InactTime != 0)
		mod_timer(&pTsCommonInfo->inact_timer,
			  jiffies + msecs_to_jiffies(InactTime));
}


static struct ts_common_info *SearchAdmitTRStream(struct ieee80211_device *ieee,
						  u8 *Addr, u8 TID,
						  enum tr_select TxRxSelect)
{
	
	u8	dir;
	bool				search_dir[4] = {0};
	struct list_head		*psearch_list; 
	struct ts_common_info	*pRet = NULL;
	if (ieee->iw_mode == IW_MODE_MASTER) { 
		if (TxRxSelect == TX_DIR) {
			search_dir[DIR_DOWN] = true;
			search_dir[DIR_BI_DIR] = true;
		} else {
			search_dir[DIR_UP]	= true;
			search_dir[DIR_BI_DIR] = true;
		}
	} else if (ieee->iw_mode == IW_MODE_ADHOC) {
		if (TxRxSelect == TX_DIR)
			search_dir[DIR_UP]	= true;
		else
			search_dir[DIR_DOWN] = true;
	} else {
		if (TxRxSelect == TX_DIR) {
			search_dir[DIR_UP]	= true;
			search_dir[DIR_BI_DIR] = true;
			search_dir[DIR_DIRECT] = true;
		} else {
			search_dir[DIR_DOWN] = true;
			search_dir[DIR_BI_DIR] = true;
			search_dir[DIR_DIRECT] = true;
		}
	}

	if (TxRxSelect == TX_DIR)
		psearch_list = &ieee->Tx_TS_Admit_List;
	else
		psearch_list = &ieee->Rx_TS_Admit_List;

	
	for (dir = 0; dir <= DIR_BI_DIR; dir++) {
		if (!search_dir[dir])
			continue;
		list_for_each_entry(pRet, psearch_list, list) {
	
			if (memcmp(pRet->addr, Addr, 6) == 0)
				if (pRet->t_spec.ts_info.uc_tsid == TID)
					if (pRet->t_spec.ts_info.uc_direction == dir) {
	
						break;
					}
		}
		if (&pRet->list  != psearch_list)
			break;
	}

	if (&pRet->list  != psearch_list)
		return pRet;
	else
		return NULL;
}

static void MakeTSEntry(struct ts_common_info *pTsCommonInfo, u8 *Addr,
			struct tspec_body *pTSPEC, union qos_tclas *pTCLAS, u8 TCLAS_Num,
			u8 TCLAS_Proc)
{
	u8	count;

	if (pTsCommonInfo == NULL)
		return;

	memcpy(pTsCommonInfo->addr, Addr, 6);

	if (pTSPEC != NULL)
		memcpy((u8 *)(&(pTsCommonInfo->t_spec)), (u8 *)pTSPEC, sizeof(struct tspec_body));

	for (count = 0; count < TCLAS_Num; count++)
		memcpy((u8 *)(&(pTsCommonInfo->t_class[count])), (u8 *)pTCLAS, sizeof(union qos_tclas));

	pTsCommonInfo->t_clas_proc = TCLAS_Proc;
	pTsCommonInfo->t_clas_num = TCLAS_Num;
}


bool GetTs(
	struct ieee80211_device		*ieee,
	struct ts_common_info		**ppTS,
	u8				*Addr,
	u8				TID,
	enum tr_select			TxRxSelect,  
	bool				bAddNewTs
	)
{
	u8	UP = 0;
	
	
	
	
	if (is_multicast_ether_addr(Addr)) {
		IEEE80211_DEBUG(IEEE80211_DL_ERR, "get TS for Broadcast or Multicast\n");
		return false;
	}

	if (ieee->current_network.qos_data.supported == 0) {
		UP = 0;
	} else {
		
		if (!is_ac_valid(TID)) {
			IEEE80211_DEBUG(IEEE80211_DL_ERR, " in %s(), TID(%d) is not valid\n", __func__, TID);
			return false;
		}

		switch (TID) {
		case 0:
		case 3:
			UP = 0;
			break;

		case 1:
		case 2:
			UP = 2;
			break;

		case 4:
		case 5:
			UP = 5;
			break;

		case 6:
		case 7:
			UP = 7;
			break;
		}
	}

	*ppTS = SearchAdmitTRStream(
			ieee,
			Addr,
			UP,
			TxRxSelect);
	if (*ppTS != NULL) {
		return true;
	} else {
		if (!bAddNewTs) {
			IEEE80211_DEBUG(IEEE80211_DL_TS, "add new TS failed(tid:%d)\n", UP);
			return false;
		} else {
			
			
			
			
			
			struct tspec_body	TSpec;
			struct qos_tsinfo	*pTSInfo = &TSpec.ts_info;
			struct list_head	*pUnusedList =
								(TxRxSelect == TX_DIR) ?
								(&ieee->Tx_TS_Unused_List) :
								(&ieee->Rx_TS_Unused_List);

			struct list_head	*pAddmitList =
								(TxRxSelect == TX_DIR) ?
								(&ieee->Tx_TS_Admit_List) :
								(&ieee->Rx_TS_Admit_List);

			enum direction_value	Dir =		(ieee->iw_mode == IW_MODE_MASTER) ?
								((TxRxSelect == TX_DIR) ? DIR_DOWN : DIR_UP) :
								((TxRxSelect == TX_DIR) ? DIR_UP : DIR_DOWN);
			IEEE80211_DEBUG(IEEE80211_DL_TS, "to add Ts\n");
			if (!list_empty(pUnusedList)) {
				(*ppTS) = list_entry(pUnusedList->next, struct ts_common_info, list);
				list_del_init(&(*ppTS)->list);
				if (TxRxSelect == TX_DIR) {
					struct tx_ts_record *tmp = container_of(*ppTS, struct tx_ts_record, ts_common_info);
					ResetTxTsEntry(tmp);
				} else {
					struct rx_ts_record *tmp = container_of(*ppTS, struct rx_ts_record, ts_common_info);
					ResetRxTsEntry(tmp);
				}

				IEEE80211_DEBUG(IEEE80211_DL_TS, "to init current TS, UP:%d, Dir:%d, addr:%pM\n", UP, Dir, Addr);
				
				pTSInfo->uc_traffic_type = 0;		
				pTSInfo->uc_tsid = UP;			
				pTSInfo->uc_direction = Dir;		
				pTSInfo->uc_access_policy = 1;		
				pTSInfo->uc_aggregation = 0;		
				pTSInfo->uc_psb = 0;			
				pTSInfo->uc_up = UP;			
				pTSInfo->uc_ts_info_ack_policy = 0;	
				pTSInfo->uc_schedule = 0;		

				MakeTSEntry(*ppTS, Addr, &TSpec, NULL, 0, 0);
				AdmitTS(ieee, *ppTS, 0);
				list_add_tail(&((*ppTS)->list), pAddmitList);
				

				return true;
			} else {
				IEEE80211_DEBUG(IEEE80211_DL_ERR, "in function %s() There is not enough TS record to be used!!", __func__);
				return false;
			}
		}
	}
}

static void RemoveTsEntry(struct ieee80211_device *ieee, struct ts_common_info *pTs,
			  enum tr_select TxRxSelect)
{
	
	unsigned long flags = 0;
	del_timer_sync(&pTs->setup_timer);
	del_timer_sync(&pTs->inact_timer);
	TsInitDelBA(ieee, pTs, TxRxSelect);

	if (TxRxSelect == RX_DIR) {
		struct rx_reorder_entry	*pRxReorderEntry;
		struct rx_ts_record     *pRxTS = (struct rx_ts_record *)pTs;
		if (timer_pending(&pRxTS->rx_pkt_pending_timer))
			del_timer_sync(&pRxTS->rx_pkt_pending_timer);

		while (!list_empty(&pRxTS->rx_pending_pkt_list)) {
			spin_lock_irqsave(&(ieee->reorder_spinlock), flags);
			
			pRxReorderEntry = list_entry(pRxTS->rx_pending_pkt_list.prev, struct rx_reorder_entry, List);
			list_del_init(&pRxReorderEntry->List);
			{
				int i = 0;
				struct ieee80211_rxb *prxb = pRxReorderEntry->prxb;
				if (unlikely(!prxb)) {
					spin_unlock_irqrestore(&(ieee->reorder_spinlock), flags);
					return;
				}
				for (i =  0; i < prxb->nr_subframes; i++)
					dev_kfree_skb(prxb->subframes[i]);

				kfree(prxb);
				prxb = NULL;
			}
			list_add_tail(&pRxReorderEntry->List, &ieee->RxReorder_Unused_List);
			spin_unlock_irqrestore(&(ieee->reorder_spinlock), flags);
		}

	} else {
		struct tx_ts_record *pTxTS = (struct tx_ts_record *)pTs;
		del_timer_sync(&pTxTS->ts_add_ba_timer);
	}
}

void RemovePeerTS(struct ieee80211_device *ieee, u8 *Addr)
{
	struct ts_common_info	*pTS, *pTmpTS;

	printk("===========>%s,%pM\n", __func__, Addr);
	list_for_each_entry_safe(pTS, pTmpTS, &ieee->Tx_TS_Pending_List, list) {
		if (memcmp(pTS->addr, Addr, 6) == 0) {
			RemoveTsEntry(ieee, pTS, TX_DIR);
			list_del_init(&pTS->list);
			list_add_tail(&pTS->list, &ieee->Tx_TS_Unused_List);
		}
	}

	list_for_each_entry_safe(pTS, pTmpTS, &ieee->Tx_TS_Admit_List, list) {
		if (memcmp(pTS->addr, Addr, 6) == 0) {
			printk("====>remove Tx_TS_admin_list\n");
			RemoveTsEntry(ieee, pTS, TX_DIR);
			list_del_init(&pTS->list);
			list_add_tail(&pTS->list, &ieee->Tx_TS_Unused_List);
		}
	}

	list_for_each_entry_safe(pTS, pTmpTS, &ieee->Rx_TS_Pending_List, list) {
		if (memcmp(pTS->addr, Addr, 6) == 0) {
			RemoveTsEntry(ieee, pTS, RX_DIR);
			list_del_init(&pTS->list);
			list_add_tail(&pTS->list, &ieee->Rx_TS_Unused_List);
		}
	}

	list_for_each_entry_safe(pTS, pTmpTS, &ieee->Rx_TS_Admit_List, list) {
		if (memcmp(pTS->addr, Addr, 6) == 0) {
			RemoveTsEntry(ieee, pTS, RX_DIR);
			list_del_init(&pTS->list);
			list_add_tail(&pTS->list, &ieee->Rx_TS_Unused_List);
		}
	}
}

void RemoveAllTS(struct ieee80211_device *ieee)
{
	struct ts_common_info *pTS, *pTmpTS;

	list_for_each_entry_safe(pTS, pTmpTS, &ieee->Tx_TS_Pending_List, list) {
		RemoveTsEntry(ieee, pTS, TX_DIR);
		list_del_init(&pTS->list);
		list_add_tail(&pTS->list, &ieee->Tx_TS_Unused_List);
	}

	list_for_each_entry_safe(pTS, pTmpTS, &ieee->Tx_TS_Admit_List, list) {
		RemoveTsEntry(ieee, pTS, TX_DIR);
		list_del_init(&pTS->list);
		list_add_tail(&pTS->list, &ieee->Tx_TS_Unused_List);
	}

	list_for_each_entry_safe(pTS, pTmpTS, &ieee->Rx_TS_Pending_List, list) {
		RemoveTsEntry(ieee, pTS, RX_DIR);
		list_del_init(&pTS->list);
		list_add_tail(&pTS->list, &ieee->Rx_TS_Unused_List);
	}

	list_for_each_entry_safe(pTS, pTmpTS, &ieee->Rx_TS_Admit_List, list) {
		RemoveTsEntry(ieee, pTS, RX_DIR);
		list_del_init(&pTS->list);
		list_add_tail(&pTS->list, &ieee->Rx_TS_Unused_List);
	}
}

void TsStartAddBaProcess(struct ieee80211_device *ieee, struct tx_ts_record *pTxTS)
{
	if (!pTxTS->add_ba_req_in_progress) {
		pTxTS->add_ba_req_in_progress = true;
		if (pTxTS->add_ba_req_delayed)	{
			IEEE80211_DEBUG(IEEE80211_DL_BA, "%s: Delayed Start ADDBA after 60 sec!!\n", __func__);
			mod_timer(&pTxTS->ts_add_ba_timer,
				  jiffies + msecs_to_jiffies(TS_ADDBA_DELAY));
		} else {
			IEEE80211_DEBUG(IEEE80211_DL_BA, "%s: Immediately Start ADDBA now!!\n", __func__);
			mod_timer(&pTxTS->ts_add_ba_timer, jiffies + 10); 
		}
	} else {
		IEEE80211_DEBUG(IEEE80211_DL_ERR, "%s()==>BA timer is already added\n", __func__);
	}
}
