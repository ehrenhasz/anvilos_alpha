 

#include "cxgb4.h"
#include "smt.h"
#include "t4_msg.h"
#include "t4fw_api.h"
#include "t4_regs.h"
#include "t4_values.h"

struct smt_data *t4_init_smt(void)
{
	unsigned int smt_size;
	struct smt_data *s;
	int i;

	smt_size = SMT_SIZE;

	s = kvzalloc(struct_size(s, smtab, smt_size), GFP_KERNEL);
	if (!s)
		return NULL;
	s->smt_size = smt_size;
	rwlock_init(&s->lock);
	for (i = 0; i < s->smt_size; ++i) {
		s->smtab[i].idx = i;
		s->smtab[i].state = SMT_STATE_UNUSED;
		eth_zero_addr(s->smtab[i].src_mac);
		spin_lock_init(&s->smtab[i].lock);
		s->smtab[i].refcnt = 0;
	}
	return s;
}

static struct smt_entry *find_or_alloc_smte(struct smt_data *s, u8 *smac)
{
	struct smt_entry *first_free = NULL;
	struct smt_entry *e, *end;

	for (e = &s->smtab[0], end = &s->smtab[s->smt_size]; e != end; ++e) {
		if (e->refcnt == 0) {
			if (!first_free)
				first_free = e;
		} else {
			if (e->state == SMT_STATE_SWITCHING) {
				 
				if (memcmp(e->src_mac, smac, ETH_ALEN) == 0)
					goto found_reuse;
			}
		}
	}

	if (first_free) {
		e = first_free;
		goto found;
	}
	return NULL;

found:
	e->state = SMT_STATE_UNUSED;

found_reuse:
	return e;
}

static void t4_smte_free(struct smt_entry *e)
{
	if (e->refcnt == 0) {   
		e->state = SMT_STATE_UNUSED;
	}
}

 
void cxgb4_smt_release(struct smt_entry *e)
{
	spin_lock_bh(&e->lock);
	if ((--e->refcnt) == 0)
		t4_smte_free(e);
	spin_unlock_bh(&e->lock);
}
EXPORT_SYMBOL(cxgb4_smt_release);

void do_smt_write_rpl(struct adapter *adap, const struct cpl_smt_write_rpl *rpl)
{
	unsigned int smtidx = TID_TID_G(GET_TID(rpl));
	struct smt_data *s = adap->smt;

	if (unlikely(rpl->status != CPL_ERR_NONE)) {
		struct smt_entry *e = &s->smtab[smtidx];

		dev_err(adap->pdev_dev,
			"Unexpected SMT_WRITE_RPL status %u for entry %u\n",
			rpl->status, smtidx);
		spin_lock(&e->lock);
		e->state = SMT_STATE_ERROR;
		spin_unlock(&e->lock);
		return;
	}
}

static int write_smt_entry(struct adapter *adapter, struct smt_entry *e)
{
	struct cpl_t6_smt_write_req *t6req;
	struct smt_data *s = adapter->smt;
	struct cpl_smt_write_req *req;
	struct sk_buff *skb;
	int size;
	u8 row;

	if (CHELSIO_CHIP_VERSION(adapter->params.chip) <= CHELSIO_T5) {
		size = sizeof(*req);
		skb = alloc_skb(size, GFP_ATOMIC);
		if (!skb)
			return -ENOMEM;
		 
		req = (struct cpl_smt_write_req *)__skb_put(skb, size);
		INIT_TP_WR(req, 0);

		 
		row = (e->idx >> 1);
		if (e->idx & 1) {
			req->pfvf1 = 0x0;
			memcpy(req->src_mac1, e->src_mac, ETH_ALEN);

			 
			req->pfvf0 = 0x0;
			memcpy(req->src_mac0, s->smtab[e->idx - 1].src_mac,
			       ETH_ALEN);
		} else {
			req->pfvf0 = 0x0;
			memcpy(req->src_mac0, e->src_mac, ETH_ALEN);

			 
			req->pfvf1 = 0x0;
			memcpy(req->src_mac1, s->smtab[e->idx + 1].src_mac,
			       ETH_ALEN);
		}
	} else {
		size = sizeof(*t6req);
		skb = alloc_skb(size, GFP_ATOMIC);
		if (!skb)
			return -ENOMEM;
		 
		t6req = (struct cpl_t6_smt_write_req *)__skb_put(skb, size);
		INIT_TP_WR(t6req, 0);
		req = (struct cpl_smt_write_req *)t6req;

		 
		req->pfvf0 = 0x0;
		memcpy(req->src_mac0, s->smtab[e->idx].src_mac, ETH_ALEN);
		row = e->idx;
	}

	OPCODE_TID(req) =
		htonl(MK_OPCODE_TID(CPL_SMT_WRITE_REQ, e->idx |
				    TID_QID_V(adapter->sge.fw_evtq.abs_id)));
	req->params = htonl(SMTW_NORPL_V(0) |
			    SMTW_IDX_V(row) |
			    SMTW_OVLAN_IDX_V(0));
	t4_mgmt_tx(adapter, skb);
	return 0;
}

static struct smt_entry *t4_smt_alloc_switching(struct adapter *adap, u16 pfvf,
						u8 *smac)
{
	struct smt_data *s = adap->smt;
	struct smt_entry *e;

	write_lock_bh(&s->lock);
	e = find_or_alloc_smte(s, smac);
	if (e) {
		spin_lock(&e->lock);
		if (!e->refcnt) {
			e->refcnt = 1;
			e->state = SMT_STATE_SWITCHING;
			e->pfvf = pfvf;
			memcpy(e->src_mac, smac, ETH_ALEN);
			write_smt_entry(adap, e);
		} else {
			++e->refcnt;
		}
		spin_unlock(&e->lock);
	}
	write_unlock_bh(&s->lock);
	return e;
}

 
struct smt_entry *cxgb4_smt_alloc_switching(struct net_device *dev, u8 *smac)
{
	struct adapter *adap = netdev2adap(dev);

	return t4_smt_alloc_switching(adap, 0x0, smac);
}
EXPORT_SYMBOL(cxgb4_smt_alloc_switching);
