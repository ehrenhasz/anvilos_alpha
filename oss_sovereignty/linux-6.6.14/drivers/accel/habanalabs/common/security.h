 

#ifndef SECURITY_H_
#define SECURITY_H_

#include <linux/io-64-nonatomic-lo-hi.h>

struct hl_device;

 
#define HL_MAX_NUM_OF_GLBL_ERR_CAUSE		10
#define HL_GLBL_ERR_ADDRESS_MASK		GENMASK(11, 0)
 
#define HL_GLBL_ERR_ADDR_OFFSET		0xF44
 
#define HL_GLBL_ERR_CAUSE_OFFSET	0xF48

 
struct hl_special_block_info {
	int block_type;
	u32 base_addr;
	u32 major;
	u32 minor;
	u32 sub_minor;
	u32 major_offset;
	u32 minor_offset;
	u32 sub_minor_offset;
};

 
struct hl_automated_pb_cfg {
	struct hl_special_block_info addr;
	u32 prot_map;
	u32 data_map;
	const u32 *data;
	u8 data_size;
};

 
struct hl_special_blocks_cfg {
	struct hl_automated_pb_cfg *priv_automated_pb_cfg;
	struct hl_automated_pb_cfg *sec_automated_pb_cfg;
	struct hl_skip_blocks_cfg *skip_blocks_cfg;
	u32 priv_cfg_size;
	u32 sec_cfg_size;
	u8 prot_lvl_priv;
};

 

 
struct hl_skip_blocks_cfg {
	int *block_types;
	size_t block_types_len;
	struct range *block_ranges;
	size_t block_ranges_len;
	bool (*skip_block_hook)(struct hl_device *hdev,
				struct hl_special_blocks_cfg *special_blocks_cfg,
				u32 blk_idx, u32 major, u32 minor, u32 sub_minor);
};

 
struct iterate_special_ctx {
	 
	int (*fn)(struct hl_device *hdev, u32 block_id, u32 major, u32 minor,
						u32 sub_minor, void *data);
	void *data;
};

int hl_iterate_special_blocks(struct hl_device *hdev, struct iterate_special_ctx *ctx);
void hl_check_for_glbl_errors(struct hl_device *hdev);

#endif  
