 

#define pr_fmt(fmt)		KBUILD_MODNAME ": " fmt

#include <linux/module.h>
#include <linux/crc8.h>
#include <linux/printk.h>

 
void crc8_populate_msb(u8 table[CRC8_TABLE_SIZE], u8 polynomial)
{
	int i, j;
	const u8 msbit = 0x80;
	u8 t = msbit;

	table[0] = 0;

	for (i = 1; i < CRC8_TABLE_SIZE; i *= 2) {
		t = (t << 1) ^ (t & msbit ? polynomial : 0);
		for (j = 0; j < i; j++)
			table[i+j] = table[j] ^ t;
	}
}
EXPORT_SYMBOL(crc8_populate_msb);

 
void crc8_populate_lsb(u8 table[CRC8_TABLE_SIZE], u8 polynomial)
{
	int i, j;
	u8 t = 1;

	table[0] = 0;

	for (i = (CRC8_TABLE_SIZE >> 1); i; i >>= 1) {
		t = (t >> 1) ^ (t & 1 ? polynomial : 0);
		for (j = 0; j < CRC8_TABLE_SIZE; j += 2*i)
			table[i+j] = table[j] ^ t;
	}
}
EXPORT_SYMBOL(crc8_populate_lsb);

 
u8 crc8(const u8 table[CRC8_TABLE_SIZE], const u8 *pdata, size_t nbytes, u8 crc)
{
	 
	while (nbytes-- > 0)
		crc = table[(crc ^ *pdata++) & 0xff];

	return crc;
}
EXPORT_SYMBOL(crc8);

MODULE_DESCRIPTION("CRC8 (by Williams, Ross N.) function");
MODULE_AUTHOR("Broadcom Corporation");
MODULE_LICENSE("Dual BSD/GPL");
