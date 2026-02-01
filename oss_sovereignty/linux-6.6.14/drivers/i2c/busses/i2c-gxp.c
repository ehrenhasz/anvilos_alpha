
 

#include <linux/err.h>
#include <linux/io.h>
#include <linux/i2c.h>
#include <linux/mod_devicetable.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/regmap.h>
#include <linux/mfd/syscon.h>

#define GXP_MAX_I2C_ENGINE 10
static const char * const gxp_i2c_name[] = {
	"gxp-i2c0", "gxp-i2c1", "gxp-i2c2", "gxp-i2c3",
	"gxp-i2c4", "gxp-i2c5", "gxp-i2c6", "gxp-i2c7",
	"gxp-i2c8", "gxp-i2c9" };

 
#define GXP_I2CINTSTAT		0x00
#define GXP_I2CINTEN		0x04

 
#define GXP_I2CSTAT		0x00
#define MASK_STOP_EVENT		0x20
#define MASK_ACK		0x08
#define MASK_RW			0x04
#define GXP_I2CEVTERR		0x01
#define MASK_SLAVE_CMD_EVENT	0x01
#define MASK_SLAVE_DATA_EVENT	0x02
#define MASK_MASTER_EVENT	0x10
#define GXP_I2CSNPDAT		0x02
#define GXP_I2CMCMD		0x04
#define GXP_I2CSCMD		0x06
#define GXP_I2CSNPAA		0x09
#define GXP_I2CADVFEAT		0x0A
#define GXP_I2COWNADR		0x0B
#define GXP_I2CFREQDIV		0x0C
#define GXP_I2CFLTFAIR		0x0D
#define GXP_I2CTMOEDG		0x0E
#define GXP_I2CCYCTIM		0x0F

 
#define SNOOP_EVT_CLR		0x80
#define SLAVE_EVT_CLR		0x40
#define	SNOOP_EVT_MASK		0x20
#define SLAVE_EVT_MASK		0x10
#define SLAVE_ACK_ENAB		0x08
#define SLAVE_EVT_STALL		0x01

 
#define MASTER_EVT_CLR		0x80
#define MASTER_ACK_ENAB		0x08
#define RW_CMD			0x04
#define STOP_CMD		0x02
#define START_CMD		0x01

 
#define GXP_DATA_EDGE_RST_CTRL	0x0a  

 
#define FILTER_CNT		0x30
#define FAIRNESS_CNT		0x02

enum {
	GXP_I2C_IDLE = 0,
	GXP_I2C_ADDR_PHASE,
	GXP_I2C_RDATA_PHASE,
	GXP_I2C_WDATA_PHASE,
	GXP_I2C_ADDR_NACK,
	GXP_I2C_DATA_NACK,
	GXP_I2C_ERROR,
	GXP_I2C_COMP
};

struct gxp_i2c_drvdata {
	struct device *dev;
	void __iomem *base;
	struct i2c_timings t;
	u32 engine;
	int irq;
	struct completion completion;
	struct i2c_adapter adapter;
	struct i2c_msg *curr_msg;
	int msgs_remaining;
	int msgs_num;
	u8 *buf;
	size_t buf_remaining;
	unsigned char state;
	struct i2c_client *slave;
	unsigned char stopped;
};

static struct regmap *i2cg_map;

static void gxp_i2c_start(struct gxp_i2c_drvdata *drvdata)
{
	u16 value;

	drvdata->buf = drvdata->curr_msg->buf;
	drvdata->buf_remaining = drvdata->curr_msg->len;

	 
	value = drvdata->curr_msg->addr << 9;

	 
	value |= drvdata->curr_msg->flags & I2C_M_RD ? RW_CMD | START_CMD : START_CMD;

	drvdata->state = GXP_I2C_ADDR_PHASE;
	writew(value, drvdata->base + GXP_I2CMCMD);
}

static int gxp_i2c_master_xfer(struct i2c_adapter *adapter,
			       struct i2c_msg *msgs, int num)
{
	int ret;
	struct gxp_i2c_drvdata *drvdata = i2c_get_adapdata(adapter);
	unsigned long time_left;

	drvdata->msgs_remaining = num;
	drvdata->curr_msg = msgs;
	drvdata->msgs_num = num;
	reinit_completion(&drvdata->completion);

	gxp_i2c_start(drvdata);

