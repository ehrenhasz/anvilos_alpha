 
#include "umc_v8_10.h"
#include "amdgpu_ras.h"
#include "amdgpu_umc.h"
#include "amdgpu.h"
#include "umc/umc_8_10_0_offset.h"
#include "umc/umc_8_10_0_sh_mask.h"

#define UMC_8_NODE_DIST   0x800000
#define UMC_8_INST_DIST   0x4000

struct channelnum_map_colbit {
	uint32_t channel_num;
	uint32_t col_bit;
};

const struct channelnum_map_colbit umc_v8_10_channelnum_map_colbit_table[] = {
	{24, 13},
	{20, 13},
	{16, 12},
	{14, 12},
	{12, 12},
	{10, 12},
	{6,  11},
};

const uint32_t
	umc_v8_10_channel_idx_tbl_ext0[]
				[UMC_V8_10_UMC_INSTANCE_NUM]
				[UMC_V8_10_CHANNEL_INSTANCE_NUM] = {
	   {{1,   5}, {7,  3}},
	   {{14, 15}, {13, 12}},
	   {{10, 11}, {9,  8}},
	   {{6,   2}, {0,  4}}
	};

const uint32_t
	umc_v8_10_channel_idx_tbl[]
				[UMC_V8_10_UMC_INSTANCE_NUM]
				[UMC_V8_10_CHANNEL_INSTANCE_NUM] = {
	   {{16, 18}, {17, 19}},
	   {{15, 11}, {3,   7}},
	   {{1,   5}, {13,  9}},
	   {{23, 21}, {22, 20}},
	   {{0,   4}, {12,  8}},
	   {{14, 10}, {2,   6}}
	};

static inline uint32_t get_umc_v8_10_reg_offset(struct amdgpu_device *adev,
					    uint32_t node_inst,
					    uint32_t umc_inst,
					    uint32_t ch_inst)
{
	return adev->umc.channel_offs * ch_inst + UMC_8_INST_DIST * umc_inst +
		UMC_8_NODE_DIST * node_inst;
}

static int umc_v8_10_clear_error_count_per_channel(struct amdgpu_device *adev,
					uint32_t node_inst, uint32_t umc_inst,
					uint32_t ch_inst, void *data)
{
	uint32_t ecc_err_cnt_addr;
	uint32_t umc_reg_offset =
		get_umc_v8_10_reg_offset(adev, node_inst, umc_inst, ch_inst);

	ecc_err_cnt_addr =
		SOC15_REG_OFFSET(UMC, 0, regUMCCH0_0_GeccErrCnt);

	 
	WREG32_PCIE((ecc_err_cnt_addr + umc_reg_offset) * 4,
			UMC_V8_10_CE_CNT_INIT);

	return 0;
}

static void umc_v8_10_clear_error_count(struct amdgpu_device *adev)
{
	amdgpu_umc_loop_channels(adev,
		umc_v8_10_clear_error_count_per_channel, NULL);
}

static void umc_v8_10_query_correctable_error_count(struct amdgpu_device *adev,
						   uint32_t umc_reg_offset,
						   unsigned long *error_count)
{
	uint64_t mc_umc_status;
	uint32_t mc_umc_status_addr;

	 
	mc_umc_status_addr =
		SOC15_REG_OFFSET(UMC, 0, regMCA_UMC_UMC0_MCUMC_STATUST0);

	 
	mc_umc_status = RREG64_PCIE((mc_umc_status_addr + umc_reg_offset) * 4);
	if (REG_GET_FIELD(mc_umc_status, MCA_UMC_UMC0_MCUMC_STATUST0, Val) == 1 &&
	    REG_GET_FIELD(mc_umc_status, MCA_UMC_UMC0_MCUMC_STATUST0, CECC) == 1)
		*error_count += 1;
}

