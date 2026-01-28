
#include <linux/atomic.h>
#include <linux/clk.h>
#include <linux/delay.h>
#include <linux/device.h>
#include <linux/i2c.h>
#include <linux/i2c-smbus.h>
#include <linux/io.h>
#include <linux/kernel.h>


#define SW_TWSI_V		BIT_ULL(63)	
#define SW_TWSI_EIA		BIT_ULL(61)	
#define SW_TWSI_R		BIT_ULL(56)	
#define SW_TWSI_SOVR		BIT_ULL(55)	
#define SW_TWSI_SIZE_SHIFT	52
#define SW_TWSI_ADDR_SHIFT	40
#define SW_TWSI_IA_SHIFT	32		


#define SW_TWSI_OP_SHIFT	57
#define SW_TWSI_OP_7		(0ULL << SW_TWSI_OP_SHIFT)
#define SW_TWSI_OP_7_IA		(1ULL << SW_TWSI_OP_SHIFT)
#define SW_TWSI_OP_10		(2ULL << SW_TWSI_OP_SHIFT)
#define SW_TWSI_OP_10_IA	(3ULL << SW_TWSI_OP_SHIFT)
#define SW_TWSI_OP_TWSI_CLK	(4ULL << SW_TWSI_OP_SHIFT)
#define SW_TWSI_OP_EOP		(6ULL << SW_TWSI_OP_SHIFT) 


#define SW_TWSI_EOP_SHIFT	32
#define SW_TWSI_EOP_TWSI_DATA	(SW_TWSI_OP_EOP | 1ULL << SW_TWSI_EOP_SHIFT)
#define SW_TWSI_EOP_TWSI_CTL	(SW_TWSI_OP_EOP | 2ULL << SW_TWSI_EOP_SHIFT)
#define SW_TWSI_EOP_TWSI_CLKCTL	(SW_TWSI_OP_EOP | 3ULL << SW_TWSI_EOP_SHIFT)
#define SW_TWSI_EOP_TWSI_STAT	(SW_TWSI_OP_EOP | 3ULL << SW_TWSI_EOP_SHIFT)
#define SW_TWSI_EOP_TWSI_RST	(SW_TWSI_OP_EOP | 7ULL << SW_TWSI_EOP_SHIFT)


#define TWSI_CTL_CE		0x80	
#define TWSI_CTL_ENAB		0x40	
#define TWSI_CTL_STA		0x20	
#define TWSI_CTL_STP		0x10	
#define TWSI_CTL_IFLG		0x08	
#define TWSI_CTL_AAK		0x04	


#define STAT_BUS_ERROR		0x00
#define STAT_START		0x08
#define STAT_REP_START		0x10
#define STAT_TXADDR_ACK		0x18
#define STAT_TXADDR_NAK		0x20
#define STAT_TXDATA_ACK		0x28
#define STAT_TXDATA_NAK		0x30
#define STAT_LOST_ARB_38	0x38
#define STAT_RXADDR_ACK		0x40
#define STAT_RXADDR_NAK		0x48
#define STAT_RXDATA_ACK		0x50
#define STAT_RXDATA_NAK		0x58
#define STAT_SLAVE_60		0x60
#define STAT_LOST_ARB_68	0x68
#define STAT_SLAVE_70		0x70
#define STAT_LOST_ARB_78	0x78
#define STAT_SLAVE_80		0x80
#define STAT_SLAVE_88		0x88
#define STAT_GENDATA_ACK	0x90
#define STAT_GENDATA_NAK	0x98
#define STAT_SLAVE_A0		0xA0
#define STAT_SLAVE_A8		0xA8
#define STAT_LOST_ARB_B0	0xB0
#define STAT_SLAVE_LOST		0xB8
#define STAT_SLAVE_NAK		0xC0
#define STAT_SLAVE_ACK		0xC8
#define STAT_AD2W_ACK		0xD0
#define STAT_AD2W_NAK		0xD8
#define STAT_IDLE		0xF8


#define TWSI_INT_ST_INT		BIT_ULL(0)
#define TWSI_INT_TS_INT		BIT_ULL(1)
#define TWSI_INT_CORE_INT	BIT_ULL(2)
#define TWSI_INT_ST_EN		BIT_ULL(4)
#define TWSI_INT_TS_EN		BIT_ULL(5)
#define TWSI_INT_CORE_EN	BIT_ULL(6)
#define TWSI_INT_SDA_OVR	BIT_ULL(8)
#define TWSI_INT_SCL_OVR	BIT_ULL(9)
#define TWSI_INT_SDA		BIT_ULL(10)
#define TWSI_INT_SCL		BIT_ULL(11)

