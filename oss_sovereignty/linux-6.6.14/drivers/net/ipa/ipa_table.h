#ifndef _IPA_TABLE_H_
#define _IPA_TABLE_H_
#include <linux/types.h>
struct ipa;
bool ipa_filtered_valid(struct ipa *ipa, u64 filtered);
static inline bool ipa_table_hash_support(struct ipa *ipa)
{
	return ipa->version != IPA_VERSION_4_2;
}
void ipa_table_reset(struct ipa *ipa, bool modem);
int ipa_table_hash_flush(struct ipa *ipa);
int ipa_table_setup(struct ipa *ipa);
void ipa_table_config(struct ipa *ipa);
int ipa_table_init(struct ipa *ipa);
void ipa_table_exit(struct ipa *ipa);
bool ipa_table_mem_valid(struct ipa *ipa, bool filter);
#endif  
