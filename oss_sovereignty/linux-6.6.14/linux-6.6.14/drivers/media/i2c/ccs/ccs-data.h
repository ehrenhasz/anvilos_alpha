#ifndef __CCS_DATA_H__
#define __CCS_DATA_H__
#include <linux/types.h>
struct device;
struct ccs_data_block_version {
	u16 version_major;
	u16 version_minor;
	u16 date_year;
	u8 date_month;
	u8 date_day;
};
struct ccs_reg {
	u16 addr;
	u16 len;
	u8 *value;
};
struct ccs_if_rule {
	u16 addr;
	u8 value;
	u8 mask;
};
struct ccs_frame_format_desc {
	u8 pixelcode;
	u16 value;
};
struct ccs_frame_format_descs {
	u8 num_column_descs;
	u8 num_row_descs;
	struct ccs_frame_format_desc *column_descs;
	struct ccs_frame_format_desc *row_descs;
};
struct ccs_pdaf_readout {
	u8 pdaf_readout_info_order;
	struct ccs_frame_format_descs *ffd;
};
struct ccs_rule {
	size_t num_if_rules;
	struct ccs_if_rule *if_rules;
	size_t num_read_only_regs;
	struct ccs_reg *read_only_regs;
	size_t num_manufacturer_regs;
	struct ccs_reg *manufacturer_regs;
	struct ccs_frame_format_descs *frame_format;
	struct ccs_pdaf_readout *pdaf_readout;
};
struct ccs_pdaf_pix_loc_block_desc {
	u8 block_type_id;
	u16 repeat_x;
};
struct ccs_pdaf_pix_loc_block_desc_group {
	u8 repeat_y;
	u16 num_block_descs;
	struct ccs_pdaf_pix_loc_block_desc *block_descs;
};
struct ccs_pdaf_pix_loc_pixel_desc {
	u8 pixel_type;
	u8 small_offset_x;
	u8 small_offset_y;
};
struct ccs_pdaf_pix_loc_pixel_desc_group {
	u8 num_descs;
	struct ccs_pdaf_pix_loc_pixel_desc *descs;
};
struct ccs_pdaf_pix_loc {
	u16 main_offset_x;
	u16 main_offset_y;
	u8 global_pdaf_type;
	u8 block_width;
	u8 block_height;
	u16 num_block_desc_groups;
	struct ccs_pdaf_pix_loc_block_desc_group *block_desc_groups;
	u8 num_pixel_desc_grups;
	struct ccs_pdaf_pix_loc_pixel_desc_group *pixel_desc_groups;
};
struct ccs_data_container {
	struct ccs_data_block_version *version;
	size_t num_sensor_read_only_regs;
	struct ccs_reg *sensor_read_only_regs;
	size_t num_sensor_manufacturer_regs;
	struct ccs_reg *sensor_manufacturer_regs;
	size_t num_sensor_rules;
	struct ccs_rule *sensor_rules;
	size_t num_module_read_only_regs;
	struct ccs_reg *module_read_only_regs;
	size_t num_module_manufacturer_regs;
	struct ccs_reg *module_manufacturer_regs;
	size_t num_module_rules;
	struct ccs_rule *module_rules;
	struct ccs_pdaf_pix_loc *sensor_pdaf;
	struct ccs_pdaf_pix_loc *module_pdaf;
	size_t license_length;
	char *license;
	bool end;
	void *backing;
};
int ccs_data_parse(struct ccs_data_container *ccsdata, const void *data,
		   size_t len, struct device *dev, bool verbose);
#endif  