#define I2C_OCTEON_EVENT_WAIT 80 


struct octeon_i2c_reg_offset {
	unsigned int sw_twsi;
	unsigned int twsi_int;
	unsigned int sw_twsi_ext;
};

#define SW_TWSI(x)	(x->roff.sw_twsi)
#define TWSI_INT(x)	(x->roff.twsi_int)
#define SW_TWSI_EXT(x)	(x->roff.sw_twsi_ext)

struct octeon_i2c {
	wait_queue_head_t queue;
	struct i2c_adapter adap;
	struct octeon_i2c_reg_offset roff;
	struct clk *clk;
	int irq;
	int hlc_irq;		
	u32 twsi_freq;
	int sys_freq;
	void __iomem *twsi_base;
	struct device *dev;
	bool hlc_enabled;
	bool broken_irq_mode;
	bool broken_irq_check;
	void (*int_enable)(struct octeon_i2c *);
	void (*int_disable)(struct octeon_i2c *);
	void (*hlc_int_enable)(struct octeon_i2c *);
	void (*hlc_int_disable)(struct octeon_i2c *);
	atomic_t int_enable_cnt;
	atomic_t hlc_int_enable_cnt;
	struct i2c_smbus_alert_setup alert_data;
	struct i2c_client *ara;
};

static inline void octeon_i2c_writeq_flush(u64 val, void __iomem *addr)
{
	__raw_writeq(val, addr);
	__raw_readq(addr);	
}


static inline void octeon_i2c_reg_write(struct octeon_i2c *i2c, u64 eop_reg, u8 data)
{
	int tries = 1000;
	u64 tmp;

	__raw_writeq(SW_TWSI_V | eop_reg | data, i2c->twsi_base + SW_TWSI(i2c));
	do {
		tmp = __raw_readq(i2c->twsi_base + SW_TWSI(i2c));
		if (--tries < 0)
			return;
	} while ((tmp & SW_TWSI_V) != 0);
}

#define octeon_i2c_ctl_write(i2c, val)					\
	octeon_i2c_reg_write(i2c, SW_TWSI_EOP_TWSI_CTL, val)
#define octeon_i2c_data_write(i2c, val)					\
	octeon_i2c_reg_write(i2c, SW_TWSI_EOP_TWSI_DATA, val)


static inline int octeon_i2c_reg_read(struct octeon_i2c *i2c, u64 eop_reg,
				      int *error)
{
	int tries = 1000;
	u64 tmp;

	__raw_writeq(SW_TWSI_V | eop_reg | SW_TWSI_R, i2c->twsi_base + SW_TWSI(i2c));
	do {
		tmp = __raw_readq(i2c->twsi_base + SW_TWSI(i2c));
		if (--tries < 0) {
			
			if (error)
				*error = -EIO;
			return 0;
		}
	} while ((tmp & SW_TWSI_V) != 0);

	return tmp & 0xFF;
}

#define octeon_i2c_ctl_read(i2c)					\
	octeon_i2c_reg_read(i2c, SW_TWSI_EOP_TWSI_CTL, NULL)
#define octeon_i2c_data_read(i2c, error)				\
	octeon_i2c_reg_read(i2c, SW_TWSI_EOP_TWSI_DATA, error)
#define octeon_i2c_stat_read(i2c)					\
	octeon_i2c_reg_read(i2c, SW_TWSI_EOP_TWSI_STAT, NULL)


static inline u64 octeon_i2c_read_int(struct octeon_i2c *i2c)
{
	return __raw_readq(i2c->twsi_base + TWSI_INT(i2c));
}


static inline void octeon_i2c_write_int(struct octeon_i2c *i2c, u64 data)
{
	octeon_i2c_writeq_flush(data, i2c->twsi_base + TWSI_INT(i2c));
}


irqreturn_t octeon_i2c_isr(int irq, void *dev_id);
int octeon_i2c_xfer(struct i2c_adapter *adap, struct i2c_msg *msgs, int num);
int octeon_i2c_init_lowlevel(struct octeon_i2c *i2c);
void octeon_i2c_set_clock(struct octeon_i2c *i2c);
extern struct i2c_bus_recovery_info octeon_i2c_recovery_info;
