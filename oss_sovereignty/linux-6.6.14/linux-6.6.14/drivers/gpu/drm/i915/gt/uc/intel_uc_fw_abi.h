#ifndef _INTEL_UC_FW_ABI_H
#define _INTEL_UC_FW_ABI_H
#include <linux/types.h>
#include <linux/build_bug.h>
struct uc_css_header {
	u32 module_type;
	u32 header_size_dw;
	u32 header_version;
	u32 module_id;
	u32 module_vendor;
	u32 date;
#define CSS_DATE_DAY			(0xFF << 0)
#define CSS_DATE_MONTH			(0xFF << 8)
#define CSS_DATE_YEAR			(0xFFFF << 16)
	u32 size_dw;  
	u32 key_size_dw;
	u32 modulus_size_dw;
	u32 exponent_size_dw;
	u32 time;
#define CSS_TIME_HOUR			(0xFF << 0)
#define CSS_DATE_MIN			(0xFF << 8)
#define CSS_DATE_SEC			(0xFFFF << 16)
	char username[8];
	char buildnumber[12];
	u32 sw_version;
#define CSS_SW_VERSION_UC_MAJOR		(0xFF << 16)
#define CSS_SW_VERSION_UC_MINOR		(0xFF << 8)
#define CSS_SW_VERSION_UC_PATCH		(0xFF << 0)
	u32 vf_version;
	u32 reserved0[12];
	union {
		u32 private_data_size;  
		u32 reserved1;
	};
	u32 header_info;
} __packed;
static_assert(sizeof(struct uc_css_header) == 128);
#endif  
