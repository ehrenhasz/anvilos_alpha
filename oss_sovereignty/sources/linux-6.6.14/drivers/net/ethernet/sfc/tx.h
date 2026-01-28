


#ifndef EFX_TX_H
#define EFX_TX_H

#include <linux/types.h>



unsigned int efx_tx_limit_len(struct efx_tx_queue *tx_queue,
			      dma_addr_t dma_addr, unsigned int len);

u8 *efx_tx_get_copy_buffer_limited(struct efx_tx_queue *tx_queue,
				   struct efx_tx_buffer *buffer, size_t len);


static inline unsigned int efx_tx_csum_type_skb(struct sk_buff *skb)
{
	if (skb->ip_summed != CHECKSUM_PARTIAL)
		return 0; 

	if (skb->encapsulation &&
	    skb_checksum_start_offset(skb) == skb_inner_transport_offset(skb)) {
		

		
		if (skb_shinfo(skb)->gso_segs > 1 &&
		    !(skb_shinfo(skb)->gso_type & SKB_GSO_PARTIAL) &&
		    (skb_shinfo(skb)->gso_type & SKB_GSO_UDP_TUNNEL_CSUM))
			return EFX_TXQ_TYPE_OUTER_CSUM | EFX_TXQ_TYPE_INNER_CSUM;
		return EFX_TXQ_TYPE_INNER_CSUM;
	}

	
	return EFX_TXQ_TYPE_OUTER_CSUM;
}
#endif 
