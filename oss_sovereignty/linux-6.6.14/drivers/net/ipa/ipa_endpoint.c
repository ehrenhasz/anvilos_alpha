

 

#include <linux/types.h>
#include <linux/device.h>
#include <linux/slab.h>
#include <linux/bitfield.h>
#include <linux/if_rmnet.h>
#include <linux/dma-direction.h>

#include "gsi.h"
#include "gsi_trans.h"
#include "ipa.h"
#include "ipa_data.h"
#include "ipa_endpoint.h"
#include "ipa_cmd.h"
#include "ipa_mem.h"
#include "ipa_modem.h"
#include "ipa_table.h"
#include "ipa_gsi.h"
#include "ipa_power.h"

 
#define IPA_REPLENISH_BATCH	16		 

 
#define IPA_RX_BUFFER_OVERHEAD	(PAGE_SIZE - SKB_MAX_ORDER(NET_SKB_PAD, 0))

 
#define IPA_ENDPOINT_QMAP_METADATA_MASK		0x000000ff  

#define IPA_ENDPOINT_RESET_AGGR_RETRY_MAX	3

 
enum ipa_status_opcode {				 
	IPA_STATUS_OPCODE_PACKET		= 1,
	IPA_STATUS_OPCODE_NEW_RULE_PACKET	= 2,
	IPA_STATUS_OPCODE_DROPPED_PACKET	= 4,
	IPA_STATUS_OPCODE_SUSPENDED_PACKET	= 8,
	IPA_STATUS_OPCODE_LOG			= 16,
	IPA_STATUS_OPCODE_DCMP			= 32,
	IPA_STATUS_OPCODE_PACKET_2ND_PASS	= 64,
};

 
enum ipa_status_exception {				 
	 
	IPA_STATUS_EXCEPTION_DEAGGR		= 1,
	IPA_STATUS_EXCEPTION_IPTYPE		= 4,
	IPA_STATUS_EXCEPTION_PACKET_LENGTH	= 8,
	IPA_STATUS_EXCEPTION_FRAG_RULE_MISS	= 16,
	IPA_STATUS_EXCEPTION_SW_FILTER		= 32,
	IPA_STATUS_EXCEPTION_NAT		= 64,		 
	IPA_STATUS_EXCEPTION_IPV6_CONN_TRACK	= 64,		 
	IPA_STATUS_EXCEPTION_UC			= 128,
	IPA_STATUS_EXCEPTION_INVALID_ENDPOINT	= 129,
	IPA_STATUS_EXCEPTION_HEADER_INSERT	= 136,
	IPA_STATUS_EXCEPTION_CHEKCSUM		= 229,
};

 
enum ipa_status_mask {
	IPA_STATUS_MASK_FRAG_PROCESS		= BIT(0),
	IPA_STATUS_MASK_FILT_PROCESS		= BIT(1),
	IPA_STATUS_MASK_NAT_PROCESS		= BIT(2),
	IPA_STATUS_MASK_ROUTE_PROCESS		= BIT(3),
	IPA_STATUS_MASK_TAG_VALID		= BIT(4),
	IPA_STATUS_MASK_FRAGMENT		= BIT(5),
	IPA_STATUS_MASK_FIRST_FRAGMENT		= BIT(6),
	IPA_STATUS_MASK_V4			= BIT(7),
	IPA_STATUS_MASK_CKSUM_PROCESS		= BIT(8),
	IPA_STATUS_MASK_AGGR_PROCESS		= BIT(9),
	IPA_STATUS_MASK_DEST_EOT		= BIT(10),
	IPA_STATUS_MASK_DEAGGR_PROCESS		= BIT(11),
	IPA_STATUS_MASK_DEAGG_FIRST		= BIT(12),
	IPA_STATUS_MASK_SRC_EOT			= BIT(13),
	IPA_STATUS_MASK_PREV_EOT		= BIT(14),
	IPA_STATUS_MASK_BYTE_LIMIT		= BIT(15),
};

 
#define IPA_STATUS_RULE_MISS	0x3ff	 

 

 
enum ipa_status_field_id {
	STATUS_OPCODE,			 
	STATUS_EXCEPTION,		 
	STATUS_MASK,			 
	STATUS_LENGTH,
	STATUS_SRC_ENDPOINT,
	STATUS_DST_ENDPOINT,
	STATUS_METADATA,
	STATUS_FILTER_LOCAL,		 
	STATUS_FILTER_HASH,		 
	STATUS_FILTER_GLOBAL,		 
	STATUS_FILTER_RETAIN,		 
	STATUS_FILTER_RULE_INDEX,
	STATUS_ROUTER_LOCAL,		 
	STATUS_ROUTER_HASH,		 
	STATUS_UCP,			 
	STATUS_ROUTER_TABLE,
	STATUS_ROUTER_RULE_INDEX,
	STATUS_NAT_HIT,			 
	STATUS_NAT_INDEX,
	STATUS_NAT_TYPE,		 
	STATUS_TAG_LOW32,		 
	STATUS_TAG_HIGH16,		 
	STATUS_SEQUENCE,
	STATUS_TIME_OF_DAY,
	STATUS_HEADER_LOCAL,		 
	STATUS_HEADER_OFFSET,
	STATUS_FRAG_HIT,		 
	STATUS_FRAG_RULE_INDEX,
};

 
#define IPA_STATUS_SIZE			sizeof(__le32[8])

 
static u32 ipa_status_extract(struct ipa *ipa, const void *data,
			      enum ipa_status_field_id field)
{
	enum ipa_version version = ipa->version;
	const __le32 *word = data;

	switch (field) {
	case STATUS_OPCODE:
		return le32_get_bits(word[0], GENMASK(7, 0));
	case STATUS_EXCEPTION:
		return le32_get_bits(word[0], GENMASK(15, 8));
	case STATUS_MASK:
		return le32_get_bits(word[0], GENMASK(31, 16));
	case STATUS_LENGTH:
		return le32_get_bits(word[1], GENMASK(15, 0));
	case STATUS_SRC_ENDPOINT:
		if (version < IPA_VERSION_5_0)
			return le32_get_bits(word[1], GENMASK(20, 16));
		return le32_get_bits(word[1], GENMASK(23, 16));
	 
	 
	case STATUS_DST_ENDPOINT:
		if (version < IPA_VERSION_5_0)
			return le32_get_bits(word[1], GENMASK(28, 24));
		return le32_get_bits(word[7], GENMASK(23, 16));
	 
	case STATUS_METADATA:
		return le32_to_cpu(word[2]);
	case STATUS_FILTER_LOCAL:
		return le32_get_bits(word[3], GENMASK(0, 0));
	case STATUS_FILTER_HASH:
		return le32_get_bits(word[3], GENMASK(1, 1));
	case STATUS_FILTER_GLOBAL:
		return le32_get_bits(word[3], GENMASK(2, 2));
	case STATUS_FILTER_RETAIN:
		return le32_get_bits(word[3], GENMASK(3, 3));
	case STATUS_FILTER_RULE_INDEX:
		return le32_get_bits(word[3], GENMASK(13, 4));
	 
	case STATUS_ROUTER_LOCAL:
		if (version < IPA_VERSION_5_0)
			return le32_get_bits(word[3], GENMASK(14, 14));
		return le32_get_bits(word[1], GENMASK(27, 27));
	case STATUS_ROUTER_HASH:
		if (version < IPA_VERSION_5_0)
			return le32_get_bits(word[3], GENMASK(15, 15));
		return le32_get_bits(word[1], GENMASK(28, 28));
	case STATUS_UCP:
		if (version < IPA_VERSION_5_0)
			return le32_get_bits(word[3], GENMASK(16, 16));
		return le32_get_bits(word[7], GENMASK(31, 31));
	case STATUS_ROUTER_TABLE:
		if (version < IPA_VERSION_5_0)
			return le32_get_bits(word[3], GENMASK(21, 17));
		return le32_get_bits(word[3], GENMASK(21, 14));
	case STATUS_ROUTER_RULE_INDEX:
		return le32_get_bits(word[3], GENMASK(31, 22));
	case STATUS_NAT_HIT:
		return le32_get_bits(word[4], GENMASK(0, 0));
	case STATUS_NAT_INDEX:
		return le32_get_bits(word[4], GENMASK(13, 1));
	case STATUS_NAT_TYPE:
		return le32_get_bits(word[4], GENMASK(15, 14));
	case STATUS_TAG_LOW32:
		return le32_get_bits(word[4], GENMASK(31, 16)) |
			(le32_get_bits(word[5], GENMASK(15, 0)) << 16);
	case STATUS_TAG_HIGH16:
		return le32_get_bits(word[5], GENMASK(31, 16));
	case STATUS_SEQUENCE:
		return le32_get_bits(word[6], GENMASK(7, 0));
	case STATUS_TIME_OF_DAY:
		return le32_get_bits(word[6], GENMASK(31, 8));
	case STATUS_HEADER_LOCAL:
		return le32_get_bits(word[7], GENMASK(0, 0));
	case STATUS_HEADER_OFFSET:
		return le32_get_bits(word[7], GENMASK(10, 1));
	case STATUS_FRAG_HIT:
		return le32_get_bits(word[7], GENMASK(11, 11));
	case STATUS_FRAG_RULE_INDEX:
		return le32_get_bits(word[7], GENMASK(15, 12));
	 
	 
	default:
		WARN(true, "%s: bad field_id %u\n", __func__, field);
		return 0;
	}
}

 
static u32 ipa_aggr_size_kb(u32 rx_buffer_size, bool aggr_hard_limit)
{
	 
	if (!aggr_hard_limit)
		rx_buffer_size -= IPA_MTU + IPA_RX_BUFFER_OVERHEAD;

	 

	return rx_buffer_size / SZ_1K;
}

