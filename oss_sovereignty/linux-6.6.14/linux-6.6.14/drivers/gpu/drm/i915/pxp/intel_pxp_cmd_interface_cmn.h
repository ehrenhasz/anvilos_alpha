#ifndef __INTEL_PXP_FW_INTERFACE_CMN_H__
#define __INTEL_PXP_FW_INTERFACE_CMN_H__
#include <linux/types.h>
#define PXP_APIVER(x, y) (((x) & 0xFFFF) << 16 | ((y) & 0xFFFF))
enum pxp_status {
	PXP_STATUS_SUCCESS = 0x0,
	PXP_STATUS_ERROR_API_VERSION = 0x1002,
	PXP_STATUS_NOT_READY = 0x100e,
	PXP_STATUS_PLATFCONFIG_KF1_NOVERIF = 0x101a,
	PXP_STATUS_PLATFCONFIG_KF1_BAD = 0x101f,
	PXP_STATUS_OP_NOT_PERMITTED = 0x4013
};
struct pxp_cmd_header {
	u32 api_version;
	u32 command_id;
	union {
		u32 status;  
		u32 stream_id;  
#define PXP_CMDHDR_EXTDATA_SESSION_VALID GENMASK(0, 0)
#define PXP_CMDHDR_EXTDATA_APP_TYPE GENMASK(1, 1)
#define PXP_CMDHDR_EXTDATA_SESSION_ID GENMASK(17, 2)
	};
	u32 buffer_len;
} __packed;
#endif  