	time_left = wait_for_completion_timeout(&drvdata->completion,
						adapter->timeout);
	ret = num - drvdata->msgs_remaining;
	if (time_left == 0)
		return -ETIMEDOUT;

	if (drvdata->state == GXP_I2C_ADDR_NACK)
		return -ENXIO;

	if (drvdata->state == GXP_I2C_DATA_NACK)
		return -EIO;

	return ret;
}

static u32 gxp_i2c_func(struct i2c_adapter *adap)
{
	if (IS_ENABLED(CONFIG_I2C_SLAVE))
		return I2C_FUNC_I2C | I2C_FUNC_SMBUS_EMUL | I2C_FUNC_SLAVE;

	return I2C_FUNC_I2C | I2C_FUNC_SMBUS_EMUL;
}

#if IS_ENABLED(CONFIG_I2C_SLAVE)
static int gxp_i2c_reg_slave(struct i2c_client *slave)
{
	struct gxp_i2c_drvdata *drvdata = i2c_get_adapdata(slave->adapter);

	if (drvdata->slave)
		return -EBUSY;

	if (slave->flags & I2C_CLIENT_TEN)
		return -EAFNOSUPPORT;

	drvdata->slave = slave;

	writeb(slave->addr << 1, drvdata->base + GXP_I2COWNADR);
	writeb(SLAVE_EVT_CLR | SNOOP_EVT_MASK | SLAVE_ACK_ENAB |
	       SLAVE_EVT_STALL, drvdata->base + GXP_I2CSCMD);

	return 0;
}

static int gxp_i2c_unreg_slave(struct i2c_client *slave)
{
	struct gxp_i2c_drvdata *drvdata = i2c_get_adapdata(slave->adapter);

	WARN_ON(!drvdata->slave);

	writeb(0x00, drvdata->base + GXP_I2COWNADR);
	writeb(SNOOP_EVT_CLR | SLAVE_EVT_CLR | SNOOP_EVT_MASK |
	       SLAVE_EVT_MASK, drvdata->base + GXP_I2CSCMD);

	drvdata->slave = NULL;

	return 0;
}
#endif

static const struct i2c_algorithm gxp_i2c_algo = {
	.master_xfer   = gxp_i2c_master_xfer,
	.functionality = gxp_i2c_func,
#if IS_ENABLED(CONFIG_I2C_SLAVE)
	.reg_slave     = gxp_i2c_reg_slave,
	.unreg_slave   = gxp_i2c_unreg_slave,
#endif
};

static void gxp_i2c_stop(struct gxp_i2c_drvdata *drvdata)
{
	 
	writeb(MASTER_EVT_CLR | STOP_CMD, drvdata->base + GXP_I2CMCMD);

	complete(&drvdata->completion);
}

static void gxp_i2c_restart(struct gxp_i2c_drvdata *drvdata)
{
	u16 value;

	drvdata->buf = drvdata->curr_msg->buf;
	drvdata->buf_remaining = drvdata->curr_msg->len;

	value = drvdata->curr_msg->addr << 9;

	if (drvdata->curr_msg->flags & I2C_M_RD) {
		 
		value |= MASTER_EVT_CLR | RW_CMD | START_CMD;
	} else {
		 
		value |= MASTER_EVT_CLR | START_CMD;
	}

	drvdata->state = GXP_I2C_ADDR_PHASE;

	writew(value, drvdata->base + GXP_I2CMCMD);
}

static void gxp_i2c_chk_addr_ack(struct gxp_i2c_drvdata *drvdata)
{
	u16 value;

	value = readb(drvdata->base + GXP_I2CSTAT);
	if (!(value & MASK_ACK)) {
		 
		drvdata->state = GXP_I2C_ADDR_NACK;
		gxp_i2c_stop(drvdata);
		return;
	}

	if (drvdata->curr_msg->flags & I2C_M_RD) {
		 
		if (drvdata->buf_remaining == 0) {
			 
			drvdata->msgs_remaining--;
			drvdata->state = GXP_I2C_COMP;
			gxp_i2c_stop(drvdata);
			return;
		}
		drvdata->state = GXP_I2C_RDATA_PHASE;

		if (drvdata->buf_remaining == 1) {
			 
			writeb(MASTER_EVT_CLR | RW_CMD,
			       drvdata->base + GXP_I2CMCMD);
		} else {
			 
			writeb(MASTER_EVT_CLR | MASTER_ACK_ENAB |
			       RW_CMD, drvdata->base + GXP_I2CMCMD);
		}
	} else {
		 
		if (drvdata->buf_remaining == 0) {
			 
			drvdata->msgs_remaining--;
			drvdata->state = GXP_I2C_COMP;
			gxp_i2c_stop(drvdata);
			return;
		}
		value = *drvdata->buf;
		value = value << 8;
		 
		value |= MASTER_EVT_CLR;
		drvdata->buf++;
		drvdata->buf_remaining--;
		drvdata->state = GXP_I2C_WDATA_PHASE;
		writew(value, drvdata->base + GXP_I2CMCMD);
	}
}

