

 

#include <linux/types.h>
#include <linux/kernel.h>
#include <linux/bits.h>
#include <linux/bitops.h>
#include <linux/bitfield.h>
#include <linux/io.h>
#include <linux/build_bug.h>
#include <linux/device.h>
#include <linux/dma-mapping.h>

#include "ipa.h"
#include "ipa_version.h"
#include "ipa_endpoint.h"
#include "ipa_table.h"
#include "ipa_reg.h"
#include "ipa_mem.h"
#include "ipa_cmd.h"
#include "gsi.h"
#include "gsi_trans.h"

 

 
#define IPA_ZERO_RULE_SIZE		(2 * sizeof(__le32))

 
static void ipa_table_validate_build(void)
{
	 
	BUILD_BUG_ON(sizeof(dma_addr_t) > sizeof(__le64));

	 
	BUILD_BUG_ON(IPA_ZERO_RULE_SIZE != sizeof(__le64));
}

static const struct ipa_mem *
ipa_table_mem(struct ipa *ipa, bool filter, bool hashed, bool ipv6)
{
	enum ipa_mem_id mem_id;

	mem_id = filter ? hashed ? ipv6 ? IPA_MEM_V6_FILTER_HASHED
					: IPA_MEM_V4_FILTER_HASHED
				 : ipv6 ? IPA_MEM_V6_FILTER
					: IPA_MEM_V4_FILTER
			: hashed ? ipv6 ? IPA_MEM_V6_ROUTE_HASHED
					: IPA_MEM_V4_ROUTE_HASHED
				 : ipv6 ? IPA_MEM_V6_ROUTE
					: IPA_MEM_V4_ROUTE;

	return ipa_mem_find(ipa, mem_id);
}

bool ipa_filtered_valid(struct ipa *ipa, u64 filtered)
{
	struct device *dev = &ipa->pdev->dev;
	u32 count;

	if (!filtered) {
		dev_err(dev, "at least one filtering endpoint is required\n");

		return false;
	}

	count = hweight64(filtered);
	if (count > ipa->filter_count) {
		dev_err(dev, "too many filtering endpoints (%u > %u)\n",
			count, ipa->filter_count);

		return false;
	}

	return true;
}

 
static dma_addr_t ipa_table_addr(struct ipa *ipa, bool filter_mask, u16 count)
{
	u32 skip;

	if (!count)
		return 0;

	WARN_ON(count > max_t(u32, ipa->filter_count, ipa->route_count));

	 
	skip = filter_mask ? 1 : 2;

	return ipa->table_addr + skip * sizeof(*ipa->table_virt);
}

