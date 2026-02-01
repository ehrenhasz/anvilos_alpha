
 

#include "img-ir-hw.h"

 
static int img_ir_rc6_scancode(int len, u64 raw, u64 enabled_protocols,
				struct img_ir_scancode_req *request)
{
	unsigned int addr, cmd, mode, trl1, trl2;

	 

	raw	>>= 27;

	trl1	= (raw >>  17)	& 0x01;
	trl2	= (raw >>  16)	& 0x01;

	mode	= (raw >>  18)	& 0x07;
	addr	= (raw >>   8)	& 0xff;
	cmd	=  raw		& 0xff;

	 
	if (trl1 == trl2)
		return -EINVAL;

	 
	if (mode)
		return -EINVAL;

	request->protocol = RC_PROTO_RC6_0;
	request->scancode = addr << 8 | cmd;
	request->toggle	  = trl2;
	return IMG_IR_SCANCODE;
}

 
static int img_ir_rc6_filter(const struct rc_scancode_filter *in,
				 struct img_ir_filter *out, u64 protocols)
{
	 
	return -EINVAL;
}

 
struct img_ir_decoder img_ir_rc6 = {
	.type		= RC_PROTO_BIT_RC6_0,
	.control	= {
		.bitorien	= 1,
		.code_type	= IMG_IR_CODETYPE_BIPHASE,
		.decoden	= 1,
		.decodinpol	= 1,
	},
	 
	.tolerance	= 20,
	 
	.timings	= {
		 
		.ldr = {
			.pulse	= { 650 },
			.space	= { 660 },
		},
		 
		.s00 = {
			.pulse	= { 370 },
			.space	= { 370 },
		},
		 
		.s01 = {
			.pulse	= { 370 },
			.space	= { 370 },
		},
		 
		.ft  = {
			.minlen = 21,
			.maxlen = 21,
			.ft_min = 2666,	 
		},
	},

	 
	.scancode	= img_ir_rc6_scancode,
	.filter		= img_ir_rc6_filter,
};
