


#ifndef _HCALLS_H
#define _HCALLS_H

#include <linux/types.h>
#include <asm/byteorder.h>
#include <asm/hvcall.h>
#include "cxl.h"

#define SG_BUFFER_SIZE 4096
#define SG_MAX_ENTRIES 256

struct sg_list {
	u64 phys_addr;
	u64 len;
};


#define CXL_PE_CSRP_VALID			(1ULL << 63)
#define CXL_PE_PROBLEM_STATE			(1ULL << 62)
#define CXL_PE_SECONDARY_SEGMENT_TBL_SRCH	(1ULL << 61)
#define CXL_PE_TAGS_ACTIVE			(1ULL << 60)
#define CXL_PE_USER_STATE			(1ULL << 59)
#define CXL_PE_TRANSLATION_ENABLED		(1ULL << 58)
#define CXL_PE_64_BIT				(1ULL << 57)
#define CXL_PE_PRIVILEGED_PROCESS		(1ULL << 56)

#define CXL_PROCESS_ELEMENT_VERSION 1
struct cxl_process_element_hcall {
	__be64 version;
	__be64 flags;
	u8     reserved0[12];
	__be32 pslVirtualIsn;
	u8     applicationVirtualIsnBitmap[256];
	u8     reserved1[144];
	struct cxl_process_element_common common;
	u8     reserved4[12];
} __packed;

#define H_STATE_NORMAL              1
#define H_STATE_DISABLE             2
#define H_STATE_TEMP_UNAVAILABLE    3
#define H_STATE_PERM_UNAVAILABLE    4


long cxl_h_attach_process(u64 unit_address, struct cxl_process_element_hcall *element,
			u64 *process_token, u64 *mmio_addr, u64 *mmio_size);


long cxl_h_detach_process(u64 unit_address, u64 process_token);


long cxl_h_reset_afu(u64 unit_address);


long cxl_h_suspend_process(u64 unit_address, u64 process_token);


long cxl_h_resume_process(u64 unit_address, u64 process_token);


long cxl_h_read_error_state(u64 unit_address, u64 *state);


long cxl_h_get_afu_err(u64 unit_address, u64 offset, u64 buf_address, u64 len);


long cxl_h_get_config(u64 unit_address, u64 cr_num, u64 offset,
		u64 buf_address, u64 len);


long cxl_h_terminate_process(u64 unit_address, u64 process_token);


long cxl_h_collect_vpd(u64 unit_address, u64 record, u64 list_address,
		       u64 num, u64 *out);


long cxl_h_get_fn_error_interrupt(u64 unit_address, u64 *reg);


long cxl_h_ack_fn_error_interrupt(u64 unit_address, u64 value);


long cxl_h_get_error_log(u64 unit_address, u64 value);


long cxl_h_collect_int_info(u64 unit_address, u64 process_token,
			struct cxl_irq_info *info);


long cxl_h_control_faults(u64 unit_address, u64 process_token,
			u64 control_mask, u64 reset_mask);


long cxl_h_reset_adapter(u64 unit_address);


long cxl_h_collect_vpd_adapter(u64 unit_address, u64 list_address,
			       u64 num, u64 *out);


long cxl_h_download_adapter_image(u64 unit_address,
				  u64 list_address, u64 num,
				  u64 *out);


long cxl_h_validate_adapter_image(u64 unit_address,
				  u64 list_address, u64 num,
				  u64 *out);
#endif 
