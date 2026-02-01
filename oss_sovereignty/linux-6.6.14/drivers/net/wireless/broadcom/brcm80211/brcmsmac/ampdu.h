 

#ifndef _BRCM_AMPDU_H_
#define _BRCM_AMPDU_H_

 
struct brcms_ampdu_session {
	struct brcms_c_info *wlc;
	struct sk_buff_head skb_list;
	unsigned max_ampdu_len;
	u16 max_ampdu_frames;
	u16 ampdu_len;
	u16 dma_len;
};

void brcms_c_ampdu_reset_session(struct brcms_ampdu_session *session,
				 struct brcms_c_info *wlc);
int brcms_c_ampdu_add_frame(struct brcms_ampdu_session *session,
			    struct sk_buff *p);
void brcms_c_ampdu_finalize(struct brcms_ampdu_session *session);

struct ampdu_info *brcms_c_ampdu_attach(struct brcms_c_info *wlc);
void brcms_c_ampdu_detach(struct ampdu_info *ampdu);
void brcms_c_ampdu_dotxstatus(struct ampdu_info *ampdu, struct scb *scb,
			      struct sk_buff *p, struct tx_status *txs);
void brcms_c_ampdu_macaddr_upd(struct brcms_c_info *wlc);
void brcms_c_ampdu_shm_upd(struct ampdu_info *ampdu);

#endif				 
