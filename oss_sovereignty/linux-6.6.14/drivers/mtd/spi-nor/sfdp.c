
 

#include <linux/bitfield.h>
#include <linux/mtd/spi-nor.h>
#include <linux/slab.h>
#include <linux/sort.h>

#include "core.h"

#define SFDP_PARAM_HEADER_ID(p)	(((p)->id_msb << 8) | (p)->id_lsb)
#define SFDP_PARAM_HEADER_PTP(p) \
	(((p)->parameter_table_pointer[2] << 16) | \
	 ((p)->parameter_table_pointer[1] <<  8) | \
	 ((p)->parameter_table_pointer[0] <<  0))
#define SFDP_PARAM_HEADER_PARAM_LEN(p) ((p)->length * 4)

#define SFDP_BFPT_ID		0xff00	 
#define SFDP_SECTOR_MAP_ID	0xff81	 
#define SFDP_4BAIT_ID		0xff84   
#define SFDP_PROFILE1_ID	0xff05	 
#define SFDP_SCCR_MAP_ID	0xff87	 
#define SFDP_SCCR_MAP_MC_ID	0xff88	 

#define SFDP_SIGNATURE		0x50444653U

struct sfdp_header {
	u32		signature;  
	u8		minor;
	u8		major;
	u8		nph;  
	u8		unused;

	 
	struct sfdp_parameter_header	bfpt_header;
};

 
struct sfdp_bfpt_read {
	 
	u32			hwcaps;

	 
	u32			supported_dword;
	u32			supported_bit;

	 
	u32			settings_dword;
	u32			settings_shift;

	 
	enum spi_nor_protocol	proto;
};

struct sfdp_bfpt_erase {
	 
	u32			dword;
	u32			shift;
};

#define SMPT_CMD_ADDRESS_LEN_MASK		GENMASK(23, 22)
#define SMPT_CMD_ADDRESS_LEN_0			(0x0UL << 22)
#define SMPT_CMD_ADDRESS_LEN_3			(0x1UL << 22)
#define SMPT_CMD_ADDRESS_LEN_4			(0x2UL << 22)
#define SMPT_CMD_ADDRESS_LEN_USE_CURRENT	(0x3UL << 22)

#define SMPT_CMD_READ_DUMMY_MASK		GENMASK(19, 16)
#define SMPT_CMD_READ_DUMMY_SHIFT		16
#define SMPT_CMD_READ_DUMMY(_cmd) \
	(((_cmd) & SMPT_CMD_READ_DUMMY_MASK) >> SMPT_CMD_READ_DUMMY_SHIFT)
#define SMPT_CMD_READ_DUMMY_IS_VARIABLE		0xfUL

#define SMPT_CMD_READ_DATA_MASK			GENMASK(31, 24)
#define SMPT_CMD_READ_DATA_SHIFT		24
#define SMPT_CMD_READ_DATA(_cmd) \
	(((_cmd) & SMPT_CMD_READ_DATA_MASK) >> SMPT_CMD_READ_DATA_SHIFT)

#define SMPT_CMD_OPCODE_MASK			GENMASK(15, 8)
#define SMPT_CMD_OPCODE_SHIFT			8
#define SMPT_CMD_OPCODE(_cmd) \
	(((_cmd) & SMPT_CMD_OPCODE_MASK) >> SMPT_CMD_OPCODE_SHIFT)

#define SMPT_MAP_REGION_COUNT_MASK		GENMASK(23, 16)
#define SMPT_MAP_REGION_COUNT_SHIFT		16
#define SMPT_MAP_REGION_COUNT(_header) \
	((((_header) & SMPT_MAP_REGION_COUNT_MASK) >> \
	  SMPT_MAP_REGION_COUNT_SHIFT) + 1)

#define SMPT_MAP_ID_MASK			GENMASK(15, 8)
#define SMPT_MAP_ID_SHIFT			8
#define SMPT_MAP_ID(_header) \
	(((_header) & SMPT_MAP_ID_MASK) >> SMPT_MAP_ID_SHIFT)

#define SMPT_MAP_REGION_SIZE_MASK		GENMASK(31, 8)
#define SMPT_MAP_REGION_SIZE_SHIFT		8
#define SMPT_MAP_REGION_SIZE(_region) \
	(((((_region) & SMPT_MAP_REGION_SIZE_MASK) >> \
	   SMPT_MAP_REGION_SIZE_SHIFT) + 1) * 256)

#define SMPT_MAP_REGION_ERASE_TYPE_MASK		GENMASK(3, 0)
#define SMPT_MAP_REGION_ERASE_TYPE(_region) \
	((_region) & SMPT_MAP_REGION_ERASE_TYPE_MASK)

#define SMPT_DESC_TYPE_MAP			BIT(1)
#define SMPT_DESC_END				BIT(0)

#define SFDP_4BAIT_DWORD_MAX	2

struct sfdp_4bait {
	 