static void umc_v8_10_query_uncorrectable_error_count(struct amdgpu_device *adev,
						      uint32_t umc_reg_offset,
						      unsigned long *error_count)
{
	uint64_t mc_umc_status;
	uint32_t mc_umc_status_addr;

	mc_umc_status_addr = SOC15_REG_OFFSET(UMC, 0, regMCA_UMC_UMC0_MCUMC_STATUST0);

	 
	mc_umc_status = RREG64_PCIE((mc_umc_status_addr + umc_reg_offset) * 4);
	if ((REG_GET_FIELD(mc_umc_status, MCA_UMC_UMC0_MCUMC_STATUST0, Val) == 1) &&
	    (REG_GET_FIELD(mc_umc_status, MCA_UMC_UMC0_MCUMC_STATUST0, Deferred) == 1 ||
	    REG_GET_FIELD(mc_umc_status, MCA_UMC_UMC0_MCUMC_STATUST0, UECC) == 1 ||
	    REG_GET_FIELD(mc_umc_status, MCA_UMC_UMC0_MCUMC_STATUST0, PCC) == 1 ||
	    REG_GET_FIELD(mc_umc_status, MCA_UMC_UMC0_MCUMC_STATUST0, UC) == 1 ||
	    REG_GET_FIELD(mc_umc_status, MCA_UMC_UMC0_MCUMC_STATUST0, TCC) == 1))
		*error_count += 1;
}

static int umc_v8_10_query_ecc_error_count(struct amdgpu_device *adev,
					uint32_t node_inst, uint32_t umc_inst,
					uint32_t ch_inst, void *data)
{
	struct ras_err_data *err_data = (struct ras_err_data *)data;
	uint32_t umc_reg_offset =
		get_umc_v8_10_reg_offset(adev, node_inst, umc_inst, ch_inst);

	umc_v8_10_query_correctable_error_count(adev,
					umc_reg_offset,
					&(err_data->ce_count));
	umc_v8_10_query_uncorrectable_error_count(adev,
					umc_reg_offset,
					&(err_data->ue_count));

	return 0;
}

static void umc_v8_10_query_ras_error_count(struct amdgpu_device *adev,
					   void *ras_error_status)
{
	amdgpu_umc_loop_channels(adev,
		umc_v8_10_query_ecc_error_count, ras_error_status);

	umc_v8_10_clear_error_count(adev);
}

static uint32_t umc_v8_10_get_col_bit(uint32_t channel_num)
{
	uint32_t t = 0;

	for (t = 0; t < ARRAY_SIZE(umc_v8_10_channelnum_map_colbit_table); t++)
		if (channel_num == umc_v8_10_channelnum_map_colbit_table[t].channel_num)
			return umc_v8_10_channelnum_map_colbit_table[t].col_bit;

	 
	return U32_MAX;
}

 
static int umc_v8_10_swizzle_mode_na_to_pa(struct amdgpu_device *adev,
					uint32_t channel_idx,
					uint64_t na, uint64_t *soc_pa)
{
	uint32_t channel_num = UMC_V8_10_TOTAL_CHANNEL_NUM(adev);
	uint32_t col_bit = umc_v8_10_get_col_bit(channel_num);
	uint64_t tmp_addr;

	if (col_bit == U32_MAX)
		return -1;

	tmp_addr = SWIZZLE_MODE_TMP_ADDR(na, channel_num, channel_idx);
	*soc_pa = SWIZZLE_MODE_ADDR_HI(tmp_addr, col_bit) |
		SWIZZLE_MODE_ADDR_MID(na, col_bit) |
		SWIZZLE_MODE_ADDR_LOW(tmp_addr, col_bit) |
		SWIZZLE_MODE_ADDR_LSB(na);

	return 0;
}