static void ipa_table_reset_add(struct gsi_trans *trans, bool filter,
				bool hashed, bool ipv6, u16 first, u16 count)
{
	struct ipa *ipa = container_of(trans->gsi, struct ipa, gsi);
	const struct ipa_mem *mem;
	dma_addr_t addr;
	u32 offset;
	u16 size;

	 
	mem = ipa_table_mem(ipa, filter, hashed, ipv6);
	if (!mem || !mem->size)
		return;

	if (filter)
		first++;	 

	offset = mem->offset + first * sizeof(__le64);
	size = count * sizeof(__le64);
	addr = ipa_table_addr(ipa, false, count);

	ipa_cmd_dma_shared_mem_add(trans, offset, size, addr, true);
}

 
static int
ipa_filter_reset_table(struct ipa *ipa, bool hashed, bool ipv6, bool modem)
{
	u64 ep_mask = ipa->filtered;
	struct gsi_trans *trans;
	enum gsi_ee_id ee_id;

	trans = ipa_cmd_trans_alloc(ipa, hweight64(ep_mask));
	if (!trans) {
		dev_err(&ipa->pdev->dev,
			"no transaction for %s filter reset\n",
			modem ? "modem" : "AP");
		return -EBUSY;
	}

	ee_id = modem ? GSI_EE_MODEM : GSI_EE_AP;
	while (ep_mask) {
		u32 endpoint_id = __ffs(ep_mask);
		struct ipa_endpoint *endpoint;

		ep_mask ^= BIT(endpoint_id);

		endpoint = &ipa->endpoint[endpoint_id];
		if (endpoint->ee_id != ee_id)
			continue;

		ipa_table_reset_add(trans, true, hashed, ipv6, endpoint_id, 1);
	}

	gsi_trans_commit_wait(trans);

	return 0;
}

 
static int ipa_filter_reset(struct ipa *ipa, bool modem)
{
	int ret;

	ret = ipa_filter_reset_table(ipa, false, false, modem);
	if (ret)
		return ret;

	ret = ipa_filter_reset_table(ipa, false, true, modem);
	if (ret || !ipa_table_hash_support(ipa))
		return ret;

	ret = ipa_filter_reset_table(ipa, true, false, modem);
	if (ret)
		return ret;

	return ipa_filter_reset_table(ipa, true, true, modem);
}

 
static int ipa_route_reset(struct ipa *ipa, bool modem)
{
	bool hash_support = ipa_table_hash_support(ipa);
	u32 modem_route_count = ipa->modem_route_count;
	struct gsi_trans *trans;
	u16 first;
	u16 count;

	trans = ipa_cmd_trans_alloc(ipa, hash_support ? 4 : 2);
	if (!trans) {
		dev_err(&ipa->pdev->dev,
			"no transaction for %s route reset\n",
			modem ? "modem" : "AP");
		return -EBUSY;
	}

	if (modem) {
		first = 0;
		count = modem_route_count;
	} else {
		first = modem_route_count;
		count = ipa->route_count - modem_route_count;
	}

	ipa_table_reset_add(trans, false, false, false, first, count);
	ipa_table_reset_add(trans, false, false, true, first, count);

	if (hash_support) {
		ipa_table_reset_add(trans, false, true, false, first, count);
		ipa_table_reset_add(trans, false, true, true, first, count);
	}

	gsi_trans_commit_wait(trans);

	return 0;
}

void ipa_table_reset(struct ipa *ipa, bool modem)
{
	struct device *dev = &ipa->pdev->dev;
	const char *ee_name;
	int ret;

	ee_name = modem ? "modem" : "AP";

	 
	ret = ipa_filter_reset(ipa, modem);
	if (ret)
		dev_err(dev, "error %d resetting filter table for %s\n",
				ret, ee_name);

	ret = ipa_route_reset(ipa, modem);
	if (ret)
		dev_err(dev, "error %d resetting route table for %s\n",
				ret, ee_name);
}

int ipa_table_hash_flush(struct ipa *ipa)
{
	struct gsi_trans *trans;
	const struct reg *reg;
	u32 val;

	if (!ipa_table_hash_support(ipa))
		return 0;

	trans = ipa_cmd_trans_alloc(ipa, 1);
	if (!trans) {
		dev_err(&ipa->pdev->dev, "no transaction for hash flush\n");
		return -EBUSY;
	}

	if (ipa->version < IPA_VERSION_5_0) {
		reg = ipa_reg(ipa, FILT_ROUT_HASH_FLUSH);

		val = reg_bit(reg, IPV6_ROUTER_HASH);
		val |= reg_bit(reg, IPV6_FILTER_HASH);
		val |= reg_bit(reg, IPV4_ROUTER_HASH);
		val |= reg_bit(reg, IPV4_FILTER_HASH);
	} else {
		reg = ipa_reg(ipa, FILT_ROUT_CACHE_FLUSH);

		 
		val = reg_bit(reg, ROUTER_CACHE);
		val |= reg_bit(reg, FILTER_CACHE);
	}

	ipa_cmd_register_write_add(trans, reg_offset(reg), val, val, false);

	gsi_trans_commit_wait(trans);

	return 0;
}

