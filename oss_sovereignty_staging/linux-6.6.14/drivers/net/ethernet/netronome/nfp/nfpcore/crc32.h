 
 

#ifndef NFP_CRC32_H
#define NFP_CRC32_H

#include <linux/crc32.h>

 
static inline u32 crc32_posix_end(u32 crc, size_t total_len)
{
	 
	while (total_len != 0) {
		u8 c = total_len & 0xff;

		crc = crc32_be(crc, &c, 1);
		total_len >>= 8;
	}

	return ~crc;
}

static inline u32 crc32_posix(const void *buff, size_t len)
{
	return crc32_posix_end(crc32_be(0, buff, len), len);
}

#endif  