static void umc_v8_10_convert_error_address(struct amdgpu_device *adev,
					    struct ras_err_data *err_data, uint64_t err_addr,
					    uint32_t ch_inst, uint32_t umc_inst,
					    uint32_t node_inst, uint64_t mc_umc_status)
{
	uint64_t na_err_addr_base;
	uint64_t na_err_addr, retired_page_addr;
	uint32_t channel_index, addr_lsb, col = 0;
	int ret = 0;

	channel_index =
		adev->umc.channel_idx_tbl[node_inst * adev->umc.umc_inst_num *
					adev->umc.channel_inst_num +
					umc_inst * adev->umc.channel_inst_num +
					ch_inst];

	 
	addr_lsb = REG_GET_FIELD(mc_umc_status, MCA_UMC_UMC0_MCUMC_STATUST0, AddrLsb);
	err_addr &= ~((0x1ULL << addr_lsb) - 1);
	na_err_addr_base = err_addr & ~(0x3ULL << UMC_V8_10_NA_C5_BIT);

	 
	for (col = 0; col < UMC_V8_10_NA_COL_2BITS_POWER_OF_2_NUM; col++) {
		na_err_addr = na_err_addr_base | (col << UMC_V8_10_NA_C5_BIT);

		 
		ret = umc_v8_10_swizzle_mode_na_to_pa(adev, channel_index,
						na_err_addr, &retired_page_addr);
		if (ret) {
			dev_err(adev->dev, "Failed to map pa from umc na.\n");
			break;
		}
		dev_info(adev->dev, "Error Address(PA): 0x%llx\n",
			retired_page_addr);
		amdgpu_umc_fill_error_record(err_data, na_err_addr,
				retired_page_addr, channel_index, umc_inst);
	}
}

static int umc_v8_10_query_error_address(struct amdgpu_device *adev,
					uint32_t node_inst, uint32_t umc_inst,
					uint32_t ch_inst, void *data)
{
	uint64_t mc_umc_status_addr;
	uint64_t mc_umc_status, err_addr;
	uint64_t mc_umc_addrt0;
	struct ras_err_data *err_data = (struct ras_err_data *)data;
	uint32_t umc_reg_offset =
		get_umc_v8_10_reg_offset(adev, node_inst, umc_inst, ch_inst);

	mc_umc_status_addr =
		SOC15_REG_OFFSET(UMC, 0, regMCA_UMC_UMC0_MCUMC_STATUST0);
	mc_umc_status = RREG64_PCIE((mc_umc_status_addr + umc_reg_offset) * 4);

	if (mc_umc_status == 0)
		return 0;

	if (!err_data->err_addr) {
		 
		WREG64_PCIE((mc_umc_status_addr + umc_reg_offset) * 4, 0x0ULL);
		return 0;
	}

	 
	if (REG_GET_FIELD(mc_umc_status, MCA_UMC_UMC0_MCUMC_STATUST0, Val) == 1 &&
	    REG_GET_FIELD(mc_umc_status, MCA_UMC_UMC0_MCUMC_STATUST0, AddrV) == 1 &&
	    REG_GET_FIELD(mc_umc_status, MCA_UMC_UMC0_MCUMC_STATUST0, UECC) == 1) {

		mc_umc_addrt0 = SOC15_REG_OFFSET(UMC, 0, regMCA_UMC_UMC0_MCUMC_ADDRT0);
		err_addr = RREG64_PCIE((mc_umc_addrt0 + umc_reg_offset) * 4);
		err_addr = REG_GET_FIELD(err_addr, MCA_UMC_UMC0_MCUMC_ADDRT0, ErrorAddr);

		umc_v8_10_convert_error_address(adev, err_data, err_addr,
					ch_inst, umc_inst, node_inst, mc_umc_status);
	}

	 
	WREG64_PCIE((mc_umc_status_addr + umc_reg_offset) * 4, 0x0ULL);

	return 0;
}

static void umc_v8_10_query_ras_error_address(struct amdgpu_device *adev,
					     void *ras_error_status)
{
	amdgpu_umc_loop_channels(adev,
		umc_v8_10_query_error_address, ras_error_status);
}

