

obj-$(CONFIG_BOSCH_BNO055) += bno055.o
obj-$(CONFIG_BOSCH_BNO055_SERIAL) += bno055_ser.o
bno055_ser-y := bno055_ser_core.o

CFLAGS_bno055_ser_trace.o := -I$(src)
bno055_ser-$(CONFIG_TRACING) += bno055_ser_trace.o

obj-$(CONFIG_BOSCH_BNO055_I2C) += bno055_i2c.o
