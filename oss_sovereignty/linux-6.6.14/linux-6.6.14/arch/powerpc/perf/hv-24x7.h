#ifndef LINUX_POWERPC_PERF_HV_24X7_H_
#define LINUX_POWERPC_PERF_HV_24X7_H_
#include <linux/types.h>
enum hv_perf_domains {
#define DOMAIN(n, v, x, c) HV_PERF_DOMAIN_##n = v,
#include "hv-24x7-domains.h"
#undef DOMAIN
	HV_PERF_DOMAIN_MAX,
};
#define H24x7_REQUEST_SIZE(iface_version)	(iface_version == 1 ? 16 : 32)
struct hv_24x7_request {
	__u8 performance_domain;
	__u8 reserved[0x1];
	__be16 data_size;
	__be32 data_offset;
	__be16 starting_lpar_ix;
	__be16 max_num_lpars;
	__be16 starting_ix;
	__be16 max_ix;
	__u8 starting_thread_group_ix;
	__u8 max_num_thread_groups;
	__u8 reserved2[0xE];
} __packed;
struct hv_24x7_request_buffer {
	__u8 interface_version;
	__u8 num_requests;
	__u8 reserved[0xE];
	struct hv_24x7_request requests[];
} __packed;
struct hv_24x7_result_element_v1 {
	__be16 lpar_ix;
	__be16 domain_ix;
	__be32 lpar_cfg_instance_id;
	__u64 element_data[];
} __packed;
struct hv_24x7_result_element_v2 {
	__be16 lpar_ix;
	__be16 domain_ix;
	__be32 lpar_cfg_instance_id;
	__u8 thread_group_ix;
	__u8 reserved[7];
	__u64 element_data[];
} __packed;
struct hv_24x7_result {
	__u8 result_ix;
	__u8 results_complete;
	__be16 num_elements_returned;
	__be16 result_element_data_size;
	__u8 reserved[0x2];
	char elements[];
} __packed;
struct hv_24x7_data_result_buffer {
	__u8 interface_version;
	__u8 num_results;
	__u8 reserved[0x1];
	__u8 failing_request_ix;
	__be32 detailed_rc;
	__be64 cec_cfg_instance_id;
	__be64 catalog_version_num;
	__u8 reserved2[0x8];
	struct hv_24x7_result results[];  
} __packed;
#endif
