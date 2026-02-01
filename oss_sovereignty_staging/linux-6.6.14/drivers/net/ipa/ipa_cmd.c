

 

#include <linux/types.h>
#include <linux/device.h>
#include <linux/slab.h>
#include <linux/bitfield.h>
#include <linux/dma-direction.h>

#include "gsi.h"
#include "gsi_trans.h"
#include "ipa.h"
#include "ipa_endpoint.h"
#include "ipa_table.h"
#include "ipa_cmd.h"
#include "ipa_mem.h"

 

 
enum pipeline_clear_options {
	pipeline_clear_hps		= 0x0,
	pipeline_clear_src_grp		= 0x1,
	pipeline_clear_full		= 0x2,
};

 

struct ipa_cmd_hw_ip_fltrt_init {
	__le64 hash_rules_addr;
	__le64 flags;
	__le64 nhash_rules_addr;
};

 
#define IP_FLTRT_FLAGS_HASH_SIZE_FMASK			GENMASK_ULL(11, 0)
#define IP_FLTRT_FLAGS_HASH_ADDR_FMASK			GENMASK_ULL(27, 12)
#define IP_FLTRT_FLAGS_NHASH_SIZE_FMASK			GENMASK_ULL(39, 28)
#define IP_FLTRT_FLAGS_NHASH_ADDR_FMASK			GENMASK_ULL(55, 40)

 

struct ipa_cmd_hw_hdr_init_local {
	__le64 hdr_table_addr;
	__le32 flags;
	__le32 reserved;
};

 
#define HDR_INIT_LOCAL_FLAGS_TABLE_SIZE_FMASK		GENMASK(11, 0)
#define HDR_INIT_LOCAL_FLAGS_HDR_ADDR_FMASK		GENMASK(27, 12)

 

 
#define REGISTER_WRITE_OPCODE_SKIP_CLEAR_FMASK		GENMASK(8, 8)
#define REGISTER_WRITE_OPCODE_CLEAR_OPTION_FMASK	GENMASK(10, 9)

struct ipa_cmd_register_write {
	__le16 flags;		 
	__le16 offset;
	__le32 value;
	__le32 value_mask;
	__le32 clear_options;	 
};

 
 
#define REGISTER_WRITE_FLAGS_OFFSET_HIGH_FMASK		GENMASK(14, 11)
 
#define REGISTER_WRITE_FLAGS_SKIP_CLEAR_FMASK		GENMASK(15, 15)

 
#define REGISTER_WRITE_CLEAR_OPTIONS_FMASK		GENMASK(1, 0)

 

struct ipa_cmd_ip_packet_init {
	u8 dest_endpoint;	 
	u8 reserved[7];
};

 
#define IPA_PACKET_INIT_DEST_ENDPOINT_FMASK		GENMASK(4, 0)

 

 

#define DMA_SHARED_MEM_OPCODE_SKIP_CLEAR_FMASK		GENMASK(8, 8)
#define DMA_SHARED_MEM_OPCODE_CLEAR_OPTION_FMASK	GENMASK(10, 9)

struct ipa_cmd_hw_dma_mem_mem {
	__le16 clear_after_read;  
	__le16 size;
	__le16 local_addr;
	__le16 flags;
	__le64 system_addr;
};

 
#define DMA_SHARED_MEM_CLEAR_AFTER_READ			GENMASK(15, 15)

 
#define DMA_SHARED_MEM_FLAGS_DIRECTION_FMASK		GENMASK(0, 0)
 
#define DMA_SHARED_MEM_FLAGS_SKIP_CLEAR_FMASK		GENMASK(1, 1)
#define DMA_SHARED_MEM_FLAGS_CLEAR_OPTIONS_FMASK	GENMASK(3, 2)

 

struct ipa_cmd_ip_packet_tag_status {
	__le64 tag;
};

