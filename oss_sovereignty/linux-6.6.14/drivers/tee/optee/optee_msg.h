 
 
#ifndef _OPTEE_MSG_H
#define _OPTEE_MSG_H

#include <linux/bitops.h>
#include <linux/types.h>

 

 

#define OPTEE_MSG_ATTR_TYPE_NONE		0x0
#define OPTEE_MSG_ATTR_TYPE_VALUE_INPUT		0x1
#define OPTEE_MSG_ATTR_TYPE_VALUE_OUTPUT	0x2
#define OPTEE_MSG_ATTR_TYPE_VALUE_INOUT		0x3
#define OPTEE_MSG_ATTR_TYPE_RMEM_INPUT		0x5
#define OPTEE_MSG_ATTR_TYPE_RMEM_OUTPUT		0x6
#define OPTEE_MSG_ATTR_TYPE_RMEM_INOUT		0x7
#define OPTEE_MSG_ATTR_TYPE_FMEM_INPUT		OPTEE_MSG_ATTR_TYPE_RMEM_INPUT
#define OPTEE_MSG_ATTR_TYPE_FMEM_OUTPUT		OPTEE_MSG_ATTR_TYPE_RMEM_OUTPUT
#define OPTEE_MSG_ATTR_TYPE_FMEM_INOUT		OPTEE_MSG_ATTR_TYPE_RMEM_INOUT
#define OPTEE_MSG_ATTR_TYPE_TMEM_INPUT		0x9
#define OPTEE_MSG_ATTR_TYPE_TMEM_OUTPUT		0xa
#define OPTEE_MSG_ATTR_TYPE_TMEM_INOUT		0xb

#define OPTEE_MSG_ATTR_TYPE_MASK		GENMASK(7, 0)

 
#define OPTEE_MSG_ATTR_META			BIT(8)

 
#define OPTEE_MSG_ATTR_NONCONTIG		BIT(9)

 
#define OPTEE_MSG_ATTR_CACHE_SHIFT		16
#define OPTEE_MSG_ATTR_CACHE_MASK		GENMASK(2, 0)
#define OPTEE_MSG_ATTR_CACHE_PREDEFINED		0

 
#define OPTEE_MSG_LOGIN_PUBLIC			0x00000000
#define OPTEE_MSG_LOGIN_USER			0x00000001
#define OPTEE_MSG_LOGIN_GROUP			0x00000002
#define OPTEE_MSG_LOGIN_APPLICATION		0x00000004
#define OPTEE_MSG_LOGIN_APPLICATION_USER	0x00000005
#define OPTEE_MSG_LOGIN_APPLICATION_GROUP	0x00000006

 
#define OPTEE_MSG_NONCONTIG_PAGE_SIZE		4096

#define OPTEE_MSG_FMEM_INVALID_GLOBAL_ID	0xffffffffffffffff

 
struct optee_msg_param_tmem {
	u64 buf_ptr;
	u64 size;
	u64 shm_ref;
};

 
struct optee_msg_param_rmem {
	u64 offs;
	u64 size;
	u64 shm_ref;
};

 
struct optee_msg_param_fmem {
	u32 offs_low;
	u16 offs_high;
	u16 internal_offs;
	u64 size;
	u64 global_id;
};

 
struct optee_msg_param_value {
	u64 a;
	u64 b;
	u64 c;
};

 
struct optee_msg_param {
	u64 attr;
	union {
		struct optee_msg_param_tmem tmem;
		struct optee_msg_param_rmem rmem;
		struct optee_msg_param_fmem fmem;
		struct optee_msg_param_value value;
		u8 octets[24];
	} u;
};

 
struct optee_msg_arg {
	u32 cmd;
	u32 func;
	u32 session;
	u32 cancel_id;
	u32 pad;
	u32 ret;
	u32 ret_origin;
	u32 num_params;

	 
	struct optee_msg_param params[];
};

 
#define OPTEE_MSG_GET_ARG_SIZE(num_params) \
	(sizeof(struct optee_msg_arg) + \
	 sizeof(struct optee_msg_param) * (num_params))

 

 
#define OPTEE_MSG_UID_0			0x384fb3e0
#define OPTEE_MSG_UID_1			0xe7f811e3
#define OPTEE_MSG_UID_2			0xaf630002
#define OPTEE_MSG_UID_3			0xa5d5c51b
#define OPTEE_MSG_IMAGE_LOAD_UID_0	0xa3fbeab1
#define OPTEE_MSG_IMAGE_LOAD_UID_1	0x1246315d
#define OPTEE_MSG_IMAGE_LOAD_UID_2	0xc7c406b9
#define OPTEE_MSG_IMAGE_LOAD_UID_3	0xc03cbea4
#define OPTEE_MSG_FUNCID_CALLS_UID	0xFF01

 
#define OPTEE_MSG_REVISION_MAJOR	2
#define OPTEE_MSG_REVISION_MINOR	0
#define OPTEE_MSG_FUNCID_CALLS_REVISION	0xFF03

 
#define OPTEE_MSG_OS_OPTEE_UUID_0	0x486178e0
#define OPTEE_MSG_OS_OPTEE_UUID_1	0xe7f811e3
#define OPTEE_MSG_OS_OPTEE_UUID_2	0xbc5e0002
#define OPTEE_MSG_OS_OPTEE_UUID_3	0xa5d5c51b
#define OPTEE_MSG_FUNCID_GET_OS_UUID	0x0000

 
#define OPTEE_MSG_FUNCID_GET_OS_REVISION	0x0001

 
#define OPTEE_MSG_CMD_OPEN_SESSION	0
#define OPTEE_MSG_CMD_INVOKE_COMMAND	1
#define OPTEE_MSG_CMD_CLOSE_SESSION	2
#define OPTEE_MSG_CMD_CANCEL		3
#define OPTEE_MSG_CMD_REGISTER_SHM	4
#define OPTEE_MSG_CMD_UNREGISTER_SHM	5
#define OPTEE_MSG_CMD_DO_BOTTOM_HALF	6
#define OPTEE_MSG_CMD_STOP_ASYNC_NOTIF	7
#define OPTEE_MSG_FUNCID_CALL_WITH_ARG	0x0004

#endif  