static bool ipa_endpoint_data_valid_one(struct ipa *ipa, u32 count,
			    const struct ipa_gsi_endpoint_data *all_data,
			    const struct ipa_gsi_endpoint_data *data)
{
	const struct ipa_gsi_endpoint_data *other_data;
	struct device *dev = &ipa->pdev->dev;
	enum ipa_endpoint_name other_name;

	if (ipa_gsi_endpoint_data_empty(data))
		return true;

	if (!data->toward_ipa) {
		const struct ipa_endpoint_rx *rx_config;
		const struct reg *reg;
		u32 buffer_size;
		u32 aggr_size;
		u32 limit;

		if (data->endpoint.filter_support) {
			dev_err(dev, "filtering not supported for "
					"RX endpoint %u\n",
				data->endpoint_id);
			return false;
		}

		 
		if (data->ee_id != GSI_EE_AP)
			return true;

		rx_config = &data->endpoint.config.rx;

		 
		buffer_size = rx_config->buffer_size;
		limit = IPA_MTU + IPA_RX_BUFFER_OVERHEAD;
		if (buffer_size < limit) {
			dev_err(dev, "RX buffer size too small for RX endpoint %u (%u < %u)\n",
				data->endpoint_id, buffer_size, limit);
			return false;
		}

		if (!data->endpoint.config.aggregation) {
			bool result = true;

			 
			if (rx_config->aggr_time_limit) {
				dev_err(dev,
					"time limit with no aggregation for RX endpoint %u\n",
					data->endpoint_id);
				result = false;
			}

			if (rx_config->aggr_hard_limit) {
				dev_err(dev, "hard limit with no aggregation for RX endpoint %u\n",
					data->endpoint_id);
				result = false;
			}

			if (rx_config->aggr_close_eof) {
				dev_err(dev, "close EOF with no aggregation for RX endpoint %u\n",
					data->endpoint_id);
				result = false;
			}

			return result;	 
		}

		 
		aggr_size = ipa_aggr_size_kb(buffer_size - NET_SKB_PAD,
					     rx_config->aggr_hard_limit);
		reg = ipa_reg(ipa, ENDP_INIT_AGGR);

		limit = reg_field_max(reg, BYTE_LIMIT);
		if (aggr_size > limit) {
			dev_err(dev, "aggregated size too large for RX endpoint %u (%u KB > %u KB)\n",
				data->endpoint_id, aggr_size, limit);

			return false;
		}

		return true;	 
	}

	 
	if (ipa->version >= IPA_VERSION_4_5) {
		if (data->endpoint.config.tx.seq_rep_type) {
			dev_err(dev, "no-zero seq_rep_type TX endpoint %u\n",
				data->endpoint_id);
			return false;
		}
	}

	if (data->endpoint.config.status_enable) {
		other_name = data->endpoint.config.tx.status_endpoint;
		if (other_name >= count) {
			dev_err(dev, "status endpoint name %u out of range "
					"for endpoint %u\n",
				other_name, data->endpoint_id);
			return false;
		}

		 
		other_data = &all_data[other_name];
		if (ipa_gsi_endpoint_data_empty(other_data)) {
			dev_err(dev, "DMA endpoint name %u undefined "
					"for endpoint %u\n",
				other_name, data->endpoint_id);
			return false;
		}

		 
		if (other_data->toward_ipa) {
			dev_err(dev,
				"status endpoint for endpoint %u not RX\n",
				data->endpoint_id);
			return false;
		}

		 
		if (other_data->ee_id == GSI_EE_AP) {
			 
			if (!other_data->endpoint.config.status_enable) {
				dev_err(dev,
					"status not enabled for endpoint %u\n",
					other_data->endpoint_id);
				return false;
			}
		}
	}

	if (data->endpoint.config.dma_mode) {
		other_name = data->endpoint.config.dma_endpoint;
		if (other_name >= count) {
			dev_err(dev, "DMA endpoint name %u out of range "
					"for endpoint %u\n",
				other_name, data->endpoint_id);
			return false;
		}

		other_data = &all_data[other_name];
		if (ipa_gsi_endpoint_data_empty(other_data)) {
			dev_err(dev, "DMA endpoint name %u undefined "
					"for endpoint %u\n",
				other_name, data->endpoint_id);
			return false;
		}
	}

	return true;
}

 
static u32 ipa_endpoint_max(struct ipa *ipa, u32 count,
			    const struct ipa_gsi_endpoint_data *data)
{
	const struct ipa_gsi_endpoint_data *dp = data;
	struct device *dev = &ipa->pdev->dev;
	enum ipa_endpoint_name name;
	u32 max;

	if (count > IPA_ENDPOINT_COUNT) {
		dev_err(dev, "too many endpoints specified (%u > %u)\n",
			count, IPA_ENDPOINT_COUNT);
		return 0;
	}

	 
	if (ipa_gsi_endpoint_data_empty(&data[IPA_ENDPOINT_AP_COMMAND_TX])) {
		dev_err(dev, "command TX endpoint not defined\n");
		return 0;
	}
	if (ipa_gsi_endpoint_data_empty(&data[IPA_ENDPOINT_AP_LAN_RX])) {
		dev_err(dev, "LAN RX endpoint not defined\n");
		return 0;
	}
	if (ipa_gsi_endpoint_data_empty(&data[IPA_ENDPOINT_AP_MODEM_TX])) {
		dev_err(dev, "AP->modem TX endpoint not defined\n");
		return 0;
	}
	if (ipa_gsi_endpoint_data_empty(&data[IPA_ENDPOINT_AP_MODEM_RX])) {
		dev_err(dev, "AP<-modem RX endpoint not defined\n");
		return 0;
	}

	max = 0;
	for (name = 0; name < count; name++, dp++) {
		if (!ipa_endpoint_data_valid_one(ipa, count, data, dp))
			return 0;
		max = max_t(u32, max, dp->endpoint_id);
	}

	return max;
}

 
static struct gsi_trans *ipa_endpoint_trans_alloc(struct ipa_endpoint *endpoint,
						  u32 tre_count)
{
	struct gsi *gsi = &endpoint->ipa->gsi;
	u32 channel_id = endpoint->channel_id;
	enum dma_data_direction direction;

	direction = endpoint->toward_ipa ? DMA_TO_DEVICE : DMA_FROM_DEVICE;

	return gsi_channel_trans_alloc(gsi, channel_id, tre_count, direction);
}

 
static bool
ipa_endpoint_init_ctrl(struct ipa_endpoint *endpoint, bool suspend_delay)
{
	struct ipa *ipa = endpoint->ipa;
	const struct reg *reg;
	u32 field_id;
	u32 offset;
	bool state;
	u32 mask;
	u32 val;

	if (endpoint->toward_ipa)
		WARN_ON(ipa->version >= IPA_VERSION_4_2);
	else
		WARN_ON(ipa->version >= IPA_VERSION_4_0);

	reg = ipa_reg(ipa, ENDP_INIT_CTRL);
	offset = reg_n_offset(reg, endpoint->endpoint_id);
	val = ioread32(ipa->reg_virt + offset);

	field_id = endpoint->toward_ipa ? ENDP_DELAY : ENDP_SUSPEND;
	mask = reg_bit(reg, field_id);

	state = !!(val & mask);

	 
	if (suspend_delay != state) {
		val ^= mask;
		iowrite32(val, ipa->reg_virt + offset);
	}

	return state;
}

 
static void
ipa_endpoint_program_delay(struct ipa_endpoint *endpoint, bool enable)
{
	 
	WARN_ON(endpoint->ipa->version >= IPA_VERSION_4_2);
	WARN_ON(!endpoint->toward_ipa);

	(void)ipa_endpoint_init_ctrl(endpoint, enable);
}

