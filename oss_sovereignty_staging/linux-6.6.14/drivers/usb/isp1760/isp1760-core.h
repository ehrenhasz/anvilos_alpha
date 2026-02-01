 
 

#ifndef _ISP1760_CORE_H_
#define _ISP1760_CORE_H_

#include <linux/ioport.h>
#include <linux/regmap.h>

#include "isp1760-hcd.h"
#include "isp1760-udc.h"

struct device;
struct gpio_desc;

 
#define ISP1760_FLAG_BUS_WIDTH_16	0x00000002  
#define ISP1760_FLAG_PERIPHERAL_EN	0x00000004  
#define ISP1760_FLAG_ANALOG_OC		0x00000008  
#define ISP1760_FLAG_DACK_POL_HIGH	0x00000010  
#define ISP1760_FLAG_DREQ_POL_HIGH	0x00000020  
#define ISP1760_FLAG_ISP1761		0x00000040  
#define ISP1760_FLAG_INTR_POL_HIGH	0x00000080  
#define ISP1760_FLAG_INTR_EDGE_TRIG	0x00000100  
#define ISP1760_FLAG_ISP1763		0x00000200  
#define ISP1760_FLAG_BUS_WIDTH_8	0x00000400  

struct isp1760_device {
	struct device *dev;

	unsigned int devflags;
	struct gpio_desc *rst_gpio;

	struct isp1760_hcd hcd;
	struct isp1760_udc udc;
};

int isp1760_register(struct resource *mem, int irq, unsigned long irqflags,
		     struct device *dev, unsigned int devflags);
void isp1760_unregister(struct device *dev);

void isp1760_set_pullup(struct isp1760_device *isp, bool enable);

static inline u32 isp1760_field_read(struct regmap_field **fields, u32 field)
{
	unsigned int val;

	regmap_field_read(fields[field], &val);

	return val;
}

static inline void isp1760_field_write(struct regmap_field **fields, u32 field,
				       u32 val)
{
	regmap_field_write(fields[field], val);
}

static inline void isp1760_field_set(struct regmap_field **fields, u32 field)
{
	isp1760_field_write(fields, field, 0xFFFFFFFF);
}

static inline void isp1760_field_clear(struct regmap_field **fields, u32 field)
{
	isp1760_field_write(fields, field, 0);
}

static inline u32 isp1760_reg_read(struct regmap *regs, u32 reg)
{
	unsigned int val;

	regmap_read(regs, reg, &val);

	return val;
}

static inline void isp1760_reg_write(struct regmap *regs, u32 reg, u32 val)
{
	regmap_write(regs, reg, val);
}
#endif
