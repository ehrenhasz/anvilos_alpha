
 

#include "chip.h"
#include "global2.h"

 
static int mv88e6xxx_g2_scratch_read(struct mv88e6xxx_chip *chip, int reg,
				     u8 *data)
{
	u16 value;
	int err;

	err = mv88e6xxx_g2_write(chip, MV88E6XXX_G2_SCRATCH_MISC_MISC,
				 reg << 8);
	if (err)
		return err;

	err = mv88e6xxx_g2_read(chip, MV88E6XXX_G2_SCRATCH_MISC_MISC, &value);
	if (err)
		return err;

	*data = (value & MV88E6XXX_G2_SCRATCH_MISC_DATA_MASK);

	return 0;
}

static int mv88e6xxx_g2_scratch_write(struct mv88e6xxx_chip *chip, int reg,
				      u8 data)
{
	u16 value = (reg << 8) | data;

	return mv88e6xxx_g2_write(chip, MV88E6XXX_G2_SCRATCH_MISC_MISC,
				  MV88E6XXX_G2_SCRATCH_MISC_UPDATE | value);
}

 
static int mv88e6xxx_g2_scratch_get_bit(struct mv88e6xxx_chip *chip,
					int base_reg, unsigned int offset,
					int *set)
{
	int reg = base_reg + (offset / 8);
	u8 mask = (1 << (offset & 0x7));
	u8 val;
	int err;

	err = mv88e6xxx_g2_scratch_read(chip, reg, &val);
	if (err)
		return err;

	*set = !!(mask & val);

	return 0;
}

 
static int mv88e6xxx_g2_scratch_set_bit(struct mv88e6xxx_chip *chip,
					int base_reg, unsigned int offset,
					int set)
{
	int reg = base_reg + (offset / 8);
	u8 mask = (1 << (offset & 0x7));
	u8 val;
	int err;

	err = mv88e6xxx_g2_scratch_read(chip, reg, &val);
	if (err)
		return err;

	if (set)
		val |= mask;
	else
		val &= ~mask;

	return mv88e6xxx_g2_scratch_write(chip, reg, val);
}

 
static int mv88e6352_g2_scratch_gpio_get_data(struct mv88e6xxx_chip *chip,
					      unsigned int pin)
{
	int val = 0;
	int err;

	err = mv88e6xxx_g2_scratch_get_bit(chip,
					   MV88E6352_G2_SCRATCH_GPIO_DATA0,
					   pin, &val);
	if (err)
		return err;

	return val;
}

 
static int mv88e6352_g2_scratch_gpio_set_data(struct mv88e6xxx_chip *chip,
					      unsigned int pin, int value)
{
	u8 mask = (1 << (pin & 0x7));
	int offset = (pin / 8);
	int reg;

	reg = MV88E6352_G2_SCRATCH_GPIO_DATA0 + offset;

	if (value)
		chip->gpio_data[offset] |= mask;
	else
		chip->gpio_data[offset] &= ~mask;

	return mv88e6xxx_g2_scratch_write(chip, reg, chip->gpio_data[offset]);
}

 
static int mv88e6352_g2_scratch_gpio_get_dir(struct mv88e6xxx_chip *chip,
					     unsigned int pin)
{
	int val = 0;
	int err;

	err = mv88e6xxx_g2_scratch_get_bit(chip,
					   MV88E6352_G2_SCRATCH_GPIO_DIR0,
					   pin, &val);
	if (err)
		return err;

	return val;
}

 
static int mv88e6352_g2_scratch_gpio_set_dir(struct mv88e6xxx_chip *chip,
					     unsigned int pin, bool input)
{
	int value = (input ? MV88E6352_G2_SCRATCH_GPIO_DIR_IN :
			     MV88E6352_G2_SCRATCH_GPIO_DIR_OUT);

	return mv88e6xxx_g2_scratch_set_bit(chip,
					    MV88E6352_G2_SCRATCH_GPIO_DIR0,
					    pin, value);
}

 
static int mv88e6352_g2_scratch_gpio_get_pctl(struct mv88e6xxx_chip *chip,
					      unsigned int pin, int *func)
{
	int reg = MV88E6352_G2_SCRATCH_GPIO_PCTL0 + (pin / 2);
	int offset = (pin & 0x1) ? 4 : 0;
	u8 mask = (0x7 << offset);
	int err;
	u8 val;

	err = mv88e6xxx_g2_scratch_read(chip, reg, &val);
	if (err)
		return err;

	*func = (val & mask) >> offset;

	return 0;
}

 
static int mv88e6352_g2_scratch_gpio_set_pctl(struct mv88e6xxx_chip *chip,
					      unsigned int pin, int func)
{
	int reg = MV88E6352_G2_SCRATCH_GPIO_PCTL0 + (pin / 2);
	int offset = (pin & 0x1) ? 4 : 0;
	u8 mask = (0x7 << offset);
	int err;
	u8 val;

	err = mv88e6xxx_g2_scratch_read(chip, reg, &val);
	if (err)
		return err;

	val = (val & ~mask) | ((func & mask) << offset);

	return mv88e6xxx_g2_scratch_write(chip, reg, val);
}