	u32		hwcaps;

	 
	u32		supported_bit;
};

 
static int spi_nor_read_raw(struct spi_nor *nor, u32 addr, size_t len, u8 *buf)
{
	ssize_t ret;

	while (len) {
		ret = spi_nor_read_data(nor, addr, len, buf);
		if (ret < 0)
			return ret;
		if (!ret || ret > len)
			return -EIO;

		buf += ret;
		addr += ret;
		len -= ret;
	}
	return 0;
}

 
static int spi_nor_read_sfdp(struct spi_nor *nor, u32 addr,
			     size_t len, void *buf)
{
	u8 addr_nbytes, read_opcode, read_dummy;
	int ret;

	read_opcode = nor->read_opcode;
	addr_nbytes = nor->addr_nbytes;
	read_dummy = nor->read_dummy;

	nor->read_opcode = SPINOR_OP_RDSFDP;
	nor->addr_nbytes = 3;
	nor->read_dummy = 8;

	ret = spi_nor_read_raw(nor, addr, len, buf);

	nor->read_opcode = read_opcode;
	nor->addr_nbytes = addr_nbytes;
	nor->read_dummy = read_dummy;

	return ret;
}

 
static int spi_nor_read_sfdp_dma_unsafe(struct spi_nor *nor, u32 addr,
					size_t len, void *buf)
{
	void *dma_safe_buf;
	int ret;

	dma_safe_buf = kmalloc(len, GFP_KERNEL);
	if (!dma_safe_buf)
		return -ENOMEM;

	ret = spi_nor_read_sfdp(nor, addr, len, dma_safe_buf);
	memcpy(buf, dma_safe_buf, len);
	kfree(dma_safe_buf);

	return ret;
}

static void
spi_nor_set_read_settings_from_bfpt(struct spi_nor_read_command *read,
				    u16 half,
				    enum spi_nor_protocol proto)
{
	read->num_mode_clocks = (half >> 5) & 0x07;
	read->num_wait_states = (half >> 0) & 0x1f;
	read->opcode = (half >> 8) & 0xff;
	read->proto = proto;
}

static const struct sfdp_bfpt_read sfdp_bfpt_reads[] = {
	 
	{
		SNOR_HWCAPS_READ_1_1_2,
		SFDP_DWORD(1), BIT(16),	 
		SFDP_DWORD(4), 0,	 
		SNOR_PROTO_1_1_2,
	},

	 
	{
		SNOR_HWCAPS_READ_1_2_2,
		SFDP_DWORD(1), BIT(20),	 
		SFDP_DWORD(4), 16,	 
		SNOR_PROTO_1_2_2,
	},

	 
	{
		SNOR_HWCAPS_READ_2_2_2,
		SFDP_DWORD(5),  BIT(0),	 
		SFDP_DWORD(6), 16,	 
		SNOR_PROTO_2_2_2,
	},

	 
	{
		SNOR_HWCAPS_READ_1_1_4,
		SFDP_DWORD(1), BIT(22),	 
		SFDP_DWORD(3), 16,	 
		SNOR_PROTO_1_1_4,
	},

	 
	{
		SNOR_HWCAPS_READ_1_4_4,
		SFDP_DWORD(1), BIT(21),	 
		SFDP_DWORD(3), 0,	 
		SNOR_PROTO_1_4_4,
	},

	 
	{
		SNOR_HWCAPS_READ_4_4_4,
		SFDP_DWORD(5), BIT(4),	 
		SFDP_DWORD(7), 16,	 
		SNOR_PROTO_4_4_4,
	},
};

