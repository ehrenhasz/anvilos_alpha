 
 
#ifndef _MALIDP_PRODUCT_H_
#define _MALIDP_PRODUCT_H_

 
#define MALIDP_CORE_ID(__product, __major, __minor, __status) \
	((((__product) & 0xFFFF) << 16) | (((__major) & 0xF) << 12) | \
	(((__minor) & 0xF) << 8) | ((__status) & 0xFF))

#define MALIDP_CORE_ID_PRODUCT_ID(__core_id) ((__u32)(__core_id) >> 16)
#define MALIDP_CORE_ID_MAJOR(__core_id)      (((__u32)(__core_id) >> 12) & 0xF)
#define MALIDP_CORE_ID_MINOR(__core_id)      (((__u32)(__core_id) >> 8) & 0xF)
#define MALIDP_CORE_ID_STATUS(__core_id)     (((__u32)(__core_id)) & 0xFF)

 
#define MALIDP_D71_PRODUCT_ID	0x0071
#define MALIDP_D32_PRODUCT_ID	0x0032

union komeda_config_id {
	struct {
		__u32	max_line_sz:16,
			n_pipelines:2,
			n_scalers:2,  
			n_layers:3,  
			n_richs:3,  
			reserved_bits:6;
	};
	__u32 value;
};

#endif  