static void ipa_table_init_add(struct gsi_trans *trans, bool filter, bool ipv6)
{
	struct ipa *ipa = container_of(trans->gsi, struct ipa, gsi);
	const struct ipa_mem *hash_mem;
	enum ipa_cmd_opcode opcode;
	const struct ipa_mem *mem;
	dma_addr_t hash_addr;
	dma_addr_t addr;
	u32 hash_offset;
	u32 zero_offset;
	u16 hash_count;
	u32 zero_size;
	u16 hash_size;
	u16 count;
	u16 size;

	opcode = filter ? ipv6 ? IPA_CMD_IP_V6_FILTER_INIT
			       : IPA_CMD_IP_V4_FILTER_INIT
			: ipv6 ? IPA_CMD_IP_V6_ROUTING_INIT
			       : IPA_CMD_IP_V4_ROUTING_INIT;

	 
	mem = ipa_table_mem(ipa, filter, false, ipv6);
	hash_mem = ipa_table_mem(ipa, filter, true, ipv6);
	hash_offset = hash_mem ? hash_mem->offset : 0;

	 
	if (filter) {
		 
		count = 1 + hweight64(ipa->filtered);
		hash_count = hash_mem && hash_mem->size ? count : 0;
	} else {
		 
		count = mem->size / sizeof(__le64);
		hash_count = hash_mem ? hash_mem->size / sizeof(__le64) : 0;
	}
	size = count * sizeof(__le64);
	hash_size = hash_count * sizeof(__le64);

	addr = ipa_table_addr(ipa, filter, count);
	hash_addr = ipa_table_addr(ipa, filter, hash_count);

	ipa_cmd_table_init_add(trans, opcode, size, mem->offset, addr,
			       hash_size, hash_offset, hash_addr);
	if (!filter)
		return;

	 
	zero_offset = mem->offset + size;
	zero_size = mem->size - size;
	ipa_cmd_dma_shared_mem_add(trans, zero_offset, zero_size,
				   ipa->zero_addr, true);
	if (!hash_size)
		return;

	 
	zero_offset = hash_offset + hash_size;
	zero_size = hash_mem->size - hash_size;
	ipa_cmd_dma_shared_mem_add(trans, zero_offset, zero_size,
				   ipa->zero_addr, true);
}

int ipa_table_setup(struct ipa *ipa)
{
	struct gsi_trans *trans;

	 
	trans = ipa_cmd_trans_alloc(ipa, 8);
	if (!trans) {
		dev_err(&ipa->pdev->dev, "no transaction for table setup\n");
		return -EBUSY;
	}

	ipa_table_init_add(trans, false, false);
	ipa_table_init_add(trans, false, true);
	ipa_table_init_add(trans, true, false);
	ipa_table_init_add(trans, true, true);

	gsi_trans_commit_wait(trans);

	return 0;
}

 
static void ipa_filter_tuple_zero(struct ipa_endpoint *endpoint)
{
	u32 endpoint_id = endpoint->endpoint_id;
	struct ipa *ipa = endpoint->ipa;
	const struct reg *reg;
	u32 offset;
	u32 val;

	if (ipa->version < IPA_VERSION_5_0) {
		reg = ipa_reg(ipa, ENDP_FILTER_ROUTER_HSH_CFG);

		offset = reg_n_offset(reg, endpoint_id);
		val = ioread32(endpoint->ipa->reg_virt + offset);

		 
		val &= ~reg_fmask(reg, FILTER_HASH_MSK_ALL);
	} else {
		 
		reg = ipa_reg(ipa, ENDP_FILTER_CACHE_CFG);
		offset = reg_n_offset(reg, endpoint_id);

		 
		val = 0;
	}

	iowrite32(val, endpoint->ipa->reg_virt + offset);
}

 
static void ipa_filter_config(struct ipa *ipa, bool modem)
{
	enum gsi_ee_id ee_id = modem ? GSI_EE_MODEM : GSI_EE_AP;
	u64 ep_mask = ipa->filtered;

	if (!ipa_table_hash_support(ipa))
		return;

	while (ep_mask) {
		u32 endpoint_id = __ffs(ep_mask);
		struct ipa_endpoint *endpoint;

		ep_mask ^= BIT(endpoint_id);

		endpoint = &ipa->endpoint[endpoint_id];
		if (endpoint->ee_id == ee_id)
			ipa_filter_tuple_zero(endpoint);
	}
}

