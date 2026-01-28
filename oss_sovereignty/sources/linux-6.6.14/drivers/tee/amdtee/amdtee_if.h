





#ifndef AMDTEE_IF_H
#define AMDTEE_IF_H

#include <linux/types.h>


#define TEE_MAX_PARAMS		4


struct memref {
	u32 buf_id;
	u32 offset;
	u32 size;
};

struct value {
	u32 a;
	u32 b;
};


union tee_op_param {
	struct memref mref;
	struct value val;
};

struct tee_operation {
	u32 param_types;
	union tee_op_param params[TEE_MAX_PARAMS];
};


#define TEE_OP_PARAM_TYPE_NONE                  0
#define TEE_OP_PARAM_TYPE_VALUE_INPUT           1
#define TEE_OP_PARAM_TYPE_VALUE_OUTPUT          2
#define TEE_OP_PARAM_TYPE_VALUE_INOUT           3
#define TEE_OP_PARAM_TYPE_INVALID               4
#define TEE_OP_PARAM_TYPE_MEMREF_INPUT          5
#define TEE_OP_PARAM_TYPE_MEMREF_OUTPUT         6
#define TEE_OP_PARAM_TYPE_MEMREF_INOUT          7

#define TEE_PARAM_TYPE_GET(t, i)        (((t) >> ((i) * 4)) & 0xF)
#define TEE_PARAM_TYPES(t0, t1, t2, t3) \
	((t0) | ((t1) << 4) | ((t2) << 8) | ((t3) << 12))






struct tee_sg_desc {
	u32 low_addr;
	u32 hi_addr;
	u32 size;
};


#define TEE_MAX_SG_DESC 64
struct tee_sg_list {
	u32 count;
	u32 size;
	struct tee_sg_desc buf[TEE_MAX_SG_DESC];
};


struct tee_cmd_map_shared_mem {
	u32 buf_id;
	struct tee_sg_list sg_list;
};


struct tee_cmd_unmap_shared_mem {
	u32 buf_id;
};


struct tee_cmd_load_ta {
	u32 low_addr;
	u32 hi_addr;
	u32 size;
	u32 ta_handle;
	u32 return_origin;
};


struct tee_cmd_unload_ta {
	u32 ta_handle;
};


struct tee_cmd_open_session {
	u32 ta_handle;
	u32 session_info;
	struct tee_operation op;
	u32 return_origin;
};


struct tee_cmd_close_session {
	u32 ta_handle;
	u32 session_info;
};


struct tee_cmd_invoke_cmd {
	u32 ta_handle;
	u32 cmd_id;
	u32 session_info;
	struct tee_operation op;
	u32 return_origin;
};

#endif 
