
 

#include <linux/module.h>
#include <linux/types.h>
#include <linux/crc64.h>
#include "crc64table.h"

MODULE_DESCRIPTION("CRC64 calculations");
MODULE_LICENSE("GPL v2");

 
u64 __pure crc64_be(u64 crc, const void *p, size_t len)
{
	size_t i, t;

	const unsigned char *_p = p;

	for (i = 0; i < len; i++) {
		t = ((crc >> 56) ^ (*_p++)) & 0xFF;
		crc = crc64table[t] ^ (crc << 8);
	}

	return crc;
}
EXPORT_SYMBOL_GPL(crc64_be);

 
u64 __pure crc64_rocksoft_generic(u64 crc, const void *p, size_t len)
{
	const unsigned char *_p = p;
	size_t i;

	crc = ~crc;

	for (i = 0; i < len; i++)
		crc = (crc >> 8) ^ crc64rocksofttable[(crc & 0xff) ^ *_p++];

	return ~crc;
}
EXPORT_SYMBOL_GPL(crc64_rocksoft_generic);