#define IP_PACKET_TAG_STATUS_TAG_FMASK			GENMASK_ULL(63, 16)

 
union ipa_cmd_payload {
	struct ipa_cmd_hw_ip_fltrt_init table_init;
	struct ipa_cmd_hw_hdr_init_local hdr_init_local;
	struct ipa_cmd_register_write register_write;
	struct ipa_cmd_ip_packet_init ip_packet_init;
	struct ipa_cmd_hw_dma_mem_mem dma_shared_mem;
	struct ipa_cmd_ip_packet_tag_status ip_packet_tag_status;
};

static void ipa_cmd_validate_build(void)
{
	 
	 
	BUILD_BUG_ON(field_max(IP_FLTRT_FLAGS_HASH_SIZE_FMASK) !=
		     field_max(IP_FLTRT_FLAGS_NHASH_SIZE_FMASK));
	BUILD_BUG_ON(field_max(IP_FLTRT_FLAGS_HASH_ADDR_FMASK) !=
		     field_max(IP_FLTRT_FLAGS_NHASH_ADDR_FMASK));

	 
	BUILD_BUG_ON(IPA_ENDPOINT_MAX - 1 > U8_MAX);
}

 
bool ipa_cmd_table_init_valid(struct ipa *ipa, const struct ipa_mem *mem,
			      bool route)
{
	u32 offset_max = field_max(IP_FLTRT_FLAGS_NHASH_ADDR_FMASK);
	u32 size_max = field_max(IP_FLTRT_FLAGS_NHASH_SIZE_FMASK);
	const char *table = route ? "route" : "filter";
	struct device *dev = &ipa->pdev->dev;
	u32 size;

	size = route ? ipa->route_count : ipa->filter_count + 1;
	size *= sizeof(__le64);

	 
	if (size > size_max) {
		dev_err(dev, "%s table region size too large\n", table);
		dev_err(dev, "    (0x%04x > 0x%04x)\n", size, size_max);

		return false;
	}

	 
	if (mem->offset > offset_max ||
	    ipa->mem_offset > offset_max - mem->offset) {
		dev_err(dev, "%s table region offset too large\n", table);
		dev_err(dev, "    (0x%04x + 0x%04x > 0x%04x)\n",
			ipa->mem_offset, mem->offset, offset_max);

		return false;
	}

	return true;
}

 
static bool ipa_cmd_header_init_local_valid(struct ipa *ipa)
{
	struct device *dev = &ipa->pdev->dev;
	const struct ipa_mem *mem;
	u32 offset_max;
	u32 size_max;
	u32 offset;
	u32 size;

	 
	offset_max = field_max(HDR_INIT_LOCAL_FLAGS_HDR_ADDR_FMASK);
	size_max = field_max(HDR_INIT_LOCAL_FLAGS_TABLE_SIZE_FMASK);

	 
	mem = ipa_mem_find(ipa, IPA_MEM_MODEM_HEADER);
	offset = mem->offset;
	size = mem->size;

	 
	if (offset > offset_max || ipa->mem_offset > offset_max - offset) {
		dev_err(dev, "header table region offset too large\n");
		dev_err(dev, "    (0x%04x + 0x%04x > 0x%04x)\n",
			ipa->mem_offset, offset, offset_max);

		return false;
	}

	 
	mem = ipa_mem_find(ipa, IPA_MEM_AP_HEADER);
	if (mem)
		size += mem->size;

	 
	if (size > size_max) {
		dev_err(dev, "header table region size too large\n");
		dev_err(dev, "    (0x%04x > 0x%08x)\n", size, size_max);

		return false;
	}

	return true;
}

 
static bool ipa_cmd_register_write_offset_valid(struct ipa *ipa,
						const char *name, u32 offset)
{
	struct ipa_cmd_register_write *payload;
	struct device *dev = &ipa->pdev->dev;
	u32 offset_max;
	u32 bit_count;

	 
	bit_count = BITS_PER_BYTE * sizeof(payload->offset);
	if (ipa->version >= IPA_VERSION_4_0)
		bit_count += hweight32(REGISTER_WRITE_FLAGS_OFFSET_HIGH_FMASK);
	BUILD_BUG_ON(bit_count > 32);
	offset_max = ~0U >> (32 - bit_count);

	 
	if (offset > offset_max || ipa->mem_offset > offset_max - offset) {
		dev_err(dev, "%s offset too large 0x%04x + 0x%04x > 0x%04x)\n",
			name, ipa->mem_offset, offset, offset_max);
		return false;
	}

	return true;
}

 
static bool ipa_cmd_register_write_valid(struct ipa *ipa)
{
	const struct reg *reg;
	const char *name;
	u32 offset;

	 
	if (ipa_table_hash_support(ipa)) {
		if (ipa->version < IPA_VERSION_5_0)
			reg = ipa_reg(ipa, FILT_ROUT_HASH_FLUSH);
		else
			reg = ipa_reg(ipa, FILT_ROUT_CACHE_FLUSH);

		offset = reg_offset(reg);
		name = "filter/route hash flush";
		if (!ipa_cmd_register_write_offset_valid(ipa, name, offset))
			return false;
	}

	 
	reg = ipa_reg(ipa, ENDP_STATUS);
	offset = reg_n_offset(reg, IPA_ENDPOINT_COUNT - 1);
	name = "maximal endpoint status";
	if (!ipa_cmd_register_write_offset_valid(ipa, name, offset))
		return false;

	return true;
}

