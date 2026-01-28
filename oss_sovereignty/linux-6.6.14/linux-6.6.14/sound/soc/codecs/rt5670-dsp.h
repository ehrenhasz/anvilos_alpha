#ifndef __RT5670_DSP_H__
#define __RT5670_DSP_H__
#define RT5670_DSP_CTRL1		0xe0
#define RT5670_DSP_CTRL2		0xe1
#define RT5670_DSP_CTRL3		0xe2
#define RT5670_DSP_CTRL4		0xe3
#define RT5670_DSP_CTRL5		0xe4
#define RT5670_DSP_CMD_MASK		(0xff << 8)
#define RT5670_DSP_CMD_PE		(0x0d << 8)	 
#define RT5670_DSP_CMD_MW		(0x3b << 8)	 
#define RT5670_DSP_CMD_MR		(0x37 << 8)	 
#define RT5670_DSP_CMD_RR		(0x60 << 8)	 
#define RT5670_DSP_CMD_RW		(0x68 << 8)	 
#define RT5670_DSP_REG_DATHI		(0x26 << 8)	 
#define RT5670_DSP_REG_DATLO		(0x25 << 8)	 
#define RT5670_DSP_CLK_MASK		(0x3 << 6)
#define RT5670_DSP_CLK_SFT		6
#define RT5670_DSP_CLK_768K		(0x0 << 6)
#define RT5670_DSP_CLK_384K		(0x1 << 6)
#define RT5670_DSP_CLK_192K		(0x2 << 6)
#define RT5670_DSP_CLK_96K		(0x3 << 6)
#define RT5670_DSP_BUSY_MASK		(0x1 << 5)
#define RT5670_DSP_RW_MASK		(0x1 << 4)
#define RT5670_DSP_DL_MASK		(0x3 << 2)
#define RT5670_DSP_DL_0			(0x0 << 2)
#define RT5670_DSP_DL_1			(0x1 << 2)
#define RT5670_DSP_DL_2			(0x2 << 2)
#define RT5670_DSP_DL_3			(0x3 << 2)
#define RT5670_DSP_I2C_AL_16		(0x1 << 1)
#define RT5670_DSP_CMD_EN		(0x1)
struct rt5670_dsp_param {
	u16 cmd_fmt;
	u16 addr;
	u16 data;
	u8 cmd;
};
#endif  
