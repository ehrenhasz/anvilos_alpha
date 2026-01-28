#ifndef _ULTRASOC_SMB_H
#define _ULTRASOC_SMB_H
#include <linux/miscdevice.h>
#include <linux/spinlock.h>
#define SMB_GLB_CFG_REG		0x00
#define SMB_GLB_EN_REG		0x04
#define SMB_GLB_INT_REG		0x08
#define SMB_LB_CFG_LO_REG	0x40
#define SMB_LB_CFG_HI_REG	0x44
#define SMB_LB_INT_CTRL_REG	0x48
#define SMB_LB_INT_STS_REG	0x4c
#define SMB_LB_RD_ADDR_REG	0x5c
#define SMB_LB_WR_ADDR_REG	0x60
#define SMB_LB_PURGE_REG	0x64
#define SMB_GLB_CFG_BURST_LEN_MSK	GENMASK(11, 4)
#define SMB_GLB_CFG_IDLE_PRD_MSK	GENMASK(15, 12)
#define SMB_GLB_CFG_MEM_WR_MSK		GENMASK(21, 16)
#define SMB_GLB_CFG_MEM_RD_MSK		GENMASK(27, 22)
#define SMB_GLB_CFG_DEFAULT	(FIELD_PREP(SMB_GLB_CFG_BURST_LEN_MSK, 0xf) | \
				 FIELD_PREP(SMB_GLB_CFG_IDLE_PRD_MSK, 0xf) | \
				 FIELD_PREP(SMB_GLB_CFG_MEM_WR_MSK, 0x3) | \
				 FIELD_PREP(SMB_GLB_CFG_MEM_RD_MSK, 0x1b))
#define SMB_GLB_EN_HW_ENABLE	BIT(0)
#define SMB_GLB_INT_EN		BIT(0)
#define SMB_GLB_INT_PULSE	BIT(1)  
#define SMB_GLB_INT_ACT_H	BIT(2)  
#define SMB_GLB_INT_CFG		(SMB_GLB_INT_EN | SMB_GLB_INT_PULSE | \
				 SMB_GLB_INT_ACT_H)
#define SMB_LB_CFG_LO_EN		BIT(0)
#define SMB_LB_CFG_LO_SINGLE_END	BIT(1)
#define SMB_LB_CFG_LO_INIT		BIT(8)
#define SMB_LB_CFG_LO_CONT		BIT(11)
#define SMB_LB_CFG_LO_FLOW_MSK		GENMASK(19, 16)
#define SMB_LB_CFG_LO_DEFAULT	(SMB_LB_CFG_LO_EN | SMB_LB_CFG_LO_SINGLE_END | \
				 SMB_LB_CFG_LO_INIT | SMB_LB_CFG_LO_CONT | \
				 FIELD_PREP(SMB_LB_CFG_LO_FLOW_MSK, 0xf))
#define SMB_LB_CFG_HI_RANGE_UP_MSK	GENMASK(15, 8)
#define SMB_LB_CFG_HI_DEFAULT	FIELD_PREP(SMB_LB_CFG_HI_RANGE_UP_MSK, 0xff)
#define SMB_LB_INT_CTRL_EN		BIT(0)
#define SMB_LB_INT_CTRL_BUF_NOTE_MSK	GENMASK(11, 8)
#define SMB_LB_INT_CTRL_CFG	(SMB_LB_INT_CTRL_EN | \
				 FIELD_PREP(SMB_LB_INT_CTRL_BUF_NOTE_MSK, 0xf))
#define SMB_LB_INT_STS_NOT_EMPTY_MSK	BIT(0)
#define SMB_LB_INT_STS_BUF_RESET_MSK	GENMASK(3, 0)
#define SMB_LB_INT_STS_RESET	FIELD_PREP(SMB_LB_INT_STS_BUF_RESET_MSK, 0xf)
#define SMB_LB_PURGE_PURGED	BIT(0)
#define SMB_REG_ADDR_RES	0
#define SMB_BUF_ADDR_RES	1
#define SMB_BUF_ADDR_LO_MSK	GENMASK(31, 0)
struct smb_data_buffer {
	void *buf_base;
	u32 buf_hw_base;
	unsigned long buf_size;
	unsigned long data_size;
	unsigned long buf_rdptr;
};
struct smb_drv_data {
	void __iomem *base;
	struct coresight_device	*csdev;
	struct smb_data_buffer sdb;
	struct miscdevice miscdev;
	spinlock_t spinlock;
	bool reading;
	pid_t pid;
	enum cs_mode mode;
};
#endif