static bool ipa_endpoint_aggr_active(struct ipa_endpoint *endpoint)
{
	u32 endpoint_id = endpoint->endpoint_id;
	struct ipa *ipa = endpoint->ipa;
	u32 unit = endpoint_id / 32;
	const struct reg *reg;
	u32 val;

	WARN_ON(!test_bit(endpoint_id, ipa->available));

	reg = ipa_reg(ipa, STATE_AGGR_ACTIVE);
	val = ioread32(ipa->reg_virt + reg_n_offset(reg, unit));

	return !!(val & BIT(endpoint_id % 32));
}

static void ipa_endpoint_force_close(struct ipa_endpoint *endpoint)
{
	u32 endpoint_id = endpoint->endpoint_id;
	u32 mask = BIT(endpoint_id % 32);
	struct ipa *ipa = endpoint->ipa;
	u32 unit = endpoint_id / 32;
	const struct reg *reg;

	WARN_ON(!test_bit(endpoint_id, ipa->available));

	reg = ipa_reg(ipa, AGGR_FORCE_CLOSE);
	iowrite32(mask, ipa->reg_virt + reg_n_offset(reg, unit));
}

 
static void ipa_endpoint_suspend_aggr(struct ipa_endpoint *endpoint)
{
	struct ipa *ipa = endpoint->ipa;

	if (!endpoint->config.aggregation)
		return;

	 
	if (!ipa_endpoint_aggr_active(endpoint))
		return;

	 
	ipa_endpoint_force_close(endpoint);

	ipa_interrupt_simulate_suspend(ipa->interrupt);
}

 
static bool
ipa_endpoint_program_suspend(struct ipa_endpoint *endpoint, bool enable)
{
	bool suspended;

	if (endpoint->ipa->version >= IPA_VERSION_4_0)
		return enable;	 

	WARN_ON(endpoint->toward_ipa);

	suspended = ipa_endpoint_init_ctrl(endpoint, enable);

	 
	if (enable && !suspended)
		ipa_endpoint_suspend_aggr(endpoint);

	return suspended;
}

 
void ipa_endpoint_modem_pause_all(struct ipa *ipa, bool enable)
{
	u32 endpoint_id = 0;

	while (endpoint_id < ipa->endpoint_count) {
		struct ipa_endpoint *endpoint = &ipa->endpoint[endpoint_id++];

		if (endpoint->ee_id != GSI_EE_MODEM)
			continue;

		if (!endpoint->toward_ipa)
			(void)ipa_endpoint_program_suspend(endpoint, enable);
		else if (ipa->version < IPA_VERSION_4_2)
			ipa_endpoint_program_delay(endpoint, enable);
		else
			gsi_modem_channel_flow_control(&ipa->gsi,
						       endpoint->channel_id,
						       enable);
	}
}

 
int ipa_endpoint_modem_exception_reset_all(struct ipa *ipa)
{
	struct gsi_trans *trans;
	u32 endpoint_id;
	u32 count;

	 
	count = ipa->modem_tx_count + ipa_cmd_pipeline_clear_count();
	trans = ipa_cmd_trans_alloc(ipa, count);
	if (!trans) {
		dev_err(&ipa->pdev->dev,
			"no transaction to reset modem exception endpoints\n");
		return -EBUSY;
	}

	for_each_set_bit(endpoint_id, ipa->defined, ipa->endpoint_count) {
		struct ipa_endpoint *endpoint;
		const struct reg *reg;
		u32 offset;

		 
		endpoint = &ipa->endpoint[endpoint_id];
		if (!(endpoint->ee_id == GSI_EE_MODEM && endpoint->toward_ipa))
			continue;

		reg = ipa_reg(ipa, ENDP_STATUS);
		offset = reg_n_offset(reg, endpoint_id);

		 
		ipa_cmd_register_write_add(trans, offset, 0, ~0, false);
	}

	ipa_cmd_pipeline_clear_add(trans);

	gsi_trans_commit_wait(trans);

	ipa_cmd_pipeline_clear_wait(ipa);

	return 0;
}

static void ipa_endpoint_init_cfg(struct ipa_endpoint *endpoint)
{
	u32 endpoint_id = endpoint->endpoint_id;
	struct ipa *ipa = endpoint->ipa;
	enum ipa_cs_offload_en enabled;
	const struct reg *reg;
	u32 val = 0;

	reg = ipa_reg(ipa, ENDP_INIT_CFG);
	 
	if (endpoint->config.checksum) {
		enum ipa_version version = ipa->version;

		if (endpoint->toward_ipa) {
			u32 off;

			 
			off = sizeof(struct rmnet_map_header) / sizeof(u32);
			val |= reg_encode(reg, CS_METADATA_HDR_OFFSET, off);

			enabled = version < IPA_VERSION_4_5
					? IPA_CS_OFFLOAD_UL
					: IPA_CS_OFFLOAD_INLINE;
		} else {
			enabled = version < IPA_VERSION_4_5
					? IPA_CS_OFFLOAD_DL
					: IPA_CS_OFFLOAD_INLINE;
		}
	} else {
		enabled = IPA_CS_OFFLOAD_NONE;
	}
	val |= reg_encode(reg, CS_OFFLOAD_EN, enabled);
	 

	iowrite32(val, ipa->reg_virt + reg_n_offset(reg, endpoint_id));
}

static void ipa_endpoint_init_nat(struct ipa_endpoint *endpoint)
{
	u32 endpoint_id = endpoint->endpoint_id;
	struct ipa *ipa = endpoint->ipa;
	const struct reg *reg;
	u32 val;

	if (!endpoint->toward_ipa)
		return;

	reg = ipa_reg(ipa, ENDP_INIT_NAT);
	val = reg_encode(reg, NAT_EN, IPA_NAT_TYPE_BYPASS);

	iowrite32(val, ipa->reg_virt + reg_n_offset(reg, endpoint_id));
}

static u32
ipa_qmap_header_size(enum ipa_version version, struct ipa_endpoint *endpoint)
{
	u32 header_size = sizeof(struct rmnet_map_header);

	 
	if (!endpoint->config.checksum)
		return header_size;

	if (version < IPA_VERSION_4_5) {
		 
		if (endpoint->toward_ipa)
			header_size += sizeof(struct rmnet_map_ul_csum_header);
	} else {
		 
		header_size += sizeof(struct rmnet_map_v5_csum_header);
	}

	return header_size;
}

 
static u32 ipa_header_size_encode(enum ipa_version version,
				  const struct reg *reg, u32 header_size)
{
	u32 field_max = reg_field_max(reg, HDR_LEN);
	u32 val;

	 
	val = reg_encode(reg, HDR_LEN, header_size & field_max);
	if (version < IPA_VERSION_4_5) {
		WARN_ON(header_size > field_max);
		return val;
	}

	 
	header_size >>= hweight32(field_max);
	WARN_ON(header_size > reg_field_max(reg, HDR_LEN_MSB));
	val |= reg_encode(reg, HDR_LEN_MSB, header_size);

	return val;
}

 
static u32 ipa_metadata_offset_encode(enum ipa_version version,
				      const struct reg *reg, u32 offset)
{
	u32 field_max = reg_field_max(reg, HDR_OFST_METADATA);
	u32 val;

	 
	val = reg_encode(reg, HDR_OFST_METADATA, offset);
	if (version < IPA_VERSION_4_5) {
		WARN_ON(offset > field_max);
		return val;
	}

	 
	offset >>= hweight32(field_max);
	WARN_ON(offset > reg_field_max(reg, HDR_OFST_METADATA_MSB));
	val |= reg_encode(reg, HDR_OFST_METADATA_MSB, offset);

	return val;
}

 
static void ipa_endpoint_init_hdr(struct ipa_endpoint *endpoint)
{
	u32 endpoint_id = endpoint->endpoint_id;
	struct ipa *ipa = endpoint->ipa;
	const struct reg *reg;
	u32 val = 0;

	reg = ipa_reg(ipa, ENDP_INIT_HDR);
	if (endpoint->config.qmap) {
		enum ipa_version version = ipa->version;
		size_t header_size;

		header_size = ipa_qmap_header_size(version, endpoint);
		val = ipa_header_size_encode(version, reg, header_size);

		 
		if (!endpoint->toward_ipa) {
			u32 off;      

			 
			off = offsetof(struct rmnet_map_header, mux_id);
			val |= ipa_metadata_offset_encode(version, reg, off);

			 
			off = offsetof(struct rmnet_map_header, pkt_len);
			 
			if (version >= IPA_VERSION_4_5)
				off &= reg_field_max(reg, HDR_OFST_PKT_SIZE);

			val |= reg_bit(reg, HDR_OFST_PKT_SIZE_VALID);
			val |= reg_encode(reg, HDR_OFST_PKT_SIZE, off);
		}
		 
		val |= reg_bit(reg, HDR_OFST_METADATA_VALID);

		 
		 
		 
		 
	}

	iowrite32(val, ipa->reg_virt + reg_n_offset(reg, endpoint_id));
}

