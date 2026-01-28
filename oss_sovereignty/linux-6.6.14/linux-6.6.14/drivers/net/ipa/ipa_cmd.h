#ifndef _IPA_CMD_H_
#define _IPA_CMD_H_
#include <linux/types.h>
#include <linux/dma-direction.h>
struct sk_buff;
struct scatterlist;
struct ipa;
struct ipa_mem;
struct gsi_trans;
struct gsi_channel;
enum ipa_cmd_opcode {
	IPA_CMD_NONE			= 0x0,
	IPA_CMD_IP_V4_FILTER_INIT	= 0x3,
	IPA_CMD_IP_V6_FILTER_INIT	= 0x4,
	IPA_CMD_IP_V4_ROUTING_INIT	= 0x7,
	IPA_CMD_IP_V6_ROUTING_INIT	= 0x8,
	IPA_CMD_HDR_INIT_LOCAL		= 0x9,
	IPA_CMD_REGISTER_WRITE		= 0xc,
	IPA_CMD_IP_PACKET_INIT		= 0x10,
	IPA_CMD_DMA_SHARED_MEM		= 0x13,
	IPA_CMD_IP_PACKET_TAG_STATUS	= 0x14,
};
bool ipa_cmd_table_init_valid(struct ipa *ipa, const struct ipa_mem *mem,
			      bool route);
bool ipa_cmd_data_valid(struct ipa *ipa);
int ipa_cmd_pool_init(struct gsi_channel *channel, u32 tre_count);
void ipa_cmd_pool_exit(struct gsi_channel *channel);
void ipa_cmd_table_init_add(struct gsi_trans *trans, enum ipa_cmd_opcode opcode,
			    u16 size, u32 offset, dma_addr_t addr,
			    u16 hash_size, u32 hash_offset,
			    dma_addr_t hash_addr);
void ipa_cmd_hdr_init_local_add(struct gsi_trans *trans, u32 offset, u16 size,
				dma_addr_t addr);
void ipa_cmd_register_write_add(struct gsi_trans *trans, u32 offset, u32 value,
				u32 mask, bool clear_full);
void ipa_cmd_dma_shared_mem_add(struct gsi_trans *trans, u32 offset,
				u16 size, dma_addr_t addr, bool toward_ipa);
void ipa_cmd_pipeline_clear_add(struct gsi_trans *trans);
u32 ipa_cmd_pipeline_clear_count(void);
void ipa_cmd_pipeline_clear_wait(struct ipa *ipa);
struct gsi_trans *ipa_cmd_trans_alloc(struct ipa *ipa, u32 tre_count);
int ipa_cmd_init(struct ipa *ipa);
#endif  
