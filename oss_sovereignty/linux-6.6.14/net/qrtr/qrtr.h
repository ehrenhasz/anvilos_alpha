#ifndef __QRTR_H_
#define __QRTR_H_
#include <linux/types.h>
struct sk_buff;
#define QRTR_EP_NID_AUTO (-1)
struct qrtr_endpoint {
	int (*xmit)(struct qrtr_endpoint *ep, struct sk_buff *skb);
	struct qrtr_node *node;
};
int qrtr_endpoint_register(struct qrtr_endpoint *ep, unsigned int nid);
void qrtr_endpoint_unregister(struct qrtr_endpoint *ep);
int qrtr_endpoint_post(struct qrtr_endpoint *ep, const void *data, size_t len);
int qrtr_ns_init(void);
void qrtr_ns_remove(void);
#endif