static void ipa_endpoint_init_hdr_ext(struct ipa_endpoint *endpoint)
{
	u32 pad_align = endpoint->config.rx.pad_align;
	u32 endpoint_id = endpoint->endpoint_id;
	struct ipa *ipa = endpoint->ipa;
	const struct reg *reg;
	u32 val = 0;

	reg = ipa_reg(ipa, ENDP_INIT_HDR_EXT);
	if (endpoint->config.qmap) {
		 
		val |= reg_bit(reg, HDR_ENDIANNESS);	 

		 
		if (!endpoint->toward_ipa) {
			val |= reg_bit(reg, HDR_TOTAL_LEN_OR_PAD_VALID);
			 
			val |= reg_bit(reg, HDR_PAYLOAD_LEN_INC_PADDING);
			 
		}
	}

	 
	if (!endpoint->toward_ipa)
		val |= reg_encode(reg, HDR_PAD_TO_ALIGNMENT, pad_align);

	 
	if (ipa->version >= IPA_VERSION_4_5) {
		 
		if (endpoint->config.qmap && !endpoint->toward_ipa) {
			u32 mask = reg_field_max(reg, HDR_OFST_PKT_SIZE);
			u32 off;      

			off = offsetof(struct rmnet_map_header, pkt_len);
			 
			off >>= hweight32(mask);
			val |= reg_encode(reg, HDR_OFST_PKT_SIZE_MSB, off);
			 
		}
	}

	iowrite32(val, ipa->reg_virt + reg_n_offset(reg, endpoint_id));
}

static void ipa_endpoint_init_hdr_metadata_mask(struct ipa_endpoint *endpoint)
{
	u32 endpoint_id = endpoint->endpoint_id;
	struct ipa *ipa = endpoint->ipa;
	const struct reg *reg;
	u32 val = 0;
	u32 offset;

	if (endpoint->toward_ipa)
		return;		 

	reg = ipa_reg(ipa,  ENDP_INIT_HDR_METADATA_MASK);
	offset = reg_n_offset(reg, endpoint_id);

	 
	if (endpoint->config.qmap)
		val = (__force u32)cpu_to_be32(IPA_ENDPOINT_QMAP_METADATA_MASK);

	iowrite32(val, ipa->reg_virt + offset);
}

static void ipa_endpoint_init_mode(struct ipa_endpoint *endpoint)
{
	struct ipa *ipa = endpoint->ipa;
	const struct reg *reg;
	u32 offset;
	u32 val;

	if (!endpoint->toward_ipa)
		return;		 

	reg = ipa_reg(ipa, ENDP_INIT_MODE);
	if (endpoint->config.dma_mode) {
		enum ipa_endpoint_name name = endpoint->config.dma_endpoint;
		u32 dma_endpoint_id = ipa->name_map[name]->endpoint_id;

		val = reg_encode(reg, ENDP_MODE, IPA_DMA);
		val |= reg_encode(reg, DEST_PIPE_INDEX, dma_endpoint_id);
	} else {
		val = reg_encode(reg, ENDP_MODE, IPA_BASIC);
	}
	 

	offset = reg_n_offset(reg, endpoint->endpoint_id);
	iowrite32(val, ipa->reg_virt + offset);
}

 
static u32
ipa_qtime_val(struct ipa *ipa, u32 microseconds, u32 max, u32 *select)
{
	u32 which = 0;
	u32 ticks;

	 
	ticks = DIV_ROUND_CLOSEST(microseconds, 100);
	if (ticks <= max)
		goto out;

	 
	which = 1;
	ticks = DIV_ROUND_CLOSEST(microseconds, 1000);
	if (ticks <= max)
		goto out;

	if (ipa->version >= IPA_VERSION_5_0) {
		 
		which = 2;
		ticks = DIV_ROUND_CLOSEST(microseconds, 100);
	}
	WARN_ON(ticks > max);
out:
	*select = which;

	return ticks;
}

 
static u32 aggr_time_limit_encode(struct ipa *ipa, const struct reg *reg,
				  u32 microseconds)
{
	u32 ticks;
	u32 max;

	if (!microseconds)
		return 0;	 

	max = reg_field_max(reg, TIME_LIMIT);
	if (ipa->version >= IPA_VERSION_4_5) {
		u32 select;

		ticks = ipa_qtime_val(ipa, microseconds, max, &select);

		return reg_encode(reg, AGGR_GRAN_SEL, select) |
		       reg_encode(reg, TIME_LIMIT, ticks);
	}

	 
	ticks = DIV_ROUND_CLOSEST(microseconds, IPA_AGGR_GRANULARITY);
	WARN(ticks > max, "aggr_time_limit too large (%u > %u usec)\n",
	     microseconds, max * IPA_AGGR_GRANULARITY);

	return reg_encode(reg, TIME_LIMIT, ticks);
}

static void ipa_endpoint_init_aggr(struct ipa_endpoint *endpoint)
{
	u32 endpoint_id = endpoint->endpoint_id;
	struct ipa *ipa = endpoint->ipa;
	const struct reg *reg;
	u32 val = 0;

	reg = ipa_reg(ipa, ENDP_INIT_AGGR);
	if (endpoint->config.aggregation) {
		if (!endpoint->toward_ipa) {
			const struct ipa_endpoint_rx *rx_config;
			u32 buffer_size;
			u32 limit;

			rx_config = &endpoint->config.rx;
			val |= reg_encode(reg, AGGR_EN, IPA_ENABLE_AGGR);
			val |= reg_encode(reg, AGGR_TYPE, IPA_GENERIC);

			buffer_size = rx_config->buffer_size;
			limit = ipa_aggr_size_kb(buffer_size - NET_SKB_PAD,
						 rx_config->aggr_hard_limit);
			val |= reg_encode(reg, BYTE_LIMIT, limit);

			limit = rx_config->aggr_time_limit;
			val |= aggr_time_limit_encode(ipa, reg, limit);

			 

			if (rx_config->aggr_close_eof)
				val |= reg_bit(reg, SW_EOF_ACTIVE);
		} else {
			val |= reg_encode(reg, AGGR_EN, IPA_ENABLE_DEAGGR);
			val |= reg_encode(reg, AGGR_TYPE, IPA_QCMAP);
			 
		}
		 
		 
	} else {
		val |= reg_encode(reg, AGGR_EN, IPA_BYPASS_AGGR);
		 
	}

	iowrite32(val, ipa->reg_virt + reg_n_offset(reg, endpoint_id));
}

 
static u32 hol_block_timer_encode(struct ipa *ipa, const struct reg *reg,
				  u32 microseconds)
{
	u32 width;
	u32 scale;
	u64 ticks;
	u64 rate;
	u32 high;
	u32 val;

	if (!microseconds)
		return 0;	 

	if (ipa->version >= IPA_VERSION_4_5) {
		u32 max = reg_field_max(reg, TIMER_LIMIT);
		u32 select;
		u32 ticks;

		ticks = ipa_qtime_val(ipa, microseconds, max, &select);

		return reg_encode(reg, TIMER_GRAN_SEL, 1) |
		       reg_encode(reg, TIMER_LIMIT, ticks);
	}

	 
	rate = ipa_core_clock_rate(ipa);
	ticks = DIV_ROUND_CLOSEST(microseconds * rate, 128 * USEC_PER_SEC);

	 
	WARN_ON(ticks > reg_field_max(reg, TIMER_BASE_VALUE));

	 
	if (ipa->version < IPA_VERSION_4_2)
		return reg_encode(reg, TIMER_BASE_VALUE, (u32)ticks);

	 
	high = fls(ticks);		 
	width = hweight32(reg_fmask(reg, TIMER_BASE_VALUE));
	scale = high > width ? high - width : 0;
	if (scale) {
		 
		ticks += 1 << (scale - 1);
		 
		if (fls(ticks) != high)
			scale++;
	}

	val = reg_encode(reg, TIMER_SCALE, scale);
	val |= reg_encode(reg, TIMER_BASE_VALUE, (u32)ticks >> scale);

	return val;
}

 
static void ipa_endpoint_init_hol_block_timer(struct ipa_endpoint *endpoint,
					      u32 microseconds)
{
	u32 endpoint_id = endpoint->endpoint_id;
	struct ipa *ipa = endpoint->ipa;
	const struct reg *reg;
	u32 val;

	 
	reg = ipa_reg(ipa, ENDP_INIT_HOL_BLOCK_TIMER);
	val = hol_block_timer_encode(ipa, reg, microseconds);

	iowrite32(val, ipa->reg_virt + reg_n_offset(reg, endpoint_id));
}

