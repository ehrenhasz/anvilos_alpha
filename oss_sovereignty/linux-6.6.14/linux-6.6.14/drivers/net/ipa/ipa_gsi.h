#ifndef _IPA_GSI_TRANS_H_
#define _IPA_GSI_TRANS_H_
#include <linux/types.h>
struct gsi;
struct gsi_trans;
struct ipa_gsi_endpoint_data;
void ipa_gsi_trans_complete(struct gsi_trans *trans);
void ipa_gsi_trans_release(struct gsi_trans *trans);
void ipa_gsi_channel_tx_queued(struct gsi *gsi, u32 channel_id, u32 count,
			       u32 byte_count);
void ipa_gsi_channel_tx_completed(struct gsi *gsi, u32 channel_id, u32 count,
				  u32 byte_count);
bool ipa_gsi_endpoint_data_empty(const struct ipa_gsi_endpoint_data *data);
#endif  