static void gxp_i2c_ack_data(struct gxp_i2c_drvdata *drvdata)
{
	u8 value;

	 
	value = readb(drvdata->base + GXP_I2CSNPDAT);
	*drvdata->buf = value;
	drvdata->buf++;
	drvdata->buf_remaining--;

	if (drvdata->buf_remaining == 0) {
		 
		drvdata->msgs_remaining--;

		if (drvdata->msgs_remaining == 0) {
			 
			drvdata->state = GXP_I2C_COMP;
			gxp_i2c_stop(drvdata);
			return;
		}
		 
		drvdata->curr_msg++;
		gxp_i2c_restart(drvdata);
		return;
	}

	 
	drvdata->state = GXP_I2C_RDATA_PHASE;
	if (drvdata->buf_remaining == 1) {
		 
		writeb(MASTER_EVT_CLR | RW_CMD,
		       drvdata->base + GXP_I2CMCMD);
	} else {
		 
		writeb(MASTER_EVT_CLR | MASTER_ACK_ENAB |
		       RW_CMD, drvdata->base + GXP_I2CMCMD);
	}
}

static void gxp_i2c_chk_data_ack(struct gxp_i2c_drvdata *drvdata)
{
	u16 value;

	value = readb(drvdata->base + GXP_I2CSTAT);
	if (!(value & MASK_ACK)) {
		 
		drvdata->state = GXP_I2C_DATA_NACK;
		gxp_i2c_stop(drvdata);
		return;
	}

	 
	if (drvdata->buf_remaining == 0) {
		 
		drvdata->msgs_remaining--;

		if (drvdata->msgs_remaining == 0) {
			 
			drvdata->state = GXP_I2C_COMP;
			gxp_i2c_stop(drvdata);
			return;
		}
		 
		drvdata->curr_msg++;
		gxp_i2c_restart(drvdata);
		return;
	}

	 
	value = *drvdata->buf;
	value = value << 8;

	 
	value |= MASTER_EVT_CLR;
	drvdata->buf++;
	drvdata->buf_remaining--;
	drvdata->state = GXP_I2C_WDATA_PHASE;
	writew(value, drvdata->base + GXP_I2CMCMD);
}