int ipa_cmd_pool_init(struct gsi_channel *channel, u32 tre_max)
{
	struct gsi_trans_info *trans_info = &channel->trans_info;
	struct device *dev = channel->gsi->dev;

	 
	return gsi_trans_pool_init_dma(dev, &trans_info->cmd_pool,
				       sizeof(union ipa_cmd_payload),
				       tre_max, channel->trans_tre_max);
}

void ipa_cmd_pool_exit(struct gsi_channel *channel)
{
	struct gsi_trans_info *trans_info = &channel->trans_info;
	struct device *dev = channel->gsi->dev;

	gsi_trans_pool_exit_dma(dev, &trans_info->cmd_pool);
}

static union ipa_cmd_payload *
ipa_cmd_payload_alloc(struct ipa *ipa, dma_addr_t *addr)
{
	struct gsi_trans_info *trans_info;
	struct ipa_endpoint *endpoint;

	endpoint = ipa->name_map[IPA_ENDPOINT_AP_COMMAND_TX];
	trans_info = &ipa->gsi.channel[endpoint->channel_id].trans_info;

	return gsi_trans_pool_alloc_dma(&trans_info->cmd_pool, addr);
}

 
void ipa_cmd_table_init_add(struct gsi_trans *trans,
			    enum ipa_cmd_opcode opcode, u16 size, u32 offset,
			    dma_addr_t addr, u16 hash_size, u32 hash_offset,
			    dma_addr_t hash_addr)
{
	struct ipa *ipa = container_of(trans->gsi, struct ipa, gsi);
	struct ipa_cmd_hw_ip_fltrt_init *payload;
	union ipa_cmd_payload *cmd_payload;
	dma_addr_t payload_addr;
	u64 val;

	 
	offset += ipa->mem_offset;
	val = u64_encode_bits(offset, IP_FLTRT_FLAGS_NHASH_ADDR_FMASK);
	val |= u64_encode_bits(size, IP_FLTRT_FLAGS_NHASH_SIZE_FMASK);

	 
	if (hash_size) {
		 
		hash_offset += ipa->mem_offset;
		val |= u64_encode_bits(hash_offset,
				       IP_FLTRT_FLAGS_HASH_ADDR_FMASK);
		val |= u64_encode_bits(hash_size,
				       IP_FLTRT_FLAGS_HASH_SIZE_FMASK);
	}

	cmd_payload = ipa_cmd_payload_alloc(ipa, &payload_addr);
	payload = &cmd_payload->table_init;

	 
	if (hash_size)
		payload->hash_rules_addr = cpu_to_le64(hash_addr);
	payload->flags = cpu_to_le64(val);
	payload->nhash_rules_addr = cpu_to_le64(addr);

	gsi_trans_cmd_add(trans, payload, sizeof(*payload), payload_addr,
			  opcode);
}

 
void ipa_cmd_hdr_init_local_add(struct gsi_trans *trans, u32 offset, u16 size,
				dma_addr_t addr)
{
	struct ipa *ipa = container_of(trans->gsi, struct ipa, gsi);
	enum ipa_cmd_opcode opcode = IPA_CMD_HDR_INIT_LOCAL;
	struct ipa_cmd_hw_hdr_init_local *payload;
	union ipa_cmd_payload *cmd_payload;
	dma_addr_t payload_addr;
	u32 flags;

	offset += ipa->mem_offset;

	 
	cmd_payload = ipa_cmd_payload_alloc(ipa, &payload_addr);
	payload = &cmd_payload->hdr_init_local;

	payload->hdr_table_addr = cpu_to_le64(addr);
	flags = u32_encode_bits(size, HDR_INIT_LOCAL_FLAGS_TABLE_SIZE_FMASK);
	flags |= u32_encode_bits(offset, HDR_INIT_LOCAL_FLAGS_HDR_ADDR_FMASK);
	payload->flags = cpu_to_le32(flags);

	gsi_trans_cmd_add(trans, payload, sizeof(*payload), payload_addr,
			  opcode);
}

