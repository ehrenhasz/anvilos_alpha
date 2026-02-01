




#include <linux/delay.h>
#include <linux/io.h>
#include <linux/mutex.h>
#include <linux/types.h>

#include "cros_ec_lpc_mec.h"

 
static DEFINE_MUTEX(io_mutex);
static u16 mec_emi_base, mec_emi_end;

 
static void cros_ec_lpc_mec_emi_write_address(u16 addr,
			enum cros_ec_lpc_mec_emi_access_mode access_type)
{
	outb((addr & 0xfc) | access_type, MEC_EMI_EC_ADDRESS_B0(mec_emi_base));
	outb((addr >> 8) & 0x7f, MEC_EMI_EC_ADDRESS_B1(mec_emi_base));
}

 
int cros_ec_lpc_mec_in_range(unsigned int offset, unsigned int length)
{
	if (length == 0)
		return -EINVAL;

	if (WARN_ON(mec_emi_base == 0 || mec_emi_end == 0))
		return -EINVAL;

	if (offset >= mec_emi_base && offset < mec_emi_end) {
		if (WARN_ON(offset + length - 1 >= mec_emi_end))
			return -EINVAL;
		return 1;
	}

	if (WARN_ON(offset + length > mec_emi_base && offset < mec_emi_end))
		return -EINVAL;

	return 0;
}

 
u8 cros_ec_lpc_io_bytes_mec(enum cros_ec_lpc_mec_io_type io_type,
			    unsigned int offset, unsigned int length,
			    u8 *buf)
{
	int i = 0;
	int io_addr;
	u8 sum = 0;
	enum cros_ec_lpc_mec_emi_access_mode access, new_access;

	 
	WARN_ON(mec_emi_base == 0 || mec_emi_end == 0);
	if (mec_emi_base == 0 || mec_emi_end == 0)
		return 0;

	 
	if (offset & 0x3 || length < 4)
		access = ACCESS_TYPE_BYTE;
	else
		access = ACCESS_TYPE_LONG_AUTO_INCREMENT;

	mutex_lock(&io_mutex);

	 
	cros_ec_lpc_mec_emi_write_address(offset, access);

	 
	io_addr = MEC_EMI_EC_DATA_B0(mec_emi_base) + (offset & 0x3);
	while (i < length) {
		while (io_addr <= MEC_EMI_EC_DATA_B3(mec_emi_base)) {
			if (io_type == MEC_IO_READ)
				buf[i] = inb(io_addr++);
			else
				outb(buf[i], io_addr++);

			sum += buf[i++];
			offset++;

			 
			if (i == length)
				goto done;
		}

		 
		if (length - i < 4 && io_type == MEC_IO_WRITE)
			new_access = ACCESS_TYPE_BYTE;
		else
			new_access = ACCESS_TYPE_LONG_AUTO_INCREMENT;

		if (new_access != access ||
		    access != ACCESS_TYPE_LONG_AUTO_INCREMENT) {
			access = new_access;
			cros_ec_lpc_mec_emi_write_address(offset, access);
		}

		 
		io_addr = MEC_EMI_EC_DATA_B0(mec_emi_base);
	}

done:
	mutex_unlock(&io_mutex);

	return sum;
}
EXPORT_SYMBOL(cros_ec_lpc_io_bytes_mec);

void cros_ec_lpc_mec_init(unsigned int base, unsigned int end)
{
	mec_emi_base = base;
	mec_emi_end = end;
}
EXPORT_SYMBOL(cros_ec_lpc_mec_init);
