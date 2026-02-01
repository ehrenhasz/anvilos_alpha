 
 

#ifndef TDA18250_PRIV_H
#define TDA18250_PRIV_H

#include "tda18250.h"

#define R00_ID1		0x00	 
#define R01_ID2		0x01	 
#define R02_ID3		0x02	 
#define R03_THERMO1	0x03	 
#define R04_THERMO2	0x04	 
#define R05_POWER1	0x05	 
#define R06_POWER2	0x06	 
#define R07_GPIO	0x07	 
#define R08_IRQ1	0x08	 
#define R09_IRQ2	0x09	 
#define R0A_IRQ3	0x0a	 
#define R0B_IRQ4	0x0b	 
#define R0C_AGC11	0x0c	 
#define R0D_AGC12	0x0d	 
#define R0E_AGC13	0x0e	 
#define R0F_AGC14	0x0f	 
#define R10_LT1		0x10	 
#define R11_LT2		0x11	 
#define R12_AGC21	0x12	 
#define R13_AGC22	0x13	 
#define R14_AGC23	0x14	 
#define R15_AGC24	0x15	 
#define R16_AGC25	0x16	 
#define R17_AGC31	0x17	 
#define R18_AGC32	0x18	 
#define R19_AGC33	0x19	 
#define R1A_AGCK	0x1a
#define R1B_GAIN1	0x1b
#define R1C_GAIN2	0x1c
#define R1D_GAIN3	0x1d
#define R1E_WI_FI	0x1e	 
#define R1F_RF_BPF	0x1f	 
#define R20_IR_MIX	0x20	 
#define R21_IF_AGC	0x21
#define R22_IF1		0x22	 
#define R23_IF2		0x23	 
#define R24_IF3		0x24	 
#define R25_REF		0x25	 
#define R26_IF		0x26	 
#define R27_RF1		0x27	 
#define R28_RF2		0x28	 
#define R29_RF3		0x29	 
#define R2A_MSM1	0x2a
#define R2B_MSM2	0x2b
#define R2C_PS1		0x2c	 
#define R2D_PS2		0x2d	 
#define R2E_PS3		0x2e	 
#define R2F_RSSI1	0x2f
#define R30_RSSI2	0x30
#define R31_IRQ_CTRL	0x31
#define R32_DUMMY	0x32
#define R33_TEST	0x33
#define R34_MD1		0x34
#define R35_SD1		0x35
#define R36_SD2		0x36
#define R37_SD3		0x37
#define R38_SD4		0x38
#define R39_SD5		0x39
#define R3A_SD_TEST	0x3a
#define R3B_REGU	0x3b
#define R3C_RCCAL1	0x3c
#define R3D_RCCAL2	0x3d
#define R3E_IRCAL1	0x3e
#define R3F_IRCAL2	0x3f
#define R40_IRCAL3	0x40
#define R41_IRCAL4	0x41
#define R42_IRCAL5	0x42
#define R43_PD1		0x43	 
#define R44_PD2		0x44	 
#define R45_PD		0x45	 
#define R46_CPUMP	0x46	 
#define R47_LNAPOL	0x47	 
#define R48_SMOOTH1	0x48	 
#define R49_SMOOTH2	0x49	 
#define R4A_SMOOTH3	0x4a	 
#define R4B_XTALOSC1	0x4b
#define R4C_XTALOSC2	0x4c
#define R4D_XTALFLX1	0x4d
#define R4E_XTALFLX2	0x4e
#define R4F_XTALFLX3	0x4f
#define R50_XTALFLX4	0x50
#define R51_XTALFLX5	0x51
#define R52_IRLOOP0	0x52
#define R53_IRLOOP1	0x53
#define R54_IRLOOP2	0x54
#define R55_IRLOOP3	0x55
#define R56_IRLOOP4	0x56
#define R57_PLL_LOG	0x57
#define R58_AGC2_UP1	0x58
#define R59_AGC2_UP2	0x59
#define R5A_H3H5	0x5a
#define R5B_AGC_AUTO	0x5b
#define R5C_AGC_DEBUG	0x5c

#define TDA18250_NUM_REGS 93

#define TDA18250_POWER_STANDBY 0
#define TDA18250_POWER_NORMAL 1

#define TDA18250_IRQ_CAL     0x81
#define TDA18250_IRQ_HW_INIT 0x82
#define TDA18250_IRQ_TUNE    0x88

struct tda18250_dev {
	struct mutex i2c_mutex;
	struct dvb_frontend *fe;
	struct i2c_adapter *i2c;
	struct regmap *regmap;
	u8 xtal_freq;
	 
	u16 if_dvbt_6;
	u16 if_dvbt_7;
	u16 if_dvbt_8;
	u16 if_dvbc_6;
	u16 if_dvbc_8;
	u16 if_atsc;
	u16 if_frequency;
	bool slave;
	bool loopthrough;
	bool warm;
	u8 regs[TDA18250_NUM_REGS];
};

#endif