static const struct sfdp_bfpt_erase sfdp_bfpt_erases[] = {
	 
	{SFDP_DWORD(8), 0},

	 
	{SFDP_DWORD(8), 16},

	 
	{SFDP_DWORD(9), 0},

	 
	{SFDP_DWORD(9), 16},
};

 
static void
spi_nor_set_erase_settings_from_bfpt(struct spi_nor_erase_type *erase,
				     u32 size, u8 opcode, u8 i)
{
	erase->idx = i;
	spi_nor_set_erase_type(erase, size, opcode);
}

 
static int spi_nor_map_cmp_erase_type(const void *l, const void *r)
{
	const struct spi_nor_erase_type *left = l, *right = r;

	return left->size - right->size;
}

 
static u8 spi_nor_sort_erase_mask(struct spi_nor_erase_map *map, u8 erase_mask)
{
	struct spi_nor_erase_type *erase_type = map->erase_type;
	int i;
	u8 sorted_erase_mask = 0;

	if (!erase_mask)
		return 0;

	 
	for (i = 0; i < SNOR_ERASE_TYPE_MAX; i++)
		if (erase_type[i].size && erase_mask & BIT(erase_type[i].idx))
			sorted_erase_mask |= BIT(i);

	return sorted_erase_mask;
}

 
static void spi_nor_regions_sort_erase_types(struct spi_nor_erase_map *map)
{
	struct spi_nor_erase_region *region = map->regions;
	u8 region_erase_mask, sorted_erase_mask;

	while (region) {
		region_erase_mask = region->offset & SNOR_ERASE_TYPE_MASK;

		sorted_erase_mask = spi_nor_sort_erase_mask(map,
							    region_erase_mask);

		 
		region->offset = (region->offset & ~SNOR_ERASE_TYPE_MASK) |
				 sorted_erase_mask;

		region = spi_nor_region_next(region);
	}
}

 
static int spi_nor_parse_bfpt(struct spi_nor *nor,
			      const struct sfdp_parameter_header *bfpt_header)
{
	struct spi_nor_flash_parameter *params = nor->params;
	struct spi_nor_erase_map *map = &params->erase_map;
	struct spi_nor_erase_type *erase_type = map->erase_type;
	struct sfdp_bfpt bfpt;
	size_t len;
	int i, cmd, err;
	u32 addr, val;
	u32 dword;
	u16 half;
	u8 erase_mask;

	 
	if (bfpt_header->length < BFPT_DWORD_MAX_JESD216)
		return -EINVAL;

	 
	len = min_t(size_t, sizeof(bfpt),
		    bfpt_header->length * sizeof(u32));
	addr = SFDP_PARAM_HEADER_PTP(bfpt_header);
	memset(&bfpt, 0, sizeof(bfpt));
	err = spi_nor_read_sfdp_dma_unsafe(nor,  addr, len, &bfpt);
	if (err < 0)
		return err;

	 
	le32_to_cpu_array(bfpt.dwords, BFPT_DWORD_MAX);

	 
	switch (bfpt.dwords[SFDP_DWORD(1)] & BFPT_DWORD1_ADDRESS_BYTES_MASK) {
	case BFPT_DWORD1_ADDRESS_BYTES_3_ONLY:
	case BFPT_DWORD1_ADDRESS_BYTES_3_OR_4:
		params->addr_nbytes = 3;
		params->addr_mode_nbytes = 3;
		break;

	case BFPT_DWORD1_ADDRESS_BYTES_4_ONLY:
		params->addr_nbytes = 4;
		params->addr_mode_nbytes = 4;
		break;

	default:
		break;
	}

	 
	val = bfpt.dwords[SFDP_DWORD(2)];
	if (val & BIT(31)) {
		val &= ~BIT(31);

		 
		if (val > 63)
			return -EINVAL;

		params->size = 1ULL << val;
	} else {
		params->size = val + 1;
	}
	params->size >>= 3;  

	 
	for (i = 0; i < ARRAY_SIZE(sfdp_bfpt_reads); i++) {
		const struct sfdp_bfpt_read *rd = &sfdp_bfpt_reads[i];
		struct spi_nor_read_command *read;

		if (!(bfpt.dwords[rd->supported_dword] & rd->supported_bit)) {
			params->hwcaps.mask &= ~rd->hwcaps;
			continue;
		}

		params->hwcaps.mask |= rd->hwcaps;
		cmd = spi_nor_hwcaps_read2cmd(rd->hwcaps);
		read = &params->reads[cmd];
		half = bfpt.dwords[rd->settings_dword] >> rd->settings_shift;
		spi_nor_set_read_settings_from_bfpt(read, half, rd->proto);
	}

	 
	erase_mask = 0;
	memset(&params->erase_map, 0, sizeof(params->erase_map));
	for (i = 0; i < ARRAY_SIZE(sfdp_bfpt_erases); i++) {
		const struct sfdp_bfpt_erase *er = &sfdp_bfpt_erases[i];
		u32 erasesize;
		u8 opcode;

		half = bfpt.dwords[er->dword] >> er->shift;
		erasesize = half & 0xff;

		 
		if (!erasesize)
			continue;

		erasesize = 1U << erasesize;
		opcode = (half >> 8) & 0xff;
		erase_mask |= BIT(i);
		spi_nor_set_erase_settings_from_bfpt(&erase_type[i], erasesize,
						     opcode, i);
	}
	spi_nor_init_uniform_erase_map(map, erase_mask, params->size);
	 
	sort(erase_type, SNOR_ERASE_TYPE_MAX, sizeof(erase_type[0]),
	     spi_nor_map_cmp_erase_type, NULL);
	 
	spi_nor_regions_sort_erase_types(map);
	map->uniform_erase_type = map->uniform_region.offset &
				  SNOR_ERASE_TYPE_MASK;

	 
	if (bfpt_header->length == BFPT_DWORD_MAX_JESD216)
		return spi_nor_post_bfpt_fixups(nor, bfpt_header, &bfpt);

	 
	val = bfpt.dwords[SFDP_DWORD(11)];
	val &= BFPT_DWORD11_PAGE_SIZE_MASK;
	val >>= BFPT_DWORD11_PAGE_SIZE_SHIFT;
	params->page_size = 1U << val;

	 
	switch (bfpt.dwords[SFDP_DWORD(15)] & BFPT_DWORD15_QER_MASK) {
	case BFPT_DWORD15_QER_NONE:
		params->quad_enable = NULL;
		break;

	case BFPT_DWORD15_QER_SR2_BIT1_BUGGY:
		 
	case BFPT_DWORD15_QER_SR2_BIT1_NO_RD:
		 
		nor->flags |= SNOR_F_HAS_16BIT_SR | SNOR_F_NO_READ_CR;
		params->quad_enable = spi_nor_sr2_bit1_quad_enable;
		break;

	case BFPT_DWORD15_QER_SR1_BIT6:
		nor->flags &= ~SNOR_F_HAS_16BIT_SR;
		params->quad_enable = spi_nor_sr1_bit6_quad_enable;
		break;

	case BFPT_DWORD15_QER_SR2_BIT7:
		nor->flags &= ~SNOR_F_HAS_16BIT_SR;
		params->quad_enable = spi_nor_sr2_bit7_quad_enable;
		break;

	case BFPT_DWORD15_QER_SR2_BIT1:
		 
		nor->flags |= SNOR_F_HAS_16BIT_SR;

		params->quad_enable = spi_nor_sr2_bit1_quad_enable;
		break;

	default:
		dev_dbg(nor->dev, "BFPT QER reserved value used\n");
		break;
	}

	dword = bfpt.dwords[SFDP_DWORD(16)] & BFPT_DWORD16_4B_ADDR_MODE_MASK;
	if (SFDP_MASK_CHECK(dword, BFPT_DWORD16_4B_ADDR_MODE_BRWR))
		params->set_4byte_addr_mode = spi_nor_set_4byte_addr_mode_brwr;
	else if (SFDP_MASK_CHECK(dword, BFPT_DWORD16_4B_ADDR_MODE_WREN_EN4B_EX4B))
		params->set_4byte_addr_mode = spi_nor_set_4byte_addr_mode_wren_en4b_ex4b;
	else if (SFDP_MASK_CHECK(dword, BFPT_DWORD16_4B_ADDR_MODE_EN4B_EX4B))
		params->set_4byte_addr_mode = spi_nor_set_4byte_addr_mode_en4b_ex4b;
	else
		dev_dbg(nor->dev, "BFPT: 4-Byte Address Mode method is not recognized or not implemented\n");

	 
	if (bfpt.dwords[SFDP_DWORD(16)] & BFPT_DWORD16_SWRST_EN_RST)
		nor->flags |= SNOR_F_SOFT_RESET;

	 
	if (bfpt_header->length == BFPT_DWORD_MAX_JESD216B)
		return spi_nor_post_bfpt_fixups(nor, bfpt_header, &bfpt);

	 
	switch (bfpt.dwords[SFDP_DWORD(18)] & BFPT_DWORD18_CMD_EXT_MASK) {
	case BFPT_DWORD18_CMD_EXT_REP:
		nor->cmd_ext_type = SPI_NOR_EXT_REPEAT;
		break;

	case BFPT_DWORD18_CMD_EXT_INV:
		nor->cmd_ext_type = SPI_NOR_EXT_INVERT;
		break;

	case BFPT_DWORD18_CMD_EXT_RES:
		dev_dbg(nor->dev, "Reserved command extension used\n");
		break;

	case BFPT_DWORD18_CMD_EXT_16B:
		dev_dbg(nor->dev, "16-bit opcodes not supported\n");
		return -EOPNOTSUPP;
	}

	return spi_nor_post_bfpt_fixups(nor, bfpt_header, &bfpt);
}

 
static u8 spi_nor_smpt_addr_nbytes(const struct spi_nor *nor, const u32 settings)
{
	switch (settings & SMPT_CMD_ADDRESS_LEN_MASK) {
	case SMPT_CMD_ADDRESS_LEN_0:
		return 0;
	case SMPT_CMD_ADDRESS_LEN_3:
		return 3;
	case SMPT_CMD_ADDRESS_LEN_4:
		return 4;
	case SMPT_CMD_ADDRESS_LEN_USE_CURRENT:
	default:
		return nor->params->addr_mode_nbytes;
	}
}

 
static u8 spi_nor_smpt_read_dummy(const struct spi_nor *nor, const u32 settings)
{
	u8 read_dummy = SMPT_CMD_READ_DUMMY(settings);

	if (read_dummy == SMPT_CMD_READ_DUMMY_IS_VARIABLE)
		return nor->read_dummy;
	return read_dummy;
}

 
static const u32 *spi_nor_get_map_in_use(struct spi_nor *nor, const u32 *smpt,
					 u8 smpt_len)
{
	const u32 *ret;
	u8 *buf;
	u32 addr;
	int err;
	u8 i;
	u8 addr_nbytes, read_opcode, read_dummy;
	u8 read_data_mask, map_id;

	 
	buf = kmalloc(sizeof(*buf), GFP_KERNEL);
	if (!buf)
		return ERR_PTR(-ENOMEM);

	addr_nbytes = nor->addr_nbytes;
	read_dummy = nor->read_dummy;
	read_opcode = nor->read_opcode;

	map_id = 0;
	 
	for (i = 0; i < smpt_len; i += 2) {
		if (smpt[i] & SMPT_DESC_TYPE_MAP)
			break;

		read_data_mask = SMPT_CMD_READ_DATA(smpt[i]);
		nor->addr_nbytes = spi_nor_smpt_addr_nbytes(nor, smpt[i]);
		nor->read_dummy = spi_nor_smpt_read_dummy(nor, smpt[i]);
		nor->read_opcode = SMPT_CMD_OPCODE(smpt[i]);
		addr = smpt[i + 1];

		err = spi_nor_read_raw(nor, addr, 1, buf);
		if (err) {
			ret = ERR_PTR(err);
			goto out;
		}

		 
		map_id = map_id << 1 | !!(*buf & read_data_mask);
	}

	 
	ret = ERR_PTR(-EINVAL);
	while (i < smpt_len) {
		if (SMPT_MAP_ID(smpt[i]) == map_id) {
			ret = smpt + i;
			break;
		}

		 
		if (smpt[i] & SMPT_DESC_END)
			break;

		 
		i += SMPT_MAP_REGION_COUNT(smpt[i]) + 1;
	}

	 
out:
	kfree(buf);
	nor->addr_nbytes = addr_nbytes;
	nor->read_dummy = read_dummy;
	nor->read_opcode = read_opcode;
	return ret;
}

