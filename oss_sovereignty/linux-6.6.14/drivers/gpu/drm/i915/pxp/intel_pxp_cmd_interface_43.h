 
 

#ifndef __INTEL_PXP_FW_INTERFACE_43_H__
#define __INTEL_PXP_FW_INTERFACE_43_H__

#include <linux/types.h>
#include "intel_pxp_cmd_interface_cmn.h"

 
#define PXP43_CMDID_START_HUC_AUTH 0x0000003A
#define PXP43_CMDID_NEW_HUC_AUTH 0x0000003F  
#define PXP43_CMDID_INIT_SESSION 0x00000036

 
#define PXP43_MAX_HECI_INOUT_SIZE (SZ_32K)

 
#define PXP43_HUC_AUTH_INOUT_SIZE (SZ_4K)

 
struct pxp43_start_huc_auth_in {
	struct pxp_cmd_header header;
	__le64 huc_base_address;
} __packed;

 
struct pxp43_new_huc_auth_in {
	struct pxp_cmd_header header;
	u64 huc_base_address;
	u32 huc_size;
} __packed;

 
struct pxp43_huc_auth_out {
	struct pxp_cmd_header header;
} __packed;

 
struct pxp43_create_arb_in {
	struct pxp_cmd_header header;
		 
		#define PXP43_INIT_SESSION_VALID BIT(0)
		#define PXP43_INIT_SESSION_APPTYPE BIT(1)
		#define PXP43_INIT_SESSION_APPID GENMASK(17, 2)
	u32 protection_mode;
		#define PXP43_INIT_SESSION_PROTECTION_ARB 0x2
	u32 sub_session_id;
	u32 init_flags;
	u32 rsvd[12];
} __packed;

 
struct pxp43_create_arb_out {
	struct pxp_cmd_header header;
	u32 rsvd[8];
} __packed;

#endif  
