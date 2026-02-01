 

 
#ifndef _IPA_MEM_H_
#define _IPA_MEM_H_

struct ipa;
struct ipa_mem_data;

 

 
#define IPA_MEM_MAX	(2 * PAGE_SIZE)

 
enum ipa_mem_id {
	IPA_MEM_UC_SHARED,		 
	IPA_MEM_UC_INFO,		 
	IPA_MEM_V4_FILTER_HASHED,	 
	IPA_MEM_V4_FILTER,		 
	IPA_MEM_V6_FILTER_HASHED,	 
	IPA_MEM_V6_FILTER,		 
	IPA_MEM_V4_ROUTE_HASHED,	 
	IPA_MEM_V4_ROUTE,		 
	IPA_MEM_V6_ROUTE_HASHED,	 
	IPA_MEM_V6_ROUTE,		 
	IPA_MEM_MODEM_HEADER,		 
	IPA_MEM_AP_HEADER,		 
	IPA_MEM_MODEM_PROC_CTX,		 
	IPA_MEM_AP_PROC_CTX,		 
	IPA_MEM_MODEM,			 
	IPA_MEM_UC_EVENT_RING,		 
	IPA_MEM_PDN_CONFIG,		 
	IPA_MEM_STATS_QUOTA_MODEM,	 
	IPA_MEM_STATS_QUOTA_AP,		 
	IPA_MEM_STATS_TETHERING,	 
	IPA_MEM_STATS_DROP,		 
	 
	IPA_MEM_STATS_V4_FILTER,	 
	IPA_MEM_STATS_V6_FILTER,	 
	IPA_MEM_STATS_V4_ROUTE,		 
	IPA_MEM_STATS_V6_ROUTE,		 
	IPA_MEM_AP_V4_FILTER,		 
	IPA_MEM_AP_V6_FILTER,		 
	IPA_MEM_STATS_FILTER_ROUTE,	 
	IPA_MEM_NAT_TABLE,		 
	IPA_MEM_END_MARKER,		 
	IPA_MEM_COUNT,			 
};

 
struct ipa_mem {
	enum ipa_mem_id id;
	u32 offset;
	u16 size;
	u16 canary_count;
};

const struct ipa_mem *ipa_mem_find(struct ipa *ipa, enum ipa_mem_id mem_id);

int ipa_mem_config(struct ipa *ipa);
void ipa_mem_deconfig(struct ipa *ipa);

int ipa_mem_setup(struct ipa *ipa);	 

int ipa_mem_zero_modem(struct ipa *ipa);

int ipa_mem_init(struct ipa *ipa, const struct ipa_mem_data *mem_data);
void ipa_mem_exit(struct ipa *ipa);

#endif  
