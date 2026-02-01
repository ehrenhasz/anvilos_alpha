
 

#include "img-ir-hw.h"

 
static int img_ir_rc5_scancode(int len, u64 raw, u64 enabled_protocols,
				struct img_ir_scancode_req *request)
{
	unsigned int addr, cmd, tgl, start;

	 
	raw   >>= 2;

	start	=  (raw >> 13)	& 0x01;
	tgl	=  (raw >> 11)	& 0x01;
	addr	=  (raw >>  6)	& 0x1f;
	cmd	=   raw		& 0x3f;
	 
	cmd	+= ((raw >> 12) & 0x01) ? 0 : 0x40;

	if (!start)
		return -EINVAL;

	request->protocol = RC_PROTO_RC5;
	request->scancode = addr << 8 | cmd;
	request->toggle   = tgl;
	return IMG_IR_SCANCODE;
}

 
static int img_ir_rc5_filter(const struct rc_scancode_filter *in,
				 struct img_ir_filter *out, u64 protocols)
{
	 
	return -EINVAL;
}

 
struct img_ir_decoder img_ir_rc5 = {
	.type      = RC_PROTO_BIT_RC5,
	.control   = {
		.bitoriend2	= 1,
		.code_type	= IMG_IR_CODETYPE_BIPHASE,
		.decodend2	= 1,
	},
	 
	.tolerance	= 16,
	.unit		= 888888,  
	.timings	= {
		 
		.s10 = {
			.pulse	= { 1 },
			.space	= { 1 },
		},

		 
		.s11 = {
			.pulse	= { 1 },
			.space	= { 1 },
		},

		 
		.ft  = {
			.minlen = 14,
			.maxlen = 14,
			.ft_min = 5,
		},
	},

	 
	.scancode	= img_ir_rc5_scancode,
	.filter		= img_ir_rc5_filter,
};
