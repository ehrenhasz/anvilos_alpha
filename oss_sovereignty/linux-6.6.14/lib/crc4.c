
 

#include <linux/crc4.h>
#include <linux/module.h>

static const uint8_t crc4_tab[] = {
	0x0, 0x7, 0xe, 0x9, 0xb, 0xc, 0x5, 0x2,
	0x1, 0x6, 0xf, 0x8, 0xa, 0xd, 0x4, 0x3,
};

 
uint8_t crc4(uint8_t c, uint64_t x, int bits)
{
	int i;

	 
	x &= (1ull << bits) - 1;

	 
	bits = (bits + 3) & ~0x3;

	 
	for (i = bits - 4; i >= 0; i -= 4)
		c = crc4_tab[c ^ ((x >> i) & 0xf)];

	return c;
}
EXPORT_SYMBOL_GPL(crc4);

MODULE_DESCRIPTION("CRC4 calculations");
MODULE_LICENSE("GPL");