static bool gxp_i2c_slave_irq_handler(struct gxp_i2c_drvdata *drvdata)
{
	u8 value;
	u8 buf;
	int ret;

	value = readb(drvdata->base + GXP_I2CEVTERR);

	 
	if (value & MASK_SLAVE_CMD_EVENT) {
		value = readb(drvdata->base + GXP_I2CSTAT);
		 
		if (value & MASK_STOP_EVENT) {
			if (drvdata->stopped == 0)
				i2c_slave_event(drvdata->slave, I2C_SLAVE_STOP, &buf);
			writeb(SLAVE_EVT_CLR | SNOOP_EVT_MASK |
			       SLAVE_ACK_ENAB | SLAVE_EVT_STALL, drvdata->base + GXP_I2CSCMD);
			drvdata->stopped = 1;
		} else {
			 
			drvdata->stopped = 0;
			if (value & MASK_RW) {
				i2c_slave_event(drvdata->slave,
						I2C_SLAVE_READ_REQUESTED, &buf);
				value = buf << 8 | (SLAVE_EVT_CLR | SNOOP_EVT_MASK |
						    SLAVE_EVT_STALL);
				writew(value, drvdata->base + GXP_I2CSCMD);
			} else {
				 
				ret = i2c_slave_event(drvdata->slave,
						      I2C_SLAVE_WRITE_REQUESTED, &buf);
				if (!ret) {
					 
					writeb(SLAVE_EVT_CLR | SNOOP_EVT_MASK |
					       SLAVE_ACK_ENAB | SLAVE_EVT_STALL,
					       drvdata->base + GXP_I2CSCMD);
				} else {
					 
					writeb(SLAVE_EVT_CLR | SNOOP_EVT_MASK |
					       SLAVE_EVT_STALL, drvdata->base + GXP_I2CSCMD);
				}
			}
		}
	} else if (value & MASK_SLAVE_DATA_EVENT) {
		value = readb(drvdata->base + GXP_I2CSTAT);
		 
		if (value & MASK_RW) {
			 
			if (value & MASK_ACK) {
				i2c_slave_event(drvdata->slave,
						I2C_SLAVE_READ_PROCESSED, &buf);
				value = buf << 8 | (SLAVE_EVT_CLR | SNOOP_EVT_MASK |
						    SLAVE_EVT_STALL);
				writew(value, drvdata->base + GXP_I2CSCMD);
			} else {
				 
				writew(SLAVE_EVT_CLR | SNOOP_EVT_MASK |
				       SLAVE_ACK_ENAB | SLAVE_EVT_STALL,
				       drvdata->base + GXP_I2CSCMD);
			}
		} else {
			 
			value = readb(drvdata->base + GXP_I2CSNPDAT);
			buf = (uint8_t)value;
			ret = i2c_slave_event(drvdata->slave,
					      I2C_SLAVE_WRITE_RECEIVED, &buf);
			if (!ret) {
				 
				writeb(SLAVE_EVT_CLR | SNOOP_EVT_MASK |
				       SLAVE_ACK_ENAB | SLAVE_EVT_STALL,
				       drvdata->base + GXP_I2CSCMD);
			} else {
				 
				writeb(SLAVE_EVT_CLR | SNOOP_EVT_MASK |
				       SLAVE_EVT_STALL, drvdata->base + GXP_I2CSCMD);
			}
		}
	} else {
		return false;
	}

	return true;
}

static irqreturn_t gxp_i2c_irq_handler(int irq, void *_drvdata)
{
	struct gxp_i2c_drvdata *drvdata = (struct gxp_i2c_drvdata *)_drvdata;
	u32 value;

	 
	regmap_read(i2cg_map, GXP_I2CINTSTAT, &value);
	if (!(value & BIT(drvdata->engine)))
		return IRQ_NONE;

	value = readb(drvdata->base + GXP_I2CEVTERR);

	 
	if (value & ~(MASK_MASTER_EVENT | MASK_SLAVE_CMD_EVENT |
				MASK_SLAVE_DATA_EVENT)) {
		 
		writeb(0x00, drvdata->base + GXP_I2CEVTERR);
		drvdata->state = GXP_I2C_ERROR;
		gxp_i2c_stop(drvdata);
		return IRQ_HANDLED;
	}

	if (IS_ENABLED(CONFIG_I2C_SLAVE)) {
		 
		if (value & (MASK_SLAVE_CMD_EVENT | MASK_SLAVE_DATA_EVENT)) {
			if (gxp_i2c_slave_irq_handler(drvdata))
				return IRQ_HANDLED;
			return IRQ_NONE;
		}
	}

	 
	switch (drvdata->state) {
	case GXP_I2C_ADDR_PHASE:
		gxp_i2c_chk_addr_ack(drvdata);
		break;

	case GXP_I2C_RDATA_PHASE:
		gxp_i2c_ack_data(drvdata);
		break;

	case GXP_I2C_WDATA_PHASE:
		gxp_i2c_chk_data_ack(drvdata);
		break;
	}

	return IRQ_HANDLED;
}

static void gxp_i2c_init(struct gxp_i2c_drvdata *drvdata)
{
	drvdata->state = GXP_I2C_IDLE;
	writeb(2000000 / drvdata->t.bus_freq_hz,
	       drvdata->base + GXP_I2CFREQDIV);
	writeb(FILTER_CNT | FAIRNESS_CNT,
	       drvdata->base + GXP_I2CFLTFAIR);
	writeb(GXP_DATA_EDGE_RST_CTRL, drvdata->base + GXP_I2CTMOEDG);
	writeb(0x00, drvdata->base + GXP_I2CCYCTIM);
	writeb(0x00, drvdata->base + GXP_I2CSNPAA);
	writeb(0x00, drvdata->base + GXP_I2CADVFEAT);
	writeb(SNOOP_EVT_CLR | SLAVE_EVT_CLR | SNOOP_EVT_MASK |
	       SLAVE_EVT_MASK, drvdata->base + GXP_I2CSCMD);
	writeb(MASTER_EVT_CLR, drvdata->base + GXP_I2CMCMD);
	writeb(0x00, drvdata->base + GXP_I2CEVTERR);
	writeb(0x00, drvdata->base + GXP_I2COWNADR);
}