void ipa_cmd_register_write_add(struct gsi_trans *trans, u32 offset, u32 value,
				u32 mask, bool clear_full)
{
	struct ipa *ipa = container_of(trans->gsi, struct ipa, gsi);
	struct ipa_cmd_register_write *payload;
	union ipa_cmd_payload *cmd_payload;
	u32 opcode = IPA_CMD_REGISTER_WRITE;
	dma_addr_t payload_addr;
	u32 clear_option;
	u32 options;
	u16 flags;

	 
	clear_option = clear_full ? pipeline_clear_full : pipeline_clear_hps;

	 
	if (ipa->version >= IPA_VERSION_4_0) {
		u16 offset_high;
		u32 val;

		 
		 
		val = u16_encode_bits(clear_option,
				      REGISTER_WRITE_OPCODE_CLEAR_OPTION_FMASK);
		opcode |= val;

		 
		offset_high = (u16)u32_get_bits(offset, GENMASK(19, 16));
		offset &= (1 << 16) - 1;

		 
		flags = u16_encode_bits(offset_high,
				REGISTER_WRITE_FLAGS_OFFSET_HIGH_FMASK);
		options = 0;	 

	} else {
		flags = 0;	 
		options = u16_encode_bits(clear_option,
					  REGISTER_WRITE_CLEAR_OPTIONS_FMASK);
	}

	cmd_payload = ipa_cmd_payload_alloc(ipa, &payload_addr);
	payload = &cmd_payload->register_write;

	payload->flags = cpu_to_le16(flags);
	payload->offset = cpu_to_le16((u16)offset);
	payload->value = cpu_to_le32(value);
	payload->value_mask = cpu_to_le32(mask);
	payload->clear_options = cpu_to_le32(options);

	gsi_trans_cmd_add(trans, payload, sizeof(*payload), payload_addr,
			  opcode);
}

 
static void ipa_cmd_ip_packet_init_add(struct gsi_trans *trans, u8 endpoint_id)
{
	struct ipa *ipa = container_of(trans->gsi, struct ipa, gsi);
	enum ipa_cmd_opcode opcode = IPA_CMD_IP_PACKET_INIT;
	struct ipa_cmd_ip_packet_init *payload;
	union ipa_cmd_payload *cmd_payload;
	dma_addr_t payload_addr;

	cmd_payload = ipa_cmd_payload_alloc(ipa, &payload_addr);
	payload = &cmd_payload->ip_packet_init;

	if (ipa->version < IPA_VERSION_5_0) {
		payload->dest_endpoint =
			u8_encode_bits(endpoint_id,
				       IPA_PACKET_INIT_DEST_ENDPOINT_FMASK);
	} else {
		payload->dest_endpoint = endpoint_id;
	}

	gsi_trans_cmd_add(trans, payload, sizeof(*payload), payload_addr,
			  opcode);
}

 
void ipa_cmd_dma_shared_mem_add(struct gsi_trans *trans, u32 offset, u16 size,
				dma_addr_t addr, bool toward_ipa)
{
	struct ipa *ipa = container_of(trans->gsi, struct ipa, gsi);
	enum ipa_cmd_opcode opcode = IPA_CMD_DMA_SHARED_MEM;
	struct ipa_cmd_hw_dma_mem_mem *payload;
	union ipa_cmd_payload *cmd_payload;
	dma_addr_t payload_addr;
	u16 flags;

	 
	WARN_ON(!size);
	WARN_ON(size > U16_MAX);
	WARN_ON(offset > U16_MAX || ipa->mem_offset > U16_MAX - offset);

	offset += ipa->mem_offset;

	cmd_payload = ipa_cmd_payload_alloc(ipa, &payload_addr);
	payload = &cmd_payload->dma_shared_mem;

	 
	payload->size = cpu_to_le16(size);
	payload->local_addr = cpu_to_le16(offset);
	 
	flags = toward_ipa ? 0 : DMA_SHARED_MEM_FLAGS_DIRECTION_FMASK;
	payload->flags = cpu_to_le16(flags);
	payload->system_addr = cpu_to_le64(addr);

	gsi_trans_cmd_add(trans, payload, sizeof(*payload), payload_addr,
			  opcode);
}

