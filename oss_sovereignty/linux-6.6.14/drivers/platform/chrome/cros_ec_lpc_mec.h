#ifndef __CROS_EC_LPC_MEC_H
#define __CROS_EC_LPC_MEC_H
enum cros_ec_lpc_mec_emi_access_mode {
	ACCESS_TYPE_BYTE = 0x0,
	ACCESS_TYPE_WORD = 0x1,
	ACCESS_TYPE_LONG = 0x2,
	ACCESS_TYPE_LONG_AUTO_INCREMENT = 0x3,
};
enum cros_ec_lpc_mec_io_type {
	MEC_IO_READ,
	MEC_IO_WRITE,
};
#define MEC_EMI_HOST_TO_EC(MEC_EMI_BASE)	((MEC_EMI_BASE) + 0)
#define MEC_EMI_EC_TO_HOST(MEC_EMI_BASE)	((MEC_EMI_BASE) + 1)
#define MEC_EMI_EC_ADDRESS_B0(MEC_EMI_BASE)	((MEC_EMI_BASE) + 2)
#define MEC_EMI_EC_ADDRESS_B1(MEC_EMI_BASE)	((MEC_EMI_BASE) + 3)
#define MEC_EMI_EC_DATA_B0(MEC_EMI_BASE)	((MEC_EMI_BASE) + 4)
#define MEC_EMI_EC_DATA_B1(MEC_EMI_BASE)	((MEC_EMI_BASE) + 5)
#define MEC_EMI_EC_DATA_B2(MEC_EMI_BASE)	((MEC_EMI_BASE) + 6)
#define MEC_EMI_EC_DATA_B3(MEC_EMI_BASE)	((MEC_EMI_BASE) + 7)
void cros_ec_lpc_mec_init(unsigned int base, unsigned int end);
int cros_ec_lpc_mec_in_range(unsigned int offset, unsigned int length);
u8 cros_ec_lpc_io_bytes_mec(enum cros_ec_lpc_mec_io_type io_type,
			    unsigned int offset, unsigned int length, u8 *buf);
#endif  