static void
ipa_endpoint_init_hol_block_en(struct ipa_endpoint *endpoint, bool enable)
{
	u32 endpoint_id = endpoint->endpoint_id;
	struct ipa *ipa = endpoint->ipa;
	const struct reg *reg;
	u32 offset;
	u32 val;

	reg = ipa_reg(ipa, ENDP_INIT_HOL_BLOCK_EN);
	offset = reg_n_offset(reg, endpoint_id);
	val = enable ? reg_bit(reg, HOL_BLOCK_EN) : 0;

	iowrite32(val, ipa->reg_virt + offset);

	 
	if (enable && ipa->version >= IPA_VERSION_4_5)
		iowrite32(val, ipa->reg_virt + offset);
}

 
static void ipa_endpoint_init_hol_block_enable(struct ipa_endpoint *endpoint,
					       u32 microseconds)
{
	ipa_endpoint_init_hol_block_timer(endpoint, microseconds);
	ipa_endpoint_init_hol_block_en(endpoint, true);
}

static void ipa_endpoint_init_hol_block_disable(struct ipa_endpoint *endpoint)
{
	ipa_endpoint_init_hol_block_en(endpoint, false);
}

void ipa_endpoint_modem_hol_block_clear_all(struct ipa *ipa)
{
	u32 endpoint_id = 0;

	while (endpoint_id < ipa->endpoint_count) {
		struct ipa_endpoint *endpoint = &ipa->endpoint[endpoint_id++];

		if (endpoint->toward_ipa || endpoint->ee_id != GSI_EE_MODEM)
			continue;

		ipa_endpoint_init_hol_block_disable(endpoint);
		ipa_endpoint_init_hol_block_enable(endpoint, 0);
	}
}

static void ipa_endpoint_init_deaggr(struct ipa_endpoint *endpoint)
{
	u32 endpoint_id = endpoint->endpoint_id;
	struct ipa *ipa = endpoint->ipa;
	const struct reg *reg;
	u32 val = 0;

	if (!endpoint->toward_ipa)
		return;		 

	reg = ipa_reg(ipa, ENDP_INIT_DEAGGR);
	 
	 
	 
	 

	iowrite32(val, ipa->reg_virt + reg_n_offset(reg, endpoint_id));
}

static void ipa_endpoint_init_rsrc_grp(struct ipa_endpoint *endpoint)
{
	u32 resource_group = endpoint->config.resource_group;
	u32 endpoint_id = endpoint->endpoint_id;
	struct ipa *ipa = endpoint->ipa;
	const struct reg *reg;
	u32 val;

	reg = ipa_reg(ipa, ENDP_INIT_RSRC_GRP);
	val = reg_encode(reg, ENDP_RSRC_GRP, resource_group);

	iowrite32(val, ipa->reg_virt + reg_n_offset(reg, endpoint_id));
}

static void ipa_endpoint_init_seq(struct ipa_endpoint *endpoint)
{
	u32 endpoint_id = endpoint->endpoint_id;
	struct ipa *ipa = endpoint->ipa;
	const struct reg *reg;
	u32 val;

	if (!endpoint->toward_ipa)
		return;		 

	reg = ipa_reg(ipa, ENDP_INIT_SEQ);

	 
	val = reg_encode(reg, SEQ_TYPE, endpoint->config.tx.seq_type);

	 
	if (ipa->version < IPA_VERSION_4_5)
		val |= reg_encode(reg, SEQ_REP_TYPE,
				  endpoint->config.tx.seq_rep_type);

	iowrite32(val, ipa->reg_virt + reg_n_offset(reg, endpoint_id));
}

 
int ipa_endpoint_skb_tx(struct ipa_endpoint *endpoint, struct sk_buff *skb)
{
	struct gsi_trans *trans;
	u32 nr_frags;
	int ret;

	 
	nr_frags = skb_shinfo(skb)->nr_frags;
	if (nr_frags > endpoint->skb_frag_max) {
		if (skb_linearize(skb))
			return -E2BIG;
		nr_frags = 0;
	}

	trans = ipa_endpoint_trans_alloc(endpoint, 1 + nr_frags);
	if (!trans)
		return -EBUSY;

	ret = gsi_trans_skb_add(trans, skb);
	if (ret)
		goto err_trans_free;
	trans->data = skb;	 

	gsi_trans_commit(trans, !netdev_xmit_more());

	return 0;

err_trans_free:
	gsi_trans_free(trans);

	return -ENOMEM;
}

static void ipa_endpoint_status(struct ipa_endpoint *endpoint)
{
	u32 endpoint_id = endpoint->endpoint_id;
	struct ipa *ipa = endpoint->ipa;
	const struct reg *reg;
	u32 val = 0;

	reg = ipa_reg(ipa, ENDP_STATUS);
	if (endpoint->config.status_enable) {
		val |= reg_bit(reg, STATUS_EN);
		if (endpoint->toward_ipa) {
			enum ipa_endpoint_name name;
			u32 status_endpoint_id;

			name = endpoint->config.tx.status_endpoint;
			status_endpoint_id = ipa->name_map[name]->endpoint_id;

			val |= reg_encode(reg, STATUS_ENDP, status_endpoint_id);
		}
		 
		 
	}

	iowrite32(val, ipa->reg_virt + reg_n_offset(reg, endpoint_id));
}

static int ipa_endpoint_replenish_one(struct ipa_endpoint *endpoint,
				      struct gsi_trans *trans)
{
	struct page *page;
	u32 buffer_size;
	u32 offset;
	u32 len;
	int ret;

	buffer_size = endpoint->config.rx.buffer_size;
	page = dev_alloc_pages(get_order(buffer_size));
	if (!page)
		return -ENOMEM;

	 
	offset = NET_SKB_PAD;
	len = buffer_size - offset;

	ret = gsi_trans_page_add(trans, page, len, offset);
	if (ret)
		put_page(page);
	else
		trans->data = page;	 

	return ret;
}

 
static void ipa_endpoint_replenish(struct ipa_endpoint *endpoint)
{
	struct gsi_trans *trans;

	if (!test_bit(IPA_REPLENISH_ENABLED, endpoint->replenish_flags))
		return;

	 
	if (test_and_set_bit(IPA_REPLENISH_ACTIVE, endpoint->replenish_flags))
		return;

	while ((trans = ipa_endpoint_trans_alloc(endpoint, 1))) {
		bool doorbell;

		if (ipa_endpoint_replenish_one(endpoint, trans))
			goto try_again_later;


		 
		doorbell = !(++endpoint->replenish_count % IPA_REPLENISH_BATCH);
		gsi_trans_commit(trans, doorbell);
	}

	clear_bit(IPA_REPLENISH_ACTIVE, endpoint->replenish_flags);

	return;

try_again_later:
	gsi_trans_free(trans);
	clear_bit(IPA_REPLENISH_ACTIVE, endpoint->replenish_flags);

	 
	if (gsi_channel_trans_idle(&endpoint->ipa->gsi, endpoint->channel_id))
		schedule_delayed_work(&endpoint->replenish_work,
				      msecs_to_jiffies(1));
}