static int umc_v8_10_err_cnt_init_per_channel(struct amdgpu_device *adev,
					uint32_t node_inst, uint32_t umc_inst,
					uint32_t ch_inst, void *data)
{
	uint32_t ecc_err_cnt_sel, ecc_err_cnt_sel_addr;
	uint32_t ecc_err_cnt_addr;
	uint32_t umc_reg_offset =
		get_umc_v8_10_reg_offset(adev, node_inst, umc_inst, ch_inst);

	ecc_err_cnt_sel_addr =
		SOC15_REG_OFFSET(UMC, 0, regUMCCH0_0_GeccErrCntSel);
	ecc_err_cnt_addr =
		SOC15_REG_OFFSET(UMC, 0, regUMCCH0_0_GeccErrCnt);

	ecc_err_cnt_sel = RREG32_PCIE((ecc_err_cnt_sel_addr + umc_reg_offset) * 4);

	 
	ecc_err_cnt_sel = REG_SET_FIELD(ecc_err_cnt_sel, UMCCH0_0_GeccErrCntSel,
					GeccErrInt, 0x1);
	WREG32_PCIE((ecc_err_cnt_sel_addr + umc_reg_offset) * 4, ecc_err_cnt_sel);
	 
	WREG32_PCIE((ecc_err_cnt_addr + umc_reg_offset) * 4, UMC_V8_10_CE_CNT_INIT);

	return 0;
}

static void umc_v8_10_err_cnt_init(struct amdgpu_device *adev)
{
	amdgpu_umc_loop_channels(adev,
		umc_v8_10_err_cnt_init_per_channel, NULL);
}

static bool umc_v8_10_query_ras_poison_mode(struct amdgpu_device *adev)
{
	 
	return true;
}

static void umc_v8_10_ecc_info_query_correctable_error_count(struct amdgpu_device *adev,
				      uint32_t node_inst, uint32_t umc_inst, uint32_t ch_inst,
				      unsigned long *error_count)
{
	uint64_t mc_umc_status;
	uint32_t eccinfo_table_idx;
	struct amdgpu_ras *ras = amdgpu_ras_get_context(adev);

	eccinfo_table_idx = node_inst * adev->umc.umc_inst_num *
				  adev->umc.channel_inst_num +
				  umc_inst * adev->umc.channel_inst_num +
				  ch_inst;

	 
	mc_umc_status = ras->umc_ecc.ecc[eccinfo_table_idx].mca_umc_status;
	if (REG_GET_FIELD(mc_umc_status, MCA_UMC_UMC0_MCUMC_STATUST0, Val) == 1 &&
	    REG_GET_FIELD(mc_umc_status, MCA_UMC_UMC0_MCUMC_STATUST0, CECC) == 1) {
		*error_count += 1;
	}
}

static void umc_v8_10_ecc_info_query_uncorrectable_error_count(struct amdgpu_device *adev,
				      uint32_t node_inst, uint32_t umc_inst, uint32_t ch_inst,
				      unsigned long *error_count)
{
	uint64_t mc_umc_status;
	uint32_t eccinfo_table_idx;
	struct amdgpu_ras *ras = amdgpu_ras_get_context(adev);

	eccinfo_table_idx = node_inst * adev->umc.umc_inst_num *
				  adev->umc.channel_inst_num +
				  umc_inst * adev->umc.channel_inst_num +
				  ch_inst;

	 
	mc_umc_status = ras->umc_ecc.ecc[eccinfo_table_idx].mca_umc_status;
	if ((REG_GET_FIELD(mc_umc_status, MCA_UMC_UMC0_MCUMC_STATUST0, Val) == 1) &&
	    (REG_GET_FIELD(mc_umc_status, MCA_UMC_UMC0_MCUMC_STATUST0, Deferred) == 1 ||
	    REG_GET_FIELD(mc_umc_status, MCA_UMC_UMC0_MCUMC_STATUST0, UECC) == 1 ||
	    REG_GET_FIELD(mc_umc_status, MCA_UMC_UMC0_MCUMC_STATUST0, PCC) == 1 ||
	    REG_GET_FIELD(mc_umc_status, MCA_UMC_UMC0_MCUMC_STATUST0, UC) == 1 ||
	    REG_GET_FIELD(mc_umc_status, MCA_UMC_UMC0_MCUMC_STATUST0, TCC) == 1)) {
		*error_count += 1;
	}
}

