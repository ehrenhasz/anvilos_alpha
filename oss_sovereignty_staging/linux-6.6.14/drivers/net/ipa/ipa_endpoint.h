 

 
#ifndef _IPA_ENDPOINT_H_
#define _IPA_ENDPOINT_H_

#include <linux/types.h>
#include <linux/workqueue.h>
#include <linux/if_ether.h>

#include "gsi.h"
#include "ipa_reg.h"

struct net_device;
struct sk_buff;

struct ipa;
struct ipa_gsi_endpoint_data;

 
#define IPA_AGGR_GRANULARITY		500	 

#define IPA_MTU			ETH_DATA_LEN

enum ipa_endpoint_name {
	IPA_ENDPOINT_AP_COMMAND_TX,
	IPA_ENDPOINT_AP_LAN_RX,
	IPA_ENDPOINT_AP_MODEM_TX,
	IPA_ENDPOINT_AP_MODEM_RX,
	IPA_ENDPOINT_MODEM_COMMAND_TX,
	IPA_ENDPOINT_MODEM_LAN_TX,
	IPA_ENDPOINT_MODEM_LAN_RX,
	IPA_ENDPOINT_MODEM_AP_TX,
	IPA_ENDPOINT_MODEM_AP_RX,
	IPA_ENDPOINT_MODEM_DL_NLO_TX,
	IPA_ENDPOINT_COUNT,	 
};

#define IPA_ENDPOINT_MAX		36	 

 
struct ipa_endpoint_tx {
	enum ipa_seq_type seq_type;
	enum ipa_seq_rep_type seq_rep_type;
	enum ipa_endpoint_name status_endpoint;
};

 
struct ipa_endpoint_rx {
	u32 buffer_size;
	u32 pad_align;
	u32 aggr_time_limit;
	bool aggr_hard_limit;
	bool aggr_close_eof;
	bool holb_drop;
};

 
struct ipa_endpoint_config {
	u32 resource_group;
	bool checksum;
	bool qmap;
	bool aggregation;
	bool status_enable;
	bool dma_mode;
	enum ipa_endpoint_name dma_endpoint;
	union {
		struct ipa_endpoint_tx tx;
		struct ipa_endpoint_rx rx;
	};
};

 
enum ipa_replenish_flag {
	IPA_REPLENISH_ENABLED,
	IPA_REPLENISH_ACTIVE,
	IPA_REPLENISH_COUNT,	 
};

 
struct ipa_endpoint {
	struct ipa *ipa;
	enum gsi_ee_id ee_id;
	u32 channel_id;
	u32 endpoint_id;
	bool toward_ipa;
	struct ipa_endpoint_config config;

	u32 skb_frag_max;	 
	u32 evt_ring_id;

	 
	struct net_device *netdev;

	 
	DECLARE_BITMAP(replenish_flags, IPA_REPLENISH_COUNT);
	u64 replenish_count;
	struct delayed_work replenish_work;		 
};

void ipa_endpoint_modem_hol_block_clear_all(struct ipa *ipa);

void ipa_endpoint_modem_pause_all(struct ipa *ipa, bool enable);

int ipa_endpoint_modem_exception_reset_all(struct ipa *ipa);

int ipa_endpoint_skb_tx(struct ipa_endpoint *endpoint, struct sk_buff *skb);

int ipa_endpoint_enable_one(struct ipa_endpoint *endpoint);
void ipa_endpoint_disable_one(struct ipa_endpoint *endpoint);

void ipa_endpoint_suspend_one(struct ipa_endpoint *endpoint);
void ipa_endpoint_resume_one(struct ipa_endpoint *endpoint);

void ipa_endpoint_suspend(struct ipa *ipa);
void ipa_endpoint_resume(struct ipa *ipa);

void ipa_endpoint_setup(struct ipa *ipa);
void ipa_endpoint_teardown(struct ipa *ipa);

int ipa_endpoint_config(struct ipa *ipa);
void ipa_endpoint_deconfig(struct ipa *ipa);

void ipa_endpoint_default_route_set(struct ipa *ipa, u32 endpoint_id);
void ipa_endpoint_default_route_clear(struct ipa *ipa);

int ipa_endpoint_init(struct ipa *ipa, u32 count,
		      const struct ipa_gsi_endpoint_data *data);
void ipa_endpoint_exit(struct ipa *ipa);

void ipa_endpoint_trans_complete(struct ipa_endpoint *ipa,
				 struct gsi_trans *trans);
void ipa_endpoint_trans_release(struct ipa_endpoint *ipa,
				struct gsi_trans *trans);

#endif  