static void ipa_endpoint_replenish_enable(struct ipa_endpoint *endpoint)
{
	set_bit(IPA_REPLENISH_ENABLED, endpoint->replenish_flags);

	 
	if (gsi_channel_trans_idle(&endpoint->ipa->gsi, endpoint->channel_id))
		ipa_endpoint_replenish(endpoint);
}

static void ipa_endpoint_replenish_disable(struct ipa_endpoint *endpoint)
{
	clear_bit(IPA_REPLENISH_ENABLED, endpoint->replenish_flags);
}

static void ipa_endpoint_replenish_work(struct work_struct *work)
{
	struct delayed_work *dwork = to_delayed_work(work);
	struct ipa_endpoint *endpoint;

	endpoint = container_of(dwork, struct ipa_endpoint, replenish_work);

	ipa_endpoint_replenish(endpoint);
}

static void ipa_endpoint_skb_copy(struct ipa_endpoint *endpoint,
				  void *data, u32 len, u32 extra)
{
	struct sk_buff *skb;

	if (!endpoint->netdev)
		return;

	skb = __dev_alloc_skb(len, GFP_ATOMIC);
	if (skb) {
		 
		skb_put(skb, len);
		memcpy(skb->data, data, len);
		skb->truesize += extra;
	}

	ipa_modem_skb_rx(endpoint->netdev, skb);
}

static bool ipa_endpoint_skb_build(struct ipa_endpoint *endpoint,
				   struct page *page, u32 len)
{
	u32 buffer_size = endpoint->config.rx.buffer_size;
	struct sk_buff *skb;

	 
	if (!endpoint->netdev)
		return false;

	WARN_ON(len > SKB_WITH_OVERHEAD(buffer_size - NET_SKB_PAD));

	skb = build_skb(page_address(page), buffer_size);
	if (skb) {
		 
		skb_reserve(skb, NET_SKB_PAD);
		skb_put(skb, len);
	}

	 
	ipa_modem_skb_rx(endpoint->netdev, skb);

	return skb != NULL;
}

  
static bool ipa_status_format_packet(enum ipa_status_opcode opcode)
{
	switch (opcode) {
	case IPA_STATUS_OPCODE_PACKET:
	case IPA_STATUS_OPCODE_DROPPED_PACKET:
	case IPA_STATUS_OPCODE_SUSPENDED_PACKET:
	case IPA_STATUS_OPCODE_PACKET_2ND_PASS:
		return true;
	default:
		return false;
	}
}

static bool
ipa_endpoint_status_skip(struct ipa_endpoint *endpoint, const void *data)
{
	struct ipa *ipa = endpoint->ipa;
	enum ipa_status_opcode opcode;
	u32 endpoint_id;

	opcode = ipa_status_extract(ipa, data, STATUS_OPCODE);
	if (!ipa_status_format_packet(opcode))
		return true;

	endpoint_id = ipa_status_extract(ipa, data, STATUS_DST_ENDPOINT);
	if (endpoint_id != endpoint->endpoint_id)
		return true;

	return false;	 
}

static bool
ipa_endpoint_status_tag_valid(struct ipa_endpoint *endpoint, const void *data)
{
	struct ipa_endpoint *command_endpoint;
	enum ipa_status_mask status_mask;
	struct ipa *ipa = endpoint->ipa;
	u32 endpoint_id;

	status_mask = ipa_status_extract(ipa, data, STATUS_MASK);
	if (!status_mask)
		return false;	 

	 
	endpoint_id = ipa_status_extract(ipa, data, STATUS_SRC_ENDPOINT);
	command_endpoint = ipa->name_map[IPA_ENDPOINT_AP_COMMAND_TX];
	if (endpoint_id == command_endpoint->endpoint_id) {
		complete(&ipa->completion);
	} else {
		dev_err(&ipa->pdev->dev,
			"unexpected tagged packet from endpoint %u\n",
			endpoint_id);
	}

	return true;
}

 
static bool
ipa_endpoint_status_drop(struct ipa_endpoint *endpoint, const void *data)
{
	enum ipa_status_exception exception;
	struct ipa *ipa = endpoint->ipa;
	u32 rule;

	 
	if (ipa_endpoint_status_tag_valid(endpoint, data))
		return true;

	 
	exception = ipa_status_extract(ipa, data, STATUS_EXCEPTION);
	if (exception)
		return exception == IPA_STATUS_EXCEPTION_DEAGGR;

	 
	rule = ipa_status_extract(ipa, data, STATUS_ROUTER_RULE_INDEX);

	return rule == IPA_STATUS_RULE_MISS;
}

static void ipa_endpoint_status_parse(struct ipa_endpoint *endpoint,
				      struct page *page, u32 total_len)
{
	u32 buffer_size = endpoint->config.rx.buffer_size;
	void *data = page_address(page) + NET_SKB_PAD;
	u32 unused = buffer_size - total_len;
	struct ipa *ipa = endpoint->ipa;
	u32 resid = total_len;

	while (resid) {
		u32 length;
		u32 align;
		u32 len;

		if (resid < IPA_STATUS_SIZE) {
			dev_err(&endpoint->ipa->pdev->dev,
				"short message (%u bytes < %zu byte status)\n",
				resid, IPA_STATUS_SIZE);
			break;
		}

		 
		length = ipa_status_extract(ipa, data, STATUS_LENGTH);
		if (!length || ipa_endpoint_status_skip(endpoint, data)) {
			data += IPA_STATUS_SIZE;
			resid -= IPA_STATUS_SIZE;
			continue;
		}

		 
		align = endpoint->config.rx.pad_align ? : 1;
		len = IPA_STATUS_SIZE + ALIGN(length, align);
		if (endpoint->config.checksum)
			len += sizeof(struct rmnet_map_dl_csum_trailer);

		if (!ipa_endpoint_status_drop(endpoint, data)) {
			void *data2;
			u32 extra;

			 
			data2 = data + IPA_STATUS_SIZE;

			 
			extra = DIV_ROUND_CLOSEST(unused * len, total_len);
			ipa_endpoint_skb_copy(endpoint, data2, length, extra);
		}

		 
		data += len;
		resid -= len;
	}
}

void ipa_endpoint_trans_complete(struct ipa_endpoint *endpoint,
				 struct gsi_trans *trans)
{
	struct page *page;

	if (endpoint->toward_ipa)
		return;

	if (trans->cancelled)
		goto done;

	 
	page = trans->data;
	if (endpoint->config.status_enable)
		ipa_endpoint_status_parse(endpoint, page, trans->len);
	else if (ipa_endpoint_skb_build(endpoint, page, trans->len))
		trans->data = NULL;	 
done:
	ipa_endpoint_replenish(endpoint);
}

void ipa_endpoint_trans_release(struct ipa_endpoint *endpoint,
				struct gsi_trans *trans)
{
	if (endpoint->toward_ipa) {
		struct ipa *ipa = endpoint->ipa;

		 
		if (endpoint != ipa->name_map[IPA_ENDPOINT_AP_COMMAND_TX]) {
			struct sk_buff *skb = trans->data;

			if (skb)
				dev_kfree_skb_any(skb);
		}
	} else {
		struct page *page = trans->data;

		if (page)
			put_page(page);
	}
}

void ipa_endpoint_default_route_set(struct ipa *ipa, u32 endpoint_id)
{
	const struct reg *reg;
	u32 val;

	reg = ipa_reg(ipa, ROUTE);
	 
	val = reg_encode(reg, ROUTE_DEF_PIPE, endpoint_id);
	val |= reg_bit(reg, ROUTE_DEF_HDR_TABLE);
	 
	val |= reg_encode(reg, ROUTE_FRAG_DEF_PIPE, endpoint_id);
	val |= reg_bit(reg, ROUTE_DEF_RETAIN_HDR);

	iowrite32(val, ipa->reg_virt + reg_offset(reg));
}