static int umc_v8_10_ecc_info_query_ecc_error_count(struct amdgpu_device *adev,
					uint32_t node_inst, uint32_t umc_inst,
					uint32_t ch_inst, void *data)
{
	struct ras_err_data *err_data = (struct ras_err_data *)data;

	umc_v8_10_ecc_info_query_correctable_error_count(adev,
					node_inst, umc_inst, ch_inst,
					&(err_data->ce_count));
	umc_v8_10_ecc_info_query_uncorrectable_error_count(adev,
					node_inst, umc_inst, ch_inst,
					&(err_data->ue_count));
	return 0;
}

static void umc_v8_10_ecc_info_query_ras_error_count(struct amdgpu_device *adev,
					void *ras_error_status)
{
	amdgpu_umc_loop_channels(adev,
		umc_v8_10_ecc_info_query_ecc_error_count, ras_error_status);
}

static int umc_v8_10_ecc_info_query_error_address(struct amdgpu_device *adev,
					uint32_t node_inst, uint32_t umc_inst,
					uint32_t ch_inst, void *data)
{
	uint32_t eccinfo_table_idx;
	uint64_t mc_umc_status, err_addr;
	struct ras_err_data *err_data = (struct ras_err_data *)data;
	struct amdgpu_ras *ras = amdgpu_ras_get_context(adev);

	eccinfo_table_idx = node_inst * adev->umc.umc_inst_num *
				  adev->umc.channel_inst_num +
				  umc_inst * adev->umc.channel_inst_num +
				  ch_inst;

	mc_umc_status = ras->umc_ecc.ecc[eccinfo_table_idx].mca_umc_status;

	if (mc_umc_status == 0)
		return 0;

	if (!err_data->err_addr)
		return 0;

	 
	if (REG_GET_FIELD(mc_umc_status, MCA_UMC_UMC0_MCUMC_STATUST0, Val) == 1 &&
	    REG_GET_FIELD(mc_umc_status, MCA_UMC_UMC0_MCUMC_STATUST0, AddrV) == 1 &&
	    (REG_GET_FIELD(mc_umc_status, MCA_UMC_UMC0_MCUMC_STATUST0, UECC) == 1)) {

		err_addr = ras->umc_ecc.ecc[eccinfo_table_idx].mca_umc_addr;
		err_addr = REG_GET_FIELD(err_addr, MCA_UMC_UMC0_MCUMC_ADDRT0, ErrorAddr);

		umc_v8_10_convert_error_address(adev, err_data, err_addr,
					ch_inst, umc_inst, node_inst, mc_umc_status);
	}

	return 0;
}

static void umc_v8_10_ecc_info_query_ras_error_address(struct amdgpu_device *adev,
					void *ras_error_status)
{
	amdgpu_umc_loop_channels(adev,
		umc_v8_10_ecc_info_query_error_address, ras_error_status);
}

static void umc_v8_10_set_eeprom_table_version(struct amdgpu_ras_eeprom_table_header *hdr)
{
	hdr->version = RAS_TABLE_VER_V2_1;
}

const struct amdgpu_ras_block_hw_ops umc_v8_10_ras_hw_ops = {
	.query_ras_error_count = umc_v8_10_query_ras_error_count,
	.query_ras_error_address = umc_v8_10_query_ras_error_address,
};

struct amdgpu_umc_ras umc_v8_10_ras = {
	.ras_block = {
		.hw_ops = &umc_v8_10_ras_hw_ops,
	},
	.err_cnt_init = umc_v8_10_err_cnt_init,
	.query_ras_poison_mode = umc_v8_10_query_ras_poison_mode,
	.ecc_info_query_ras_error_count = umc_v8_10_ecc_info_query_ras_error_count,
	.ecc_info_query_ras_error_address = umc_v8_10_ecc_info_query_ras_error_address,
	.set_eeprom_table_version = umc_v8_10_set_eeprom_table_version,
};