static void spi_nor_region_mark_end(struct spi_nor_erase_region *region)
{
	region->offset |= SNOR_LAST_REGION;
}

static void spi_nor_region_mark_overlay(struct spi_nor_erase_region *region)
{
	region->offset |= SNOR_OVERLAID_REGION;
}

 
static void
spi_nor_region_check_overlay(struct spi_nor_erase_region *region,
			     const struct spi_nor_erase_type *erase,
			     const u8 erase_type)
{
	int i;

	for (i = 0; i < SNOR_ERASE_TYPE_MAX; i++) {
		if (!(erase[i].size && erase_type & BIT(erase[i].idx)))
			continue;
		if (region->size & erase[i].size_mask) {
			spi_nor_region_mark_overlay(region);
			return;
		}
	}
}

 
static int spi_nor_init_non_uniform_erase_map(struct spi_nor *nor,
					      const u32 *smpt)
{
	struct spi_nor_erase_map *map = &nor->params->erase_map;
	struct spi_nor_erase_type *erase = map->erase_type;
	struct spi_nor_erase_region *region;
	u64 offset;
	u32 region_count;
	int i, j;
	u8 uniform_erase_type, save_uniform_erase_type;
	u8 erase_type, regions_erase_type;

	region_count = SMPT_MAP_REGION_COUNT(*smpt);
	 
	region = devm_kcalloc(nor->dev, region_count, sizeof(*region),
			      GFP_KERNEL);
	if (!region)
		return -ENOMEM;
	map->regions = region;

	uniform_erase_type = 0xff;
	regions_erase_type = 0;
	offset = 0;
	 
	for (i = 0; i < region_count; i++) {
		j = i + 1;  
		region[i].size = SMPT_MAP_REGION_SIZE(smpt[j]);
		erase_type = SMPT_MAP_REGION_ERASE_TYPE(smpt[j]);
		region[i].offset = offset | erase_type;

		spi_nor_region_check_overlay(&region[i], erase, erase_type);

		 
		uniform_erase_type &= erase_type;

		 
		regions_erase_type |= erase_type;

		offset = (region[i].offset & ~SNOR_ERASE_FLAGS_MASK) +
			 region[i].size;
	}
	spi_nor_region_mark_end(&region[i - 1]);

	save_uniform_erase_type = map->uniform_erase_type;
	map->uniform_erase_type = spi_nor_sort_erase_mask(map,
							  uniform_erase_type);

	if (!regions_erase_type) {
		 
		map->uniform_erase_type = save_uniform_erase_type;
		return -EINVAL;
	}

	 
	for (i = 0; i < SNOR_ERASE_TYPE_MAX; i++)
		if (!(regions_erase_type & BIT(erase[i].idx)))
			spi_nor_mask_erase_type(&erase[i]);

	return 0;
}

 
static int spi_nor_parse_smpt(struct spi_nor *nor,
			      const struct sfdp_parameter_header *smpt_header)
{
	const u32 *sector_map;
	u32 *smpt;
	size_t len;
	u32 addr;
	int ret;

	 
	len = smpt_header->length * sizeof(*smpt);
	smpt = kmalloc(len, GFP_KERNEL);
	if (!smpt)
		return -ENOMEM;

	addr = SFDP_PARAM_HEADER_PTP(smpt_header);
	ret = spi_nor_read_sfdp(nor, addr, len, smpt);
	if (ret)
		goto out;

	 
	le32_to_cpu_array(smpt, smpt_header->length);

	sector_map = spi_nor_get_map_in_use(nor, smpt, smpt_header->length);
	if (IS_ERR(sector_map)) {
		ret = PTR_ERR(sector_map);
		goto out;
	}

	ret = spi_nor_init_non_uniform_erase_map(nor, sector_map);
	if (ret)
		goto out;

	spi_nor_regions_sort_erase_types(&nor->params->erase_map);
	 
out:
	kfree(smpt);
	return ret;
}

 
static int spi_nor_parse_4bait(struct spi_nor *nor,
			       const struct sfdp_parameter_header *param_header)
{
	static const struct sfdp_4bait reads[] = {
		{ SNOR_HWCAPS_READ,		BIT(0) },
		{ SNOR_HWCAPS_READ_FAST,	BIT(1) },
		{ SNOR_HWCAPS_READ_1_1_2,	BIT(2) },
		{ SNOR_HWCAPS_READ_1_2_2,	BIT(3) },
		{ SNOR_HWCAPS_READ_1_1_4,	BIT(4) },
		{ SNOR_HWCAPS_READ_1_4_4,	BIT(5) },
		{ SNOR_HWCAPS_READ_1_1_1_DTR,	BIT(13) },
		{ SNOR_HWCAPS_READ_1_2_2_DTR,	BIT(14) },
		{ SNOR_HWCAPS_READ_1_4_4_DTR,	BIT(15) },
	};
	static const struct sfdp_4bait programs[] = {
		{ SNOR_HWCAPS_PP,		BIT(6) },
		{ SNOR_HWCAPS_PP_1_1_4,		BIT(7) },
		{ SNOR_HWCAPS_PP_1_4_4,		BIT(8) },
	};
	static const struct sfdp_4bait erases[SNOR_ERASE_TYPE_MAX] = {
		{ 0u  ,		BIT(9) },
		{ 0u  ,		BIT(10) },
		{ 0u  ,		BIT(11) },
		{ 0u  ,		BIT(12) },
	};
	struct spi_nor_flash_parameter *params = nor->params;
	struct spi_nor_pp_command *params_pp = params->page_programs;
	struct spi_nor_erase_map *map = &params->erase_map;
	struct spi_nor_erase_type *erase_type = map->erase_type;
	u32 *dwords;
	size_t len;
	u32 addr, discard_hwcaps, read_hwcaps, pp_hwcaps, erase_mask;
	int i, ret;

	if (param_header->major != SFDP_JESD216_MAJOR ||
	    param_header->length < SFDP_4BAIT_DWORD_MAX)
		return -EINVAL;

	 
	len = sizeof(*dwords) * SFDP_4BAIT_DWORD_MAX;

	 
	dwords = kmalloc(len, GFP_KERNEL);
	if (!dwords)
		return -ENOMEM;

	addr = SFDP_PARAM_HEADER_PTP(param_header);
	ret = spi_nor_read_sfdp(nor, addr, len, dwords);
	if (ret)
		goto out;

	 
	le32_to_cpu_array(dwords, SFDP_4BAIT_DWORD_MAX);

	 
	discard_hwcaps = 0;
	read_hwcaps = 0;
	for (i = 0; i < ARRAY_SIZE(reads); i++) {
		const struct sfdp_4bait *read = &reads[i];

		discard_hwcaps |= read->hwcaps;
		if ((params->hwcaps.mask & read->hwcaps) &&
		    (dwords[SFDP_DWORD(1)] & read->supported_bit))
			read_hwcaps |= read->hwcaps;
	}

	 
	pp_hwcaps = 0;
	for (i = 0; i < ARRAY_SIZE(programs); i++) {
		const struct sfdp_4bait *program = &programs[i];

		 
		discard_hwcaps |= program->hwcaps;
		if (dwords[SFDP_DWORD(1)] & program->supported_bit)
			pp_hwcaps |= program->hwcaps;
	}

	 
	erase_mask = 0;
	for (i = 0; i < SNOR_ERASE_TYPE_MAX; i++) {
		const struct sfdp_4bait *erase = &erases[i];

		if (dwords[SFDP_DWORD(1)] & erase->supported_bit)
			erase_mask |= BIT(i);
	}

	 
	erase_mask = spi_nor_sort_erase_mask(map, erase_mask);

	 
	if (!read_hwcaps || !pp_hwcaps || !erase_mask)
		goto out;

	 
	params->hwcaps.mask &= ~discard_hwcaps;
	params->hwcaps.mask |= (read_hwcaps | pp_hwcaps);

	 
	for (i = 0; i < SNOR_CMD_READ_MAX; i++) {
		struct spi_nor_read_command *read_cmd = &params->reads[i];

		read_cmd->opcode = spi_nor_convert_3to4_read(read_cmd->opcode);
	}

	 
	if (pp_hwcaps & SNOR_HWCAPS_PP) {
		spi_nor_set_pp_settings(&params_pp[SNOR_CMD_PP],
					SPINOR_OP_PP_4B, SNOR_PROTO_1_1_1);
		 
		spi_nor_set_pp_settings(&params_pp[SNOR_CMD_PP_8_8_8_DTR],
					SPINOR_OP_PP_4B, SNOR_PROTO_8_8_8_DTR);
	}
	if (pp_hwcaps & SNOR_HWCAPS_PP_1_1_4)
		spi_nor_set_pp_settings(&params_pp[SNOR_CMD_PP_1_1_4],
					SPINOR_OP_PP_1_1_4_4B,
					SNOR_PROTO_1_1_4);
	if (pp_hwcaps & SNOR_HWCAPS_PP_1_4_4)
		spi_nor_set_pp_settings(&params_pp[SNOR_CMD_PP_1_4_4],
					SPINOR_OP_PP_1_4_4_4B,
					SNOR_PROTO_1_4_4);

	for (i = 0; i < SNOR_ERASE_TYPE_MAX; i++) {
		if (erase_mask & BIT(i))
			erase_type[i].opcode = (dwords[SFDP_DWORD(2)] >>
						erase_type[i].idx * 8) & 0xFF;
		else
			spi_nor_mask_erase_type(&erase_type[i]);
	}

	 
	params->addr_nbytes = 4;
	nor->flags |= SNOR_F_4B_OPCODES | SNOR_F_HAS_4BAIT;

	 
out:
	kfree(dwords);
	return ret;
}

