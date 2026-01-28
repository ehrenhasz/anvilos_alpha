


#ifndef __VPD_DECODE_H
#define __VPD_DECODE_H

#include <linux/types.h>

enum {
	VPD_OK = 0,
	VPD_FAIL,
};

enum {
	VPD_TYPE_TERMINATOR = 0,
	VPD_TYPE_STRING,
	VPD_TYPE_INFO                = 0xfe,
	VPD_TYPE_IMPLICIT_TERMINATOR = 0xff,
};


typedef int vpd_decode_callback(const u8 *key, u32 key_len,
				const u8 *value, u32 value_len,
				void *arg);


int vpd_decode_string(const u32 max_len, const u8 *input_buf, u32 *consumed,
		      vpd_decode_callback callback, void *callback_arg);

#endif  
