
 

#include "chip.h"
#include "smi.h"

 

static int mv88e6xxx_smi_direct_read(struct mv88e6xxx_chip *chip,
				     int dev, int reg, u16 *data)
{
	int ret;

	ret = mdiobus_read_nested(chip->bus, dev, reg);
	if (ret < 0)
		return ret;

	*data = ret & 0xffff;

	return 0;
}

static int mv88e6xxx_smi_direct_write(struct mv88e6xxx_chip *chip,
				      int dev, int reg, u16 data)
{
	int ret;

	ret = mdiobus_write_nested(chip->bus, dev, reg, data);
	if (ret < 0)
		return ret;

	return 0;
}

static int mv88e6xxx_smi_direct_wait(struct mv88e6xxx_chip *chip,
				     int dev, int reg, int bit, int val)
{
	const unsigned long timeout = jiffies + msecs_to_jiffies(50);
	u16 data;
	int err;
	int i;

	 
	for (i = 0; time_before(jiffies, timeout) || (i < 2); i++) {
		err = mv88e6xxx_smi_direct_read(chip, dev, reg, &data);
		if (err)
			return err;

		if (!!(data & BIT(bit)) == !!val)
			return 0;

		if (i < 2)
			cpu_relax();
		else
			usleep_range(1000, 2000);
	}

	return -ETIMEDOUT;
}

static const struct mv88e6xxx_bus_ops mv88e6xxx_smi_direct_ops = {
	.read = mv88e6xxx_smi_direct_read,
	.write = mv88e6xxx_smi_direct_write,
};

static int mv88e6xxx_smi_dual_direct_read(struct mv88e6xxx_chip *chip,
					  int dev, int reg, u16 *data)
{
	return mv88e6xxx_smi_direct_read(chip, chip->sw_addr + dev, reg, data);
}

static int mv88e6xxx_smi_dual_direct_write(struct mv88e6xxx_chip *chip,
					   int dev, int reg, u16 data)
{
	return mv88e6xxx_smi_direct_write(chip, chip->sw_addr + dev, reg, data);
}

static const struct mv88e6xxx_bus_ops mv88e6xxx_smi_dual_direct_ops = {
	.read = mv88e6xxx_smi_dual_direct_read,
	.write = mv88e6xxx_smi_dual_direct_write,
};

 

static int mv88e6xxx_smi_indirect_read(struct mv88e6xxx_chip *chip,
				       int dev, int reg, u16 *data)
{
	int err;

	err = mv88e6xxx_smi_direct_write(chip, chip->sw_addr,
					 MV88E6XXX_SMI_CMD,
					 MV88E6XXX_SMI_CMD_BUSY |
					 MV88E6XXX_SMI_CMD_MODE_22 |
					 MV88E6XXX_SMI_CMD_OP_22_READ |
					 (dev << 5) | reg);
	if (err)
		return err;

	err = mv88e6xxx_smi_direct_wait(chip, chip->sw_addr,
					MV88E6XXX_SMI_CMD, 15, 0);
	if (err)
		return err;

	return mv88e6xxx_smi_direct_read(chip, chip->sw_addr,
					 MV88E6XXX_SMI_DATA, data);
}

static int mv88e6xxx_smi_indirect_write(struct mv88e6xxx_chip *chip,
					int dev, int reg, u16 data)
{
	int err;

	err = mv88e6xxx_smi_direct_write(chip, chip->sw_addr,
					 MV88E6XXX_SMI_DATA, data);
	if (err)
		return err;

	err = mv88e6xxx_smi_direct_write(chip, chip->sw_addr,
					 MV88E6XXX_SMI_CMD,
					 MV88E6XXX_SMI_CMD_BUSY |
					 MV88E6XXX_SMI_CMD_MODE_22 |
					 MV88E6XXX_SMI_CMD_OP_22_WRITE |
					 (dev << 5) | reg);
	if (err)
		return err;

	return mv88e6xxx_smi_direct_wait(chip, chip->sw_addr,
					 MV88E6XXX_SMI_CMD, 15, 0);
}

static int mv88e6xxx_smi_indirect_init(struct mv88e6xxx_chip *chip)
{
	 
	return mv88e6xxx_smi_direct_wait(chip, chip->sw_addr,
					 MV88E6XXX_SMI_CMD, 15, 0);
}

static const struct mv88e6xxx_bus_ops mv88e6xxx_smi_indirect_ops = {
	.read = mv88e6xxx_smi_indirect_read,
	.write = mv88e6xxx_smi_indirect_write,
	.init = mv88e6xxx_smi_indirect_init,
};

int mv88e6xxx_smi_init(struct mv88e6xxx_chip *chip,
		       struct mii_bus *bus, int sw_addr)
{
	if (chip->info->dual_chip)
		chip->smi_ops = &mv88e6xxx_smi_dual_direct_ops;
	else if (sw_addr == 0)
		chip->smi_ops = &mv88e6xxx_smi_direct_ops;
	else if (chip->info->multi_chip)
		chip->smi_ops = &mv88e6xxx_smi_indirect_ops;
	else
		return -EINVAL;

	chip->bus = bus;
	chip->sw_addr = sw_addr;

	if (chip->smi_ops->init)
		return chip->smi_ops->init(chip);

	return 0;
}
