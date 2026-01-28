


#ifndef __HCLGE_COMM_TQP_STATS_H
#define __HCLGE_COMM_TQP_STATS_H
#include <linux/types.h>
#include <linux/etherdevice.h>
#include "hnae3.h"


#define HCLGE_COMM_QUEUE_PAIR_SIZE 2


struct hclge_comm_tqp_stats {
	
	u64 rcb_tx_ring_pktnum_rcd; 
	
	u64 rcb_rx_ring_pktnum_rcd; 
};

struct hclge_comm_tqp {
	
	struct device *dev;
	struct hnae3_queue q;
	struct hclge_comm_tqp_stats tqp_stats;
	u16 index;	

	bool alloced;
};

u64 *hclge_comm_tqps_get_stats(struct hnae3_handle *handle, u64 *data);
int hclge_comm_tqps_get_sset_count(struct hnae3_handle *handle);
u8 *hclge_comm_tqps_get_strings(struct hnae3_handle *handle, u8 *data);
void hclge_comm_reset_tqp_stats(struct hnae3_handle *handle);
int hclge_comm_tqps_update_stats(struct hnae3_handle *handle,
				 struct hclge_comm_hw *hw);
#endif