const struct mv88e6xxx_gpio_ops mv88e6352_gpio_ops = {
	.get_data = mv88e6352_g2_scratch_gpio_get_data,
	.set_data = mv88e6352_g2_scratch_gpio_set_data,
	.get_dir = mv88e6352_g2_scratch_gpio_get_dir,
	.set_dir = mv88e6352_g2_scratch_gpio_set_dir,
	.get_pctl = mv88e6352_g2_scratch_gpio_get_pctl,
	.set_pctl = mv88e6352_g2_scratch_gpio_set_pctl,
};

 
int mv88e6xxx_g2_scratch_gpio_set_smi(struct mv88e6xxx_chip *chip,
				      bool external)
{
	int misc_cfg = MV88E6352_G2_SCRATCH_MISC_CFG;
	int config_data1 = MV88E6352_G2_SCRATCH_CONFIG_DATA1;
	int config_data2 = MV88E6352_G2_SCRATCH_CONFIG_DATA2;
	bool no_cpu;
	u8 p0_mode;
	int err;
	u8 val;

	err = mv88e6xxx_g2_scratch_read(chip, config_data2, &val);
	if (err)
		return err;

	p0_mode = val & MV88E6352_G2_SCRATCH_CONFIG_DATA2_P0_MODE_MASK;

	if (p0_mode == 0x01 || p0_mode == 0x02)
		return -EBUSY;

	err = mv88e6xxx_g2_scratch_read(chip, config_data1, &val);
	if (err)
		return err;

	no_cpu = !!(val & MV88E6352_G2_SCRATCH_CONFIG_DATA1_NO_CPU);

	err = mv88e6xxx_g2_scratch_read(chip, misc_cfg, &val);
	if (err)
		return err;

	 
	if (!no_cpu)
		external = !external;

	if (external)
		val |= MV88E6352_G2_SCRATCH_MISC_CFG_NORMALSMI;
	else
		val &= ~MV88E6352_G2_SCRATCH_MISC_CFG_NORMALSMI;

	return mv88e6xxx_g2_scratch_write(chip, misc_cfg, val);
}

 
int mv88e6352_g2_scratch_port_has_serdes(struct mv88e6xxx_chip *chip, int port)
{
	u8 config3, p;
	int err;

	err = mv88e6xxx_g2_scratch_read(chip, MV88E6352_G2_SCRATCH_CONFIG_DATA3,
					&config3);
	if (err)
		return err;

	if (config3 & MV88E6352_G2_SCRATCH_CONFIG_DATA3_S_SEL)
		p = 5;
	else
		p = 4;

	return port == p;
}
