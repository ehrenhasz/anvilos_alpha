 

#ifndef __RSI_COEX_H__
#define __RSI_COEX_H__

#include "rsi_common.h"

#ifdef CONFIG_RSI_COEX
#define COMMON_CARD_READY_IND           0
#define NUM_COEX_TX_QUEUES              5

struct rsi_coex_ctrl_block {
	struct rsi_common *priv;
	struct sk_buff_head coex_tx_qs[NUM_COEX_TX_QUEUES];
	struct rsi_thread coex_tx_thread;
};

int rsi_coex_attach(struct rsi_common *common);
void rsi_coex_detach(struct rsi_common *common);
int rsi_coex_send_pkt(void *priv, struct sk_buff *skb, u8 proto_type);
int rsi_coex_recv_pkt(struct rsi_common *common, u8 *msg);
#endif
#endif