static bool ipa_route_id_modem(struct ipa *ipa, u32 route_id)
{
	return route_id < ipa->modem_route_count;
}

 
static void ipa_route_tuple_zero(struct ipa *ipa, u32 route_id)
{
	const struct reg *reg;
	u32 offset;
	u32 val;

	if (ipa->version < IPA_VERSION_5_0) {
		reg = ipa_reg(ipa, ENDP_FILTER_ROUTER_HSH_CFG);
		offset = reg_n_offset(reg, route_id);

		val = ioread32(ipa->reg_virt + offset);

		 
		val &= ~reg_fmask(reg, ROUTER_HASH_MSK_ALL);
	} else {
		 
		reg = ipa_reg(ipa, ENDP_ROUTER_CACHE_CFG);
		offset = reg_n_offset(reg, route_id);

		 
		val = 0;
	}

	iowrite32(val, ipa->reg_virt + offset);
}

 
static void ipa_route_config(struct ipa *ipa, bool modem)
{
	u32 route_id;

	if (!ipa_table_hash_support(ipa))
		return;

	for (route_id = 0; route_id < ipa->route_count; route_id++)
		if (ipa_route_id_modem(ipa, route_id) == modem)
			ipa_route_tuple_zero(ipa, route_id);
}

 
void ipa_table_config(struct ipa *ipa)
{
	ipa_filter_config(ipa, false);
	ipa_filter_config(ipa, true);
	ipa_route_config(ipa, false);
	ipa_route_config(ipa, true);
}

 
bool ipa_table_mem_valid(struct ipa *ipa, bool filter)
{
	bool hash_support = ipa_table_hash_support(ipa);
	const struct ipa_mem *mem_hashed;
	const struct ipa_mem *mem_ipv4;
	const struct ipa_mem *mem_ipv6;
	u32 count;

	 
	mem_ipv4 = ipa_table_mem(ipa, filter, false, false);
	if (!mem_ipv4)
		return false;

	mem_ipv6 = ipa_table_mem(ipa, filter, false, true);
	if (!mem_ipv6)
		return false;

	if (mem_ipv4->size != mem_ipv6->size)
		return false;

	 
	count = mem_ipv4->size / sizeof(__le64);
	if (count < 2)
		return false;
	if (filter)
		ipa->filter_count = count - 1;	 
	else
		ipa->route_count = count;

	 
	if (!ipa_cmd_table_init_valid(ipa, mem_ipv4, !filter))
		return false;

	 
	if (filter) {
		 
		if (count < 1 + hweight64(ipa->filtered))
			return false;
	} else {
		 
		if (count < ipa->modem_route_count + 1)
			return false;
	}

	 
	mem_hashed = ipa_table_mem(ipa, filter, true, false);
	if (hash_support) {
		if (!mem_hashed || mem_hashed->size != mem_ipv4->size)
			return false;
	} else {
		if (mem_hashed && mem_hashed->size)
			return false;
	}

	 
	mem_hashed = ipa_table_mem(ipa, filter, true, true);
	if (hash_support) {
		if (!mem_hashed || mem_hashed->size != mem_ipv6->size)
			return false;
	} else {
		if (mem_hashed && mem_hashed->size)
			return false;
	}

	return true;
}

 
int ipa_table_init(struct ipa *ipa)
{
	struct device *dev = &ipa->pdev->dev;
	dma_addr_t addr;
	__le64 le_addr;
	__le64 *virt;
	size_t size;
	u32 count;

	ipa_table_validate_build();

	count = max_t(u32, ipa->filter_count, ipa->route_count);

	 
	size = IPA_ZERO_RULE_SIZE + (1 + count) * sizeof(__le64);
	virt = dma_alloc_coherent(dev, size, &addr, GFP_KERNEL);
	if (!virt)
		return -ENOMEM;

	ipa->table_virt = virt;
	ipa->table_addr = addr;

	 
	*virt++ = 0;

	 
	if (ipa->version < IPA_VERSION_5_0)
		*virt++ = cpu_to_le64(ipa->filtered << 1);
	else
		*virt++ = cpu_to_le64(ipa->filtered);

	 
	le_addr = cpu_to_le64(addr);
	while (count--)
		*virt++ = le_addr;

	return 0;
}

void ipa_table_exit(struct ipa *ipa)
{
	u32 count = max_t(u32, 1 + ipa->filter_count, ipa->route_count);
	struct device *dev = &ipa->pdev->dev;
	size_t size;

	size = IPA_ZERO_RULE_SIZE + (1 + count) * sizeof(__le64);

	dma_free_coherent(dev, size, ipa->table_virt, ipa->table_addr);
	ipa->table_addr = 0;
	ipa->table_virt = NULL;
}
