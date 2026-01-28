#ifndef LINUX_POWERPC_PERF_HV_24X7_CATALOG_H_
#define LINUX_POWERPC_PERF_HV_24X7_CATALOG_H_
#include <linux/types.h>
struct hv_24x7_catalog_page_0 {
#define HV_24X7_CATALOG_MAGIC 0x32347837  
	__be32 magic;
	__be32 length;  
	__be64 version;  
	__u8 build_time_stamp[16];  
	__u8 reserved2[32];
	__be16 schema_data_offs;  
	__be16 schema_data_len;   
	__be16 schema_entry_count;
	__u8 reserved3[2];
	__be16 event_data_offs;
	__be16 event_data_len;
	__be16 event_entry_count;
	__u8 reserved4[2];
	__be16 group_data_offs;  
	__be16 group_data_len;   
	__be16 group_entry_count;
	__u8 reserved5[2];
	__be16 formula_data_offs;  
	__be16 formula_data_len;   
	__be16 formula_entry_count;
	__u8 reserved6[2];
} __packed;
struct hv_24x7_event_data {
	__be16 length;  
	__u8 reserved1[2];
	__u8 domain;  
	__u8 reserved2[1];
	__be16 event_group_record_offs;  
	__be16 event_group_record_len;  
	__be16 event_counter_offs;
	__be32 flags;
	__be16 primary_group_ix;
	__be16 group_count;
	__be16 event_name_len;
	__u8 remainder[];
} __packed;
#endif
