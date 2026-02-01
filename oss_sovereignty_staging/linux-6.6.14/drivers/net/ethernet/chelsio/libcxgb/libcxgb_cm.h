 

#ifndef __LIBCXGB_CM_H__
#define __LIBCXGB_CM_H__


#include <net/tcp.h>

#include <cxgb4.h>
#include <t4_msg.h>
#include <l2t.h>

void
cxgb_get_4tuple(struct cpl_pass_accept_req *, enum chip_type,
		int *, __u8 *, __u8 *, __be16 *, __be16 *);
struct dst_entry *
cxgb_find_route(struct cxgb4_lld_info *,
		struct net_device *(*)(struct net_device *),
		__be32, __be32, __be16,	__be16, u8);
struct dst_entry *
cxgb_find_route6(struct cxgb4_lld_info *,
		 struct net_device *(*)(struct net_device *),
		 __u8 *, __u8 *, __be16, __be16, u8, __u32);

 
static inline bool cxgb_is_neg_adv(unsigned int status)
{
	return status == CPL_ERR_RTX_NEG_ADVICE ||
	       status == CPL_ERR_PERSIST_NEG_ADVICE ||
	       status == CPL_ERR_KEEPALV_NEG_ADVICE;
}

static inline void
cxgb_best_mtu(const unsigned short *mtus, unsigned short mtu,
	      unsigned int *idx, int use_ts, int ipv6)
{
	unsigned short hdr_size = (ipv6 ?
				   sizeof(struct ipv6hdr) :
				   sizeof(struct iphdr)) +
				  sizeof(struct tcphdr) +
				  (use_ts ?
				   round_up(TCPOLEN_TIMESTAMP, 4) : 0);
	unsigned short data_size = mtu - hdr_size;

	cxgb4_best_aligned_mtu(mtus, hdr_size, data_size, 8, idx);
}

static inline u32 cxgb_compute_wscale(u32 win)
{
	u32 wscale = 0;

	while (wscale < 14 && (65535 << wscale) < win)
		wscale++;
	return wscale;
}

static inline void
cxgb_mk_tid_release(struct sk_buff *skb, u32 len, u32 tid, u16 chan)
{
	struct cpl_tid_release *req;

	req = __skb_put_zero(skb, len);

	INIT_TP_WR(req, tid);
	OPCODE_TID(req) = cpu_to_be32(MK_OPCODE_TID(CPL_TID_RELEASE, tid));
	set_wr_txq(skb, CPL_PRIORITY_SETUP, chan);
}

static inline void
cxgb_mk_close_con_req(struct sk_buff *skb, u32 len, u32 tid, u16 chan,
		      void *handle, arp_err_handler_t handler)
{
	struct cpl_close_con_req *req;

	req = __skb_put_zero(skb, len);

	INIT_TP_WR(req, tid);
	OPCODE_TID(req) = cpu_to_be32(MK_OPCODE_TID(CPL_CLOSE_CON_REQ, tid));
	set_wr_txq(skb, CPL_PRIORITY_DATA, chan);
	t4_set_arp_err_handler(skb, handle, handler);
}

static inline void
cxgb_mk_abort_req(struct sk_buff *skb, u32 len, u32 tid, u16 chan,
		  void *handle, arp_err_handler_t handler)
{
	struct cpl_abort_req *req;

	req = __skb_put_zero(skb, len);

	INIT_TP_WR(req, tid);
	OPCODE_TID(req) = cpu_to_be32(MK_OPCODE_TID(CPL_ABORT_REQ, tid));
	req->cmd = CPL_ABORT_SEND_RST;
	set_wr_txq(skb, CPL_PRIORITY_DATA, chan);
	t4_set_arp_err_handler(skb, handle, handler);
}

static inline void
cxgb_mk_abort_rpl(struct sk_buff *skb, u32 len, u32 tid, u16 chan)
{
	struct cpl_abort_rpl *rpl;

	rpl = __skb_put_zero(skb, len);

	INIT_TP_WR(rpl, tid);
	OPCODE_TID(rpl) = cpu_to_be32(MK_OPCODE_TID(CPL_ABORT_RPL, tid));
	rpl->cmd = CPL_ABORT_NO_RST;
	set_wr_txq(skb, CPL_PRIORITY_DATA, chan);
}

static inline void
cxgb_mk_rx_data_ack(struct sk_buff *skb, u32 len, u32 tid, u16 chan,
		    u32 credit_dack)
{
	struct cpl_rx_data_ack *req;

	req = __skb_put_zero(skb, len);

	INIT_TP_WR(req, tid);
	OPCODE_TID(req) = cpu_to_be32(MK_OPCODE_TID(CPL_RX_DATA_ACK, tid));
	req->credit_dack = cpu_to_be32(credit_dack);
	set_wr_txq(skb, CPL_PRIORITY_ACK, chan);
}
#endif
