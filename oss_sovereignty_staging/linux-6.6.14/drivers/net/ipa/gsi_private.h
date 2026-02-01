 

 
#ifndef _GSI_PRIVATE_H_
#define _GSI_PRIVATE_H_

 

#include <linux/types.h>

struct gsi_trans;
struct gsi_ring;
struct gsi_channel;

#define GSI_RING_ELEMENT_SIZE	16	 

 
void gsi_trans_move_complete(struct gsi_trans *trans);

 
void gsi_trans_move_polled(struct gsi_trans *trans);

 
void gsi_trans_complete(struct gsi_trans *trans);

 
struct gsi_trans *gsi_channel_trans_mapped(struct gsi_channel *channel,
					   u32 index);

 
struct gsi_trans *gsi_channel_trans_complete(struct gsi_channel *channel);

 
void gsi_channel_trans_cancel_pending(struct gsi_channel *channel);

 
int gsi_channel_trans_init(struct gsi *gsi, u32 channel_id);

 
void gsi_channel_trans_exit(struct gsi_channel *channel);

 
void gsi_channel_doorbell(struct gsi_channel *channel);

 
void gsi_channel_update(struct gsi_channel *channel);

 
void *gsi_ring_virt(struct gsi_ring *ring, u32 index);

 
void gsi_trans_tx_committed(struct gsi_trans *trans);

 
void gsi_trans_tx_queued(struct gsi_trans *trans);

#endif  