static int gxp_i2c_probe(struct platform_device *pdev)
{
	struct gxp_i2c_drvdata *drvdata;
	int rc;
	struct i2c_adapter *adapter;

	if (!i2cg_map) {
		i2cg_map = syscon_regmap_lookup_by_phandle(pdev->dev.of_node,
							   "hpe,sysreg");
		if (IS_ERR(i2cg_map)) {
			return dev_err_probe(&pdev->dev, PTR_ERR(i2cg_map),
					     "failed to map i2cg_handle\n");
		}

		 
		regmap_update_bits(i2cg_map, GXP_I2CINTEN, 0x00000FFF, 0);
	}

	drvdata = devm_kzalloc(&pdev->dev, sizeof(*drvdata),
			       GFP_KERNEL);
	if (!drvdata)
		return -ENOMEM;

	platform_set_drvdata(pdev, drvdata);
	drvdata->dev = &pdev->dev;
	init_completion(&drvdata->completion);

	drvdata->base = devm_platform_ioremap_resource(pdev, 0);
	if (IS_ERR(drvdata->base))
		return PTR_ERR(drvdata->base);

	 
	drvdata->engine = ((size_t)drvdata->base & 0xf00) >> 8;

	if (drvdata->engine >= GXP_MAX_I2C_ENGINE) {
		return dev_err_probe(&pdev->dev, -EINVAL, "i2c engine% is unsupported\n",
			drvdata->engine);
	}

	rc = platform_get_irq(pdev, 0);
	if (rc < 0)
		return rc;

	drvdata->irq = rc;
	rc = devm_request_irq(&pdev->dev, drvdata->irq, gxp_i2c_irq_handler,
			      IRQF_SHARED, gxp_i2c_name[drvdata->engine], drvdata);
	if (rc < 0)
		return dev_err_probe(&pdev->dev, rc, "irq request failed\n");

	i2c_parse_fw_timings(&pdev->dev, &drvdata->t, true);

	gxp_i2c_init(drvdata);

	 
	regmap_update_bits(i2cg_map, GXP_I2CINTEN, BIT(drvdata->engine),
			   BIT(drvdata->engine));

	adapter = &drvdata->adapter;
	i2c_set_adapdata(adapter, drvdata);

	adapter->owner = THIS_MODULE;
	strscpy(adapter->name, "HPE GXP I2C adapter", sizeof(adapter->name));
	adapter->algo = &gxp_i2c_algo;
	adapter->dev.parent = &pdev->dev;
	adapter->dev.of_node = pdev->dev.of_node;

	rc = i2c_add_adapter(adapter);
	if (rc)
		return dev_err_probe(&pdev->dev, rc, "i2c add adapter failed\n");

	return 0;
}

static void gxp_i2c_remove(struct platform_device *pdev)
{
	struct gxp_i2c_drvdata *drvdata = platform_get_drvdata(pdev);

	 
	regmap_update_bits(i2cg_map, GXP_I2CINTEN, BIT(drvdata->engine), 0);
	i2c_del_adapter(&drvdata->adapter);
}

static const struct of_device_id gxp_i2c_of_match[] = {
	{ .compatible = "hpe,gxp-i2c" },
	{},
};
MODULE_DEVICE_TABLE(of, gxp_i2c_of_match);

static struct platform_driver gxp_i2c_driver = {
	.probe	= gxp_i2c_probe,
	.remove_new = gxp_i2c_remove,
	.driver = {
		.name = "gxp-i2c",
		.of_match_table = gxp_i2c_of_match,
	},
};
module_platform_driver(gxp_i2c_driver);

MODULE_AUTHOR("Nick Hawkins <nick.hawkins@hpe.com>");
MODULE_DESCRIPTION("HPE GXP I2C bus driver");
MODULE_LICENSE("GPL");
