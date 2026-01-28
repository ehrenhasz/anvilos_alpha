#ifndef __CXGB4_DCB_H
#define __CXGB4_DCB_H
#include <linux/netdevice.h>
#include <linux/dcbnl.h>
#include <net/dcbnl.h>
#ifdef CONFIG_CHELSIO_T4_DCB
#define CXGB4_DCBX_FW_SUPPORT \
	(DCB_CAP_DCBX_VER_CEE | \
	 DCB_CAP_DCBX_VER_IEEE | \
	 DCB_CAP_DCBX_LLD_MANAGED)
#define CXGB4_DCBX_HOST_SUPPORT \
	(DCB_CAP_DCBX_VER_CEE | \
	 DCB_CAP_DCBX_VER_IEEE | \
	 DCB_CAP_DCBX_HOST)
#define CXGB4_MAX_PRIORITY      CXGB4_MAX_DCBX_APP_SUPPORTED
#define CXGB4_MAX_TCS           CXGB4_MAX_DCBX_APP_SUPPORTED
#define INIT_PORT_DCB_CMD(__pcmd, __port, __op, __action) \
	do { \
		memset(&(__pcmd), 0, sizeof(__pcmd)); \
		(__pcmd).op_to_portid = \
			cpu_to_be32(FW_CMD_OP_V(FW_PORT_CMD) | \
				    FW_CMD_REQUEST_F | \
				    FW_CMD_##__op##_F | \
				    FW_PORT_CMD_PORTID_V(__port)); \
		(__pcmd).action_to_len16 = \
			cpu_to_be32(FW_PORT_CMD_ACTION_V(__action) | \
				    FW_LEN16(pcmd)); \
	} while (0)
#define INIT_PORT_DCB_READ_PEER_CMD(__pcmd, __port) \
	INIT_PORT_DCB_CMD(__pcmd, __port, READ, FW_PORT_ACTION_DCB_READ_RECV)
#define INIT_PORT_DCB_READ_LOCAL_CMD(__pcmd, __port) \
	INIT_PORT_DCB_CMD(__pcmd, __port, READ, FW_PORT_ACTION_DCB_READ_TRANS)
#define INIT_PORT_DCB_READ_SYNC_CMD(__pcmd, __port) \
	INIT_PORT_DCB_CMD(__pcmd, __port, READ, FW_PORT_ACTION_DCB_READ_DET)
#define INIT_PORT_DCB_WRITE_CMD(__pcmd, __port) \
	INIT_PORT_DCB_CMD(__pcmd, __port, EXEC, FW_PORT_ACTION_L2_DCB_CFG)
#define IEEE_FAUX_SYNC(__dev, __dcb) \
	do { \
		if ((__dcb)->dcb_version == FW_PORT_DCB_VER_IEEE) \
			cxgb4_dcb_state_fsm((__dev), \
					    CXGB4_DCB_INPUT_FW_ALLSYNCED); \
	} while (0)
enum cxgb4_dcb_state {
	CXGB4_DCB_STATE_START,		 
	CXGB4_DCB_STATE_HOST,		 
	CXGB4_DCB_STATE_FW_INCOMPLETE,	 
	CXGB4_DCB_STATE_FW_ALLSYNCED,	 
};
enum cxgb4_dcb_state_input {
	CXGB4_DCB_INPUT_FW_DISABLED,	 
	CXGB4_DCB_INPUT_FW_ENABLED,	 
	CXGB4_DCB_INPUT_FW_INCOMPLETE,	 
	CXGB4_DCB_INPUT_FW_ALLSYNCED,	 
};
enum cxgb4_dcb_fw_msgs {
	CXGB4_DCB_FW_PGID	= 0x01,
	CXGB4_DCB_FW_PGRATE	= 0x02,
	CXGB4_DCB_FW_PRIORATE	= 0x04,
	CXGB4_DCB_FW_PFC	= 0x08,
	CXGB4_DCB_FW_APP_ID	= 0x10,
};
#define CXGB4_MAX_DCBX_APP_SUPPORTED 8
struct port_dcb_info {
	enum cxgb4_dcb_state state;	 
	enum cxgb4_dcb_fw_msgs msgs;	 
	unsigned int supported;		 
	bool enabled;			 
	u32	pgid;			 
	u8	dcb_version;		 
	u8	pfcen;			 
	u8	pg_num_tcs_supported;	 
	u8	pfc_num_tcs_supported;	 
	u8	pgrate[8];		 
	u8	priorate[8];		 
	u8	tsa[8];			 
	struct app_priority {  
		u8	user_prio_map;	 
		u8	sel_field;	 
		u16	protocolid;	 
	} app_priority[CXGB4_MAX_DCBX_APP_SUPPORTED];
};
void cxgb4_dcb_state_init(struct net_device *);
void cxgb4_dcb_version_init(struct net_device *);
void cxgb4_dcb_reset(struct net_device *dev);
void cxgb4_dcb_state_fsm(struct net_device *, enum cxgb4_dcb_state_input);
void cxgb4_dcb_handle_fw_update(struct adapter *, const struct fw_port_cmd *);
void cxgb4_dcb_set_caps(struct adapter *, const struct fw_port_cmd *);
extern const struct dcbnl_rtnl_ops cxgb4_dcb_ops;
static inline __u8 bitswap_1(unsigned char val)
{
	return ((val & 0x80) >> 7) |
	       ((val & 0x40) >> 5) |
	       ((val & 0x20) >> 3) |
	       ((val & 0x10) >> 1) |
	       ((val & 0x08) << 1) |
	       ((val & 0x04) << 3) |
	       ((val & 0x02) << 5) |
	       ((val & 0x01) << 7);
}
extern const char * const dcb_ver_array[];
#define CXGB4_DCB_ENABLED true
#else  
static inline void cxgb4_dcb_state_init(struct net_device *dev)
{
}
#define CXGB4_DCB_ENABLED false
#endif  
#endif  