static void ipa_cmd_ip_tag_status_add(struct gsi_trans *trans)
{
	struct ipa *ipa = container_of(trans->gsi, struct ipa, gsi);
	enum ipa_cmd_opcode opcode = IPA_CMD_IP_PACKET_TAG_STATUS;
	struct ipa_cmd_ip_packet_tag_status *payload;
	union ipa_cmd_payload *cmd_payload;
	dma_addr_t payload_addr;

	cmd_payload = ipa_cmd_payload_alloc(ipa, &payload_addr);
	payload = &cmd_payload->ip_packet_tag_status;

	payload->tag = le64_encode_bits(0, IP_PACKET_TAG_STATUS_TAG_FMASK);

	gsi_trans_cmd_add(trans, payload, sizeof(*payload), payload_addr,
			  opcode);
}

 
static void ipa_cmd_transfer_add(struct gsi_trans *trans)
{
	struct ipa *ipa = container_of(trans->gsi, struct ipa, gsi);
	enum ipa_cmd_opcode opcode = IPA_CMD_NONE;
	union ipa_cmd_payload *payload;
	dma_addr_t payload_addr;

	 
	payload = ipa_cmd_payload_alloc(ipa, &payload_addr);

	gsi_trans_cmd_add(trans, payload, sizeof(*payload), payload_addr,
			  opcode);
}

 
void ipa_cmd_pipeline_clear_add(struct gsi_trans *trans)
{
	struct ipa *ipa = container_of(trans->gsi, struct ipa, gsi);
	struct ipa_endpoint *endpoint;

	 
	reinit_completion(&ipa->completion);

	 
	ipa_cmd_register_write_add(trans, 0, 0, 0, true);

	 
	endpoint = ipa->name_map[IPA_ENDPOINT_AP_LAN_RX];
	ipa_cmd_ip_packet_init_add(trans, endpoint->endpoint_id);
	ipa_cmd_ip_tag_status_add(trans);
	ipa_cmd_transfer_add(trans);
}

 
u32 ipa_cmd_pipeline_clear_count(void)
{
	return 4;
}

void ipa_cmd_pipeline_clear_wait(struct ipa *ipa)
{
	wait_for_completion(&ipa->completion);
}

 
struct gsi_trans *ipa_cmd_trans_alloc(struct ipa *ipa, u32 tre_count)
{
	struct ipa_endpoint *endpoint;

	if (WARN_ON(tre_count > IPA_COMMAND_TRANS_TRE_MAX))
		return NULL;

	endpoint = ipa->name_map[IPA_ENDPOINT_AP_COMMAND_TX];

	return gsi_channel_trans_alloc(&ipa->gsi, endpoint->channel_id,
				       tre_count, DMA_NONE);
}

 
int ipa_cmd_init(struct ipa *ipa)
{
	ipa_cmd_validate_build();

	if (!ipa_cmd_header_init_local_valid(ipa))
		return -EINVAL;

	if (!ipa_cmd_register_write_valid(ipa))
		return -EINVAL;

	return 0;
}
