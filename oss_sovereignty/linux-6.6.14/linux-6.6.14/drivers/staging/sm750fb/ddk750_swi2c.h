#ifndef _SWI2C_H_
#define _SWI2C_H_
#define DEFAULT_I2C_SCL                     30
#define DEFAULT_I2C_SDA                     31
long sm750_sw_i2c_init(unsigned char clk_gpio, unsigned char data_gpio);
unsigned char sm750_sw_i2c_read_reg(unsigned char addr, unsigned char reg);
long sm750_sw_i2c_write_reg(unsigned char addr,
			    unsigned char reg,
			    unsigned char data);
#endif   