#define PROFILE1_DWORD1_RDSR_ADDR_BYTES		BIT(29)
#define PROFILE1_DWORD1_RDSR_DUMMY		BIT(28)
#define PROFILE1_DWORD1_RD_FAST_CMD		GENMASK(15, 8)
#define PROFILE1_DWORD4_DUMMY_200MHZ		GENMASK(11, 7)
#define PROFILE1_DWORD5_DUMMY_166MHZ		GENMASK(31, 27)
#define PROFILE1_DWORD5_DUMMY_133MHZ		GENMASK(21, 17)
#define PROFILE1_DWORD5_DUMMY_100MHZ		GENMASK(11, 7)

 
static int spi_nor_parse_profile1(struct spi_nor *nor,
				  const struct sfdp_parameter_header *profile1_header)
{
	u32 *dwords, addr;
	size_t len;
	int ret;
	u8 dummy, opcode;

	len = profile1_header->length * sizeof(*dwords);
	dwords = kmalloc(len, GFP_KERNEL);
	if (!dwords)
		return -ENOMEM;

	addr = SFDP_PARAM_HEADER_PTP(profile1_header);
	ret = spi_nor_read_sfdp(nor, addr, len, dwords);
	if (ret)
		goto out;

	le32_to_cpu_array(dwords, profile1_header->length);

	 
	opcode = FIELD_GET(PROFILE1_DWORD1_RD_FAST_CMD, dwords[SFDP_DWORD(1)]);

	  
	if (dwords[SFDP_DWORD(1)] & PROFILE1_DWORD1_RDSR_DUMMY)
		nor->params->rdsr_dummy = 8;
	else
		nor->params->rdsr_dummy = 4;

	if (dwords[SFDP_DWORD(1)] & PROFILE1_DWORD1_RDSR_ADDR_BYTES)
		nor->params->rdsr_addr_nbytes = 4;
	else
		nor->params->rdsr_addr_nbytes = 0;

	 
	dummy = FIELD_GET(PROFILE1_DWORD4_DUMMY_200MHZ, dwords[SFDP_DWORD(4)]);
	if (!dummy)
		dummy = FIELD_GET(PROFILE1_DWORD5_DUMMY_166MHZ,
				  dwords[SFDP_DWORD(5)]);
	if (!dummy)
		dummy = FIELD_GET(PROFILE1_DWORD5_DUMMY_133MHZ,
				  dwords[SFDP_DWORD(5)]);
	if (!dummy)
		dummy = FIELD_GET(PROFILE1_DWORD5_DUMMY_100MHZ,
				  dwords[SFDP_DWORD(5)]);
	if (!dummy)
		dev_dbg(nor->dev,
			"Can't find dummy cycles from Profile 1.0 table\n");

	 
	dummy = round_up(dummy, 2);

	 
	nor->params->hwcaps.mask |= SNOR_HWCAPS_READ_8_8_8_DTR;
	spi_nor_set_read_settings(&nor->params->reads[SNOR_CMD_READ_8_8_8_DTR],
				  0, dummy, opcode,
				  SNOR_PROTO_8_8_8_DTR);

	 
	nor->params->hwcaps.mask |= SNOR_HWCAPS_PP_8_8_8_DTR;

out:
	kfree(dwords);
	return ret;
}