void ipa_endpoint_default_route_clear(struct ipa *ipa)
{
	ipa_endpoint_default_route_set(ipa, 0);
}

 
static int ipa_endpoint_reset_rx_aggr(struct ipa_endpoint *endpoint)
{
	struct device *dev = &endpoint->ipa->pdev->dev;
	struct ipa *ipa = endpoint->ipa;
	struct gsi *gsi = &ipa->gsi;
	bool suspended = false;
	dma_addr_t addr;
	u32 retries;
	u32 len = 1;
	void *virt;
	int ret;

	virt = kzalloc(len, GFP_KERNEL);
	if (!virt)
		return -ENOMEM;

	addr = dma_map_single(dev, virt, len, DMA_FROM_DEVICE);
	if (dma_mapping_error(dev, addr)) {
		ret = -ENOMEM;
		goto out_kfree;
	}

	 
	ipa_endpoint_force_close(endpoint);

	 
	gsi_channel_reset(gsi, endpoint->channel_id, false);

	 
	suspended = ipa_endpoint_program_suspend(endpoint, false);

	 
	ret = gsi_channel_start(gsi, endpoint->channel_id);
	if (ret)
		goto out_suspend_again;

	ret = gsi_trans_read_byte(gsi, endpoint->channel_id, addr);
	if (ret)
		goto err_endpoint_stop;

	 
	retries = IPA_ENDPOINT_RESET_AGGR_RETRY_MAX;
	do {
		if (!ipa_endpoint_aggr_active(endpoint))
			break;
		usleep_range(USEC_PER_MSEC, 2 * USEC_PER_MSEC);
	} while (retries--);

	 
	if (ipa_endpoint_aggr_active(endpoint))
		dev_err(dev, "endpoint %u still active during reset\n",
			endpoint->endpoint_id);

	gsi_trans_read_byte_done(gsi, endpoint->channel_id);

	ret = gsi_channel_stop(gsi, endpoint->channel_id);
	if (ret)
		goto out_suspend_again;

	 
	gsi_channel_reset(gsi, endpoint->channel_id, true);

	usleep_range(USEC_PER_MSEC, 2 * USEC_PER_MSEC);

	goto out_suspend_again;

err_endpoint_stop:
	(void)gsi_channel_stop(gsi, endpoint->channel_id);
out_suspend_again:
	if (suspended)
		(void)ipa_endpoint_program_suspend(endpoint, true);
	dma_unmap_single(dev, addr, len, DMA_FROM_DEVICE);
out_kfree:
	kfree(virt);

	return ret;
}

static void ipa_endpoint_reset(struct ipa_endpoint *endpoint)
{
	u32 channel_id = endpoint->channel_id;
	struct ipa *ipa = endpoint->ipa;
	bool special;
	int ret = 0;

	 
	special = ipa->version < IPA_VERSION_4_0 && !endpoint->toward_ipa &&
			endpoint->config.aggregation;
	if (special && ipa_endpoint_aggr_active(endpoint))
		ret = ipa_endpoint_reset_rx_aggr(endpoint);
	else
		gsi_channel_reset(&ipa->gsi, channel_id, true);

	if (ret)
		dev_err(&ipa->pdev->dev,
			"error %d resetting channel %u for endpoint %u\n",
			ret, endpoint->channel_id, endpoint->endpoint_id);
}

static void ipa_endpoint_program(struct ipa_endpoint *endpoint)
{
	if (endpoint->toward_ipa) {
		 
		if (endpoint->ipa->version < IPA_VERSION_4_2)
			ipa_endpoint_program_delay(endpoint, false);
	} else {
		 
		(void)ipa_endpoint_program_suspend(endpoint, false);
	}
	ipa_endpoint_init_cfg(endpoint);
	ipa_endpoint_init_nat(endpoint);
	ipa_endpoint_init_hdr(endpoint);
	ipa_endpoint_init_hdr_ext(endpoint);
	ipa_endpoint_init_hdr_metadata_mask(endpoint);
	ipa_endpoint_init_mode(endpoint);
	ipa_endpoint_init_aggr(endpoint);
	if (!endpoint->toward_ipa) {
		if (endpoint->config.rx.holb_drop)
			ipa_endpoint_init_hol_block_enable(endpoint, 0);
		else
			ipa_endpoint_init_hol_block_disable(endpoint);
	}
	ipa_endpoint_init_deaggr(endpoint);
	ipa_endpoint_init_rsrc_grp(endpoint);
	ipa_endpoint_init_seq(endpoint);
	ipa_endpoint_status(endpoint);
}

int ipa_endpoint_enable_one(struct ipa_endpoint *endpoint)
{
	u32 endpoint_id = endpoint->endpoint_id;
	struct ipa *ipa = endpoint->ipa;
	struct gsi *gsi = &ipa->gsi;
	int ret;

	ret = gsi_channel_start(gsi, endpoint->channel_id);
	if (ret) {
		dev_err(&ipa->pdev->dev,
			"error %d starting %cX channel %u for endpoint %u\n",
			ret, endpoint->toward_ipa ? 'T' : 'R',
			endpoint->channel_id, endpoint_id);
		return ret;
	}

	if (!endpoint->toward_ipa) {
		ipa_interrupt_suspend_enable(ipa->interrupt, endpoint_id);
		ipa_endpoint_replenish_enable(endpoint);
	}

	__set_bit(endpoint_id, ipa->enabled);

	return 0;
}

void ipa_endpoint_disable_one(struct ipa_endpoint *endpoint)
{
	u32 endpoint_id = endpoint->endpoint_id;
	struct ipa *ipa = endpoint->ipa;
	struct gsi *gsi = &ipa->gsi;
	int ret;

	if (!test_bit(endpoint_id, ipa->enabled))
		return;

	__clear_bit(endpoint_id, endpoint->ipa->enabled);

	if (!endpoint->toward_ipa) {
		ipa_endpoint_replenish_disable(endpoint);
		ipa_interrupt_suspend_disable(ipa->interrupt, endpoint_id);
	}

	 
	ret = gsi_channel_stop(gsi, endpoint->channel_id);
	if (ret)
		dev_err(&ipa->pdev->dev,
			"error %d attempting to stop endpoint %u\n", ret,
			endpoint_id);
}

void ipa_endpoint_suspend_one(struct ipa_endpoint *endpoint)
{
	struct device *dev = &endpoint->ipa->pdev->dev;
	struct gsi *gsi = &endpoint->ipa->gsi;
	int ret;

	if (!test_bit(endpoint->endpoint_id, endpoint->ipa->enabled))
		return;

	if (!endpoint->toward_ipa) {
		ipa_endpoint_replenish_disable(endpoint);
		(void)ipa_endpoint_program_suspend(endpoint, true);
	}

	ret = gsi_channel_suspend(gsi, endpoint->channel_id);
	if (ret)
		dev_err(dev, "error %d suspending channel %u\n", ret,
			endpoint->channel_id);
}

void ipa_endpoint_resume_one(struct ipa_endpoint *endpoint)
{
	struct device *dev = &endpoint->ipa->pdev->dev;
	struct gsi *gsi = &endpoint->ipa->gsi;
	int ret;

	if (!test_bit(endpoint->endpoint_id, endpoint->ipa->enabled))
		return;

	if (!endpoint->toward_ipa)
		(void)ipa_endpoint_program_suspend(endpoint, false);

	ret = gsi_channel_resume(gsi, endpoint->channel_id);
	if (ret)
		dev_err(dev, "error %d resuming channel %u\n", ret,
			endpoint->channel_id);
	else if (!endpoint->toward_ipa)
		ipa_endpoint_replenish_enable(endpoint);
}

void ipa_endpoint_suspend(struct ipa *ipa)
{
	if (!ipa->setup_complete)
		return;

	if (ipa->modem_netdev)
		ipa_modem_suspend(ipa->modem_netdev);

	ipa_endpoint_suspend_one(ipa->name_map[IPA_ENDPOINT_AP_LAN_RX]);
	ipa_endpoint_suspend_one(ipa->name_map[IPA_ENDPOINT_AP_COMMAND_TX]);
}

void ipa_endpoint_resume(struct ipa *ipa)
{
	if (!ipa->setup_complete)
		return;

	ipa_endpoint_resume_one(ipa->name_map[IPA_ENDPOINT_AP_COMMAND_TX]);
	ipa_endpoint_resume_one(ipa->name_map[IPA_ENDPOINT_AP_LAN_RX]);

	if (ipa->modem_netdev)
		ipa_modem_resume(ipa->modem_netdev);
}

