#ifndef _ASM_POWERPC_RTAS_TYPES_H
#define _ASM_POWERPC_RTAS_TYPES_H
#include <linux/compiler_attributes.h>
typedef __be32 rtas_arg_t;
struct rtas_args {
	__be32 token;
	__be32 nargs;
	__be32 nret;
	rtas_arg_t args[16];
	rtas_arg_t *rets;      
} __aligned(8);
struct rtas_t {
	unsigned long entry;		 
	unsigned long base;		 
	unsigned long size;
	struct device_node *dev;	 
};
struct rtas_error_log {
	u8		byte0;			 
	u8		byte1;
	u8		byte2;
	u8		byte3;			 
	__be32		extended_log_length;	 
	unsigned char	buffer[1];		 
};
struct rtas_ext_event_log_v6 {
	u8 byte0;
	u8 byte1;			 
	u8 byte2;
	u8 byte3;			 
	u8 reserved[8];			 
	__be32  company_id;		 
	u8 vendor_log[1];		 
};
struct pseries_errorlog {
	__be16 id;			 
	__be16 length;			 
	u8 version;			 
	u8 subtype;			 
	__be16 creator_component;	 
	u8 data[];			 
};
struct pseries_hp_errorlog {
	u8	resource;
	u8	action;
	u8	id_type;
	u8	reserved;
	union {
		__be32	drc_index;
		__be32	drc_count;
		struct { __be32 count, index; } ic;
		char	drc_name[1];
	} _drc_u;
};
#endif  