#define SCCR_DWORD22_OCTAL_DTR_EN_VOLATILE		BIT(31)

 
static int spi_nor_parse_sccr(struct spi_nor *nor,
			      const struct sfdp_parameter_header *sccr_header)
{
	struct spi_nor_flash_parameter *params = nor->params;
	u32 *dwords, addr;
	size_t len;
	int ret;

	len = sccr_header->length * sizeof(*dwords);
	dwords = kmalloc(len, GFP_KERNEL);
	if (!dwords)
		return -ENOMEM;

	addr = SFDP_PARAM_HEADER_PTP(sccr_header);
	ret = spi_nor_read_sfdp(nor, addr, len, dwords);
	if (ret)
		goto out;

	le32_to_cpu_array(dwords, sccr_header->length);

	 
	if (!params->vreg_offset) {
		params->vreg_offset = devm_kmalloc(nor->dev, sizeof(*dwords),
						   GFP_KERNEL);
		if (!params->vreg_offset) {
			ret = -ENOMEM;
			goto out;
		}
	}
	params->vreg_offset[0] = dwords[SFDP_DWORD(1)];
	params->n_dice = 1;

	if (FIELD_GET(SCCR_DWORD22_OCTAL_DTR_EN_VOLATILE,
		      dwords[SFDP_DWORD(22)]))
		nor->flags |= SNOR_F_IO_MODE_EN_VOLATILE;

out:
	kfree(dwords);
	return ret;
}

 
static int spi_nor_parse_sccr_mc(struct spi_nor *nor,
				 const struct sfdp_parameter_header *sccr_mc_header)
{
	struct spi_nor_flash_parameter *params = nor->params;
	u32 *dwords, addr;
	u8 i, n_dice;
	size_t len;
	int ret;

	len = sccr_mc_header->length * sizeof(*dwords);
	dwords = kmalloc(len, GFP_KERNEL);
	if (!dwords)
		return -ENOMEM;

	addr = SFDP_PARAM_HEADER_PTP(sccr_mc_header);
	ret = spi_nor_read_sfdp(nor, addr, len, dwords);
	if (ret)
		goto out;

	le32_to_cpu_array(dwords, sccr_mc_header->length);

	 
	n_dice = 1 + sccr_mc_header->length / 2;

	 
	params->vreg_offset =
			devm_krealloc(nor->dev, params->vreg_offset,
				      n_dice * sizeof(*dwords),
				      GFP_KERNEL);
	if (!params->vreg_offset) {
		ret = -ENOMEM;
		goto out;
	}

	for (i = 1; i < n_dice; i++)
		params->vreg_offset[i] = dwords[SFDP_DWORD(i) * 2];

	params->n_dice = n_dice;

out:
	kfree(dwords);
	return ret;
}

 
static int spi_nor_post_sfdp_fixups(struct spi_nor *nor)
{
	int ret;

	if (nor->manufacturer && nor->manufacturer->fixups &&
	    nor->manufacturer->fixups->post_sfdp) {
		ret = nor->manufacturer->fixups->post_sfdp(nor);
		if (ret)
			return ret;
	}

	if (nor->info->fixups && nor->info->fixups->post_sfdp)
		return nor->info->fixups->post_sfdp(nor);

	return 0;
}

 
int spi_nor_check_sfdp_signature(struct spi_nor *nor)
{
	u32 signature;
	int err;

	 
	err = spi_nor_read_sfdp_dma_unsafe(nor, 0, sizeof(signature),
					   &signature);
	if (err < 0)
		return err;

	 
	if (le32_to_cpu(signature) != SFDP_SIGNATURE)
		return -EINVAL;

	return 0;
}

 
int spi_nor_parse_sfdp(struct spi_nor *nor)
{
	const struct sfdp_parameter_header *param_header, *bfpt_header;
	struct sfdp_parameter_header *param_headers = NULL;
	struct sfdp_header header;
	struct device *dev = nor->dev;
	struct sfdp *sfdp;
	size_t sfdp_size;
	size_t psize;
	int i, err;

	 
	err = spi_nor_read_sfdp_dma_unsafe(nor, 0, sizeof(header), &header);
	if (err < 0)
		return err;

	 
	if (le32_to_cpu(header.signature) != SFDP_SIGNATURE ||
	    header.major != SFDP_JESD216_MAJOR)
		return -EINVAL;

	 
	bfpt_header = &header.bfpt_header;
	if (SFDP_PARAM_HEADER_ID(bfpt_header) != SFDP_BFPT_ID ||
	    bfpt_header->major != SFDP_JESD216_MAJOR)
		return -EINVAL;

	sfdp_size = SFDP_PARAM_HEADER_PTP(bfpt_header) +
		    SFDP_PARAM_HEADER_PARAM_LEN(bfpt_header);

	 
	if (header.nph) {
		psize = header.nph * sizeof(*param_headers);

		param_headers = kmalloc(psize, GFP_KERNEL);
		if (!param_headers)
			return -ENOMEM;

		err = spi_nor_read_sfdp(nor, sizeof(header),
					psize, param_headers);
		if (err < 0) {
			dev_dbg(dev, "failed to read SFDP parameter headers\n");
			goto exit;
		}
	}

	 
	for (i = 0; i < header.nph; i++) {
		param_header = &param_headers[i];
		sfdp_size = max_t(size_t, sfdp_size,
				  SFDP_PARAM_HEADER_PTP(param_header) +
				  SFDP_PARAM_HEADER_PARAM_LEN(param_header));
	}

	 
	if (sfdp_size > PAGE_SIZE) {
		dev_dbg(dev, "SFDP data (%zu) too big, truncating\n",
			sfdp_size);
		sfdp_size = PAGE_SIZE;
	}

	sfdp = devm_kzalloc(dev, sizeof(*sfdp), GFP_KERNEL);
	if (!sfdp) {
		err = -ENOMEM;
		goto exit;
	}

	 
	sfdp->num_dwords = DIV_ROUND_UP(sfdp_size, sizeof(*sfdp->dwords));
	sfdp->dwords = devm_kcalloc(dev, sfdp->num_dwords,
				    sizeof(*sfdp->dwords), GFP_KERNEL);
	if (!sfdp->dwords) {
		err = -ENOMEM;
		devm_kfree(dev, sfdp);
		goto exit;
	}

	err = spi_nor_read_sfdp(nor, 0, sfdp_size, sfdp->dwords);
	if (err < 0) {
		dev_dbg(dev, "failed to read SFDP data\n");
		devm_kfree(dev, sfdp->dwords);
		devm_kfree(dev, sfdp);
		goto exit;
	}

	nor->sfdp = sfdp;

	 
	for (i = 0; i < header.nph; i++) {
		param_header = &param_headers[i];

		if (SFDP_PARAM_HEADER_ID(param_header) == SFDP_BFPT_ID &&
		    param_header->major == SFDP_JESD216_MAJOR &&
		    (param_header->minor > bfpt_header->minor ||
		     (param_header->minor == bfpt_header->minor &&
		      param_header->length > bfpt_header->length)))
			bfpt_header = param_header;
	}

	err = spi_nor_parse_bfpt(nor, bfpt_header);
	if (err)
		goto exit;

	 
	for (i = 0; i < header.nph; i++) {
		param_header = &param_headers[i];

		switch (SFDP_PARAM_HEADER_ID(param_header)) {
		case SFDP_SECTOR_MAP_ID:
			err = spi_nor_parse_smpt(nor, param_header);
			break;

		case SFDP_4BAIT_ID:
			err = spi_nor_parse_4bait(nor, param_header);
			break;

		case SFDP_PROFILE1_ID:
			err = spi_nor_parse_profile1(nor, param_header);
			break;

		case SFDP_SCCR_MAP_ID:
			err = spi_nor_parse_sccr(nor, param_header);
			break;

		case SFDP_SCCR_MAP_MC_ID:
			err = spi_nor_parse_sccr_mc(nor, param_header);
			break;

		default:
			break;
		}

		if (err) {
			dev_warn(dev, "Failed to parse optional parameter table: %04x\n",
				 SFDP_PARAM_HEADER_ID(param_header));
			 
			err = 0;
		}
	}

	err = spi_nor_post_sfdp_fixups(nor);
exit:
	kfree(param_headers);
	return err;
}
