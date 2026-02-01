 

 
#ifndef _IPA_DATA_H_
#define _IPA_DATA_H_

#include <linux/types.h>

#include "ipa_version.h"
#include "ipa_endpoint.h"
#include "ipa_mem.h"

 

 
#define IPA_RESOURCE_GROUP_MAX	8

 
enum ipa_qsb_master_id {
	IPA_QSB_MASTER_DDR,
	IPA_QSB_MASTER_PCIE,
};

 
struct ipa_qsb_data {
	u8 max_writes;
	u8 max_reads;
	u8 max_reads_beats;		 
};

 
struct gsi_channel_data {
	u16 tre_count;			 
	u16 event_count;		 
	u8 tlv_count;
};

 
struct ipa_endpoint_data {
	bool filter_support;
	struct ipa_endpoint_config config;
};

 
struct ipa_gsi_endpoint_data {
	u8 ee_id;		 
	u8 channel_id;
	u8 endpoint_id;
	bool toward_ipa;

	struct gsi_channel_data channel;
	struct ipa_endpoint_data endpoint;
};

 
struct ipa_resource_limits {
	u32 min;
	u32 max;
};

 
struct ipa_resource {
	struct ipa_resource_limits limits[IPA_RESOURCE_GROUP_MAX];
};

 
struct ipa_resource_data {
	u32 rsrc_group_src_count;
	u32 rsrc_group_dst_count;
	u32 resource_src_count;
	const struct ipa_resource *resource_src;
	u32 resource_dst_count;
	const struct ipa_resource *resource_dst;
};

 
struct ipa_mem_data {
	u32 local_count;
	const struct ipa_mem *local;
	u32 imem_addr;
	u32 imem_size;
	u32 smem_id;
	u32 smem_size;
};

 
struct ipa_interconnect_data {
	const char *name;
	u32 peak_bandwidth;
	u32 average_bandwidth;
};

 
struct ipa_power_data {
	u32 core_clock_rate;
	u32 interconnect_count;		 
	const struct ipa_interconnect_data *interconnect_data;
};

 
struct ipa_data {
	enum ipa_version version;
	u32 backward_compat;
	u32 qsb_count;		 
	const struct ipa_qsb_data *qsb_data;
	u32 modem_route_count;
	u32 endpoint_count;	 
	const struct ipa_gsi_endpoint_data *endpoint_data;
	const struct ipa_resource_data *resource_data;
	const struct ipa_mem_data *mem_data;
	const struct ipa_power_data *power_data;
};

extern const struct ipa_data ipa_data_v3_1;
extern const struct ipa_data ipa_data_v3_5_1;
extern const struct ipa_data ipa_data_v4_2;
extern const struct ipa_data ipa_data_v4_5;
extern const struct ipa_data ipa_data_v4_7;
extern const struct ipa_data ipa_data_v4_9;
extern const struct ipa_data ipa_data_v4_11;
extern const struct ipa_data ipa_data_v5_0;

#endif  
