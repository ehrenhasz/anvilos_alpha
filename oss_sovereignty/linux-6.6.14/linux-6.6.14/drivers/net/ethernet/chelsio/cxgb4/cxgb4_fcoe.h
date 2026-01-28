#ifndef __CXGB4_FCOE_H__
#define __CXGB4_FCOE_H__
#ifdef CONFIG_CHELSIO_T4_FCOE
#define CXGB_FCOE_TXPKT_CSUM_START	28
#define CXGB_FCOE_TXPKT_CSUM_END	8
enum {
	CXGB_FCOE_ENABLED     = (1 << 0),
};
struct cxgb_fcoe {
	u8	flags;
};
int cxgb_fcoe_enable(struct net_device *);
int cxgb_fcoe_disable(struct net_device *);
bool cxgb_fcoe_sof_eof_supported(struct adapter *, struct sk_buff *);
#endif  
#endif  
