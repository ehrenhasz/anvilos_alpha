#ifndef __ASM_ARCH_IIC_CORE_H
#define __ASM_ARCH_IIC_CORE_H __FILE__
static inline void s3c_i2c0_setname(char *name)
{
	s3c_device_i2c0.name = name;
}
static inline void s3c_i2c1_setname(char *name)
{
#ifdef CONFIG_S3C_DEV_I2C1
	s3c_device_i2c1.name = name;
#endif
}
#endif  
