 
 
#ifndef __BERLIN2_COMMON_H
#define __BERLIN2_COMMON_H

struct berlin2_gate_data {
	const char *name;
	const char *parent_name;
	u8 bit_idx;
	unsigned long flags;
};

#endif  
