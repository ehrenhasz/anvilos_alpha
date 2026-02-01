 
 

#ifndef EF4_TX_H
#define EF4_TX_H

#include <linux/types.h>

 

unsigned int ef4_tx_limit_len(struct ef4_tx_queue *tx_queue,
			      dma_addr_t dma_addr, unsigned int len);

u8 *ef4_tx_get_copy_buffer_limited(struct ef4_tx_queue *tx_queue,
				   struct ef4_tx_buffer *buffer, size_t len);

int ef4_enqueue_skb_tso(struct ef4_tx_queue *tx_queue, struct sk_buff *skb,
			bool *data_mapped);

#endif  