static void ipa_endpoint_setup_one(struct ipa_endpoint *endpoint)
{
	struct gsi *gsi = &endpoint->ipa->gsi;
	u32 channel_id = endpoint->channel_id;

	 
	if (endpoint->ee_id != GSI_EE_AP)
		return;

	endpoint->skb_frag_max = gsi->channel[channel_id].trans_tre_max - 1;
	if (!endpoint->toward_ipa) {
		 
		clear_bit(IPA_REPLENISH_ENABLED, endpoint->replenish_flags);
		clear_bit(IPA_REPLENISH_ACTIVE, endpoint->replenish_flags);
		INIT_DELAYED_WORK(&endpoint->replenish_work,
				  ipa_endpoint_replenish_work);
	}

	ipa_endpoint_program(endpoint);

	__set_bit(endpoint->endpoint_id, endpoint->ipa->set_up);
}

static void ipa_endpoint_teardown_one(struct ipa_endpoint *endpoint)
{
	__clear_bit(endpoint->endpoint_id, endpoint->ipa->set_up);

	if (!endpoint->toward_ipa)
		cancel_delayed_work_sync(&endpoint->replenish_work);

	ipa_endpoint_reset(endpoint);
}

void ipa_endpoint_setup(struct ipa *ipa)
{
	u32 endpoint_id;

	for_each_set_bit(endpoint_id, ipa->defined, ipa->endpoint_count)
		ipa_endpoint_setup_one(&ipa->endpoint[endpoint_id]);
}

void ipa_endpoint_teardown(struct ipa *ipa)
{
	u32 endpoint_id;

	for_each_set_bit(endpoint_id, ipa->set_up, ipa->endpoint_count)
		ipa_endpoint_teardown_one(&ipa->endpoint[endpoint_id]);
}

void ipa_endpoint_deconfig(struct ipa *ipa)
{
	ipa->available_count = 0;
	bitmap_free(ipa->available);
	ipa->available = NULL;
}

int ipa_endpoint_config(struct ipa *ipa)
{
	struct device *dev = &ipa->pdev->dev;
	const struct reg *reg;
	u32 endpoint_id;
	u32 hw_limit;
	u32 tx_count;
	u32 rx_count;
	u32 rx_base;
	u32 limit;
	u32 val;

	 
	if (ipa->version < IPA_VERSION_3_5) {
		ipa->available = bitmap_zalloc(IPA_ENDPOINT_MAX, GFP_KERNEL);
		if (!ipa->available)
			return -ENOMEM;
		ipa->available_count = IPA_ENDPOINT_MAX;

		bitmap_set(ipa->available, 0, IPA_ENDPOINT_MAX);

		return 0;
	}

	 
	reg = ipa_reg(ipa, FLAVOR_0);
	val = ioread32(ipa->reg_virt + reg_offset(reg));

	 
	tx_count = reg_decode(reg, MAX_CONS_PIPES, val);
	rx_count = reg_decode(reg, MAX_PROD_PIPES, val);
	rx_base = reg_decode(reg, PROD_LOWEST, val);

	limit = rx_base + rx_count;
	if (limit > IPA_ENDPOINT_MAX) {
		dev_err(dev, "too many endpoints, %u > %u\n",
			limit, IPA_ENDPOINT_MAX);
		return -EINVAL;
	}

	 
	hw_limit = ipa->version < IPA_VERSION_5_0 ? 32 : U8_MAX + 1;
	if (limit > hw_limit) {
		dev_err(dev, "unexpected endpoint count, %u > %u\n",
			limit, hw_limit);
		return -EINVAL;
	}

	 
	ipa->available = bitmap_zalloc(limit, GFP_KERNEL);
	if (!ipa->available)
		return -ENOMEM;
	ipa->available_count = limit;

	 
	bitmap_set(ipa->available, 0, tx_count);
	bitmap_set(ipa->available, rx_base, rx_count);

	for_each_set_bit(endpoint_id, ipa->defined, ipa->endpoint_count) {
		struct ipa_endpoint *endpoint;

		if (endpoint_id >= limit) {
			dev_err(dev, "invalid endpoint id, %u > %u\n",
				endpoint_id, limit - 1);
			goto err_free_bitmap;
		}

		if (!test_bit(endpoint_id, ipa->available)) {
			dev_err(dev, "unavailable endpoint id %u\n",
				endpoint_id);
			goto err_free_bitmap;
		}

		 
		endpoint = &ipa->endpoint[endpoint_id];
		if (endpoint->toward_ipa) {
			if (endpoint_id < tx_count)
				continue;
		} else if (endpoint_id >= rx_base) {
			continue;
		}

		dev_err(dev, "endpoint id %u wrong direction\n", endpoint_id);
		goto err_free_bitmap;
	}

	return 0;

err_free_bitmap:
	ipa_endpoint_deconfig(ipa);

	return -EINVAL;
}

static void ipa_endpoint_init_one(struct ipa *ipa, enum ipa_endpoint_name name,
				  const struct ipa_gsi_endpoint_data *data)
{
	struct ipa_endpoint *endpoint;

	endpoint = &ipa->endpoint[data->endpoint_id];

	if (data->ee_id == GSI_EE_AP)
		ipa->channel_map[data->channel_id] = endpoint;
	ipa->name_map[name] = endpoint;

	endpoint->ipa = ipa;
	endpoint->ee_id = data->ee_id;
	endpoint->channel_id = data->channel_id;
	endpoint->endpoint_id = data->endpoint_id;
	endpoint->toward_ipa = data->toward_ipa;
	endpoint->config = data->endpoint.config;

	__set_bit(endpoint->endpoint_id, ipa->defined);
}

static void ipa_endpoint_exit_one(struct ipa_endpoint *endpoint)
{
	__clear_bit(endpoint->endpoint_id, endpoint->ipa->defined);

	memset(endpoint, 0, sizeof(*endpoint));
}

void ipa_endpoint_exit(struct ipa *ipa)
{
	u32 endpoint_id;

	ipa->filtered = 0;

	for_each_set_bit(endpoint_id, ipa->defined, ipa->endpoint_count)
		ipa_endpoint_exit_one(&ipa->endpoint[endpoint_id]);

	bitmap_free(ipa->enabled);
	ipa->enabled = NULL;
	bitmap_free(ipa->set_up);
	ipa->set_up = NULL;
	bitmap_free(ipa->defined);
	ipa->defined = NULL;

	memset(ipa->name_map, 0, sizeof(ipa->name_map));
	memset(ipa->channel_map, 0, sizeof(ipa->channel_map));
}

 
int ipa_endpoint_init(struct ipa *ipa, u32 count,
		      const struct ipa_gsi_endpoint_data *data)
{
	enum ipa_endpoint_name name;
	u32 filtered;

	BUILD_BUG_ON(!IPA_REPLENISH_BATCH);

	 
	ipa->endpoint_count = ipa_endpoint_max(ipa, count, data) + 1;
	if (!ipa->endpoint_count)
		return -EINVAL;

	 
	ipa->defined = bitmap_zalloc(ipa->endpoint_count, GFP_KERNEL);
	if (!ipa->defined)
		return -ENOMEM;

	ipa->set_up = bitmap_zalloc(ipa->endpoint_count, GFP_KERNEL);
	if (!ipa->set_up)
		goto err_free_defined;

	ipa->enabled = bitmap_zalloc(ipa->endpoint_count, GFP_KERNEL);
	if (!ipa->enabled)
		goto err_free_set_up;

	filtered = 0;
	for (name = 0; name < count; name++, data++) {
		if (ipa_gsi_endpoint_data_empty(data))
			continue;	 

		ipa_endpoint_init_one(ipa, name, data);

		if (data->endpoint.filter_support)
			filtered |= BIT(data->endpoint_id);
		if (data->ee_id == GSI_EE_MODEM && data->toward_ipa)
			ipa->modem_tx_count++;
	}

	 
	if (!ipa_filtered_valid(ipa, filtered)) {
		ipa_endpoint_exit(ipa);

		return -EINVAL;
	}

	ipa->filtered = filtered;

	return 0;

err_free_set_up:
	bitmap_free(ipa->set_up);
	ipa->set_up = NULL;
err_free_defined:
	bitmap_free(ipa->defined);
	ipa->defined = NULL;

	return -ENOMEM;
}
